#include "amucore/supervisor.h"

#include "amucore/rcon.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <tlhelp32.h>

namespace amucore {

namespace {

// Minimal scope guard: runs `f` on scope exit unless it was dismissed. Used to
// make state that MUST be released (the `updating` flag, an in-flight worker
// slot) immune to a future early return or a throw.
template <class F>
class ScopeGuard {
 public:
  explicit ScopeGuard(F f) : f_(std::move(f)) {}
  ~ScopeGuard() {
    if (!armed_) return;
    try {
      f_();
    } catch (...) {
      // A destructor must never throw - that would call std::terminate.
    }
  }
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
  void dismiss() { armed_ = false; }

 private:
  F f_;
  bool armed_ = true;
};

// UTF-8 -> UTF-16 for Win32 wide APIs (same pattern as serverconfig.cpp).
std::wstring toWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

// Locale-invariant ASCII case-insensitive compare (exe names are plain ASCII).
bool equalsNoCaseW(const wchar_t* a, const wchar_t* b) {
  while (*a && *b) {
    wchar_t ca = *a, cb = *b;
    if (ca >= L'A' && ca <= L'Z') ca = ca - L'A' + L'a';
    if (cb >= L'A' && cb <= L'Z') cb = cb - L'A' + L'a';
    if (ca != cb) return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

// Lowercase (ASCII-invariant), '/' -> '\', collapse duplicate separators and
// strip a trailing one. Applied to BOTH sides of the compare so mixed input
// forms (UNC included) normalize consistently.
std::wstring normalizePathW(const std::wstring& p) {
  std::wstring out;
  out.reserve(p.size());
  for (wchar_t c : p) {
    if (c == L'/') c = L'\\';
    if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
    if (c == L'\\' && !out.empty() && out.back() == L'\\') continue;
    out.push_back(c);
  }
  while (!out.empty() && out.back() == L'\\') out.pop_back();
  return out;
}

// Current UTC time as FILETIME 100ns ticks.
int64_t nowTicks() {
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return static_cast<int64_t>(u.QuadPart);
}

int64_t ticksOf(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return static_cast<int64_t>(u.QuadPart);
}

const char* stopStatusName(StopStatus st) {
  switch (st) {
    case StopStatus::CleanExit: return "clean exit";
    case StopStatus::TerminatedAfterTimeout: return "terminated after timeout";
    case StopStatus::AlreadyStopped: return "already stopped";
    case StopStatus::NoHandle: return "no handle";
  }
  return "unknown";
}

const char* actualName(Actual a) {
  switch (a) {
    case Actual::Stopped: return "stopped";
    case Actual::Starting: return "starting";
    case Actual::Running: return "running";
    case Actual::Stopping: return "stopping";
    case Actual::Crashed: return "crashed";
    case Actual::FailedToStart: return "failed";
  }
  return "stopped";
}

// The blocking graceful-stop sequence (design section 3): saveworld -> confirm
// fresh save -> DoExit -> wait for clean exit -> TerminateProcess fallback.
// Free-standing on plain data so both doStop (worker thread) and stopForUpdate
// (inline on the orchestrator's thread) share it. `stopEvent` (may be null)
// aborts the long waits early on supervisor shutdown - on that path the process
// is left ALONE (AMU quitting must never kill game servers) and NoHandle is
// returned as the "stop did not complete" marker.
StopStatus runStopSequence(uint32_t pid, void* ownedProcess, const std::string& installRoot,
                           const std::string& map, const RconEndpoint& rcon,
                           const StopOptions& opt, void* stopEvent,
                           const std::function<void(const char*, const std::string&)>& note) {
  HANDLE owned = static_cast<HANDLE>(ownedProcess);
  HANDLE stopEv = static_cast<HANDLE>(stopEvent);

  if (owned) {
    if (WaitForSingleObject(owned, 0) == WAIT_OBJECT_0) return StopStatus::AlreadyStopped;
  } else if (pid == 0) {
    return StopStatus::AlreadyStopped;
  }

  const std::string host = rcon.host.empty() ? std::string("127.0.0.1") : rcon.host;

  // 1) saveworld - warn and continue on RCON failure (AutoIt behavior: a
  // misconfigured RCON must not wedge the stop).
  if (rcon.enabled && opt.sendSaveWorld) {
    RconClient rc;
    std::string resp;
    RconStatus st = rc.runCommand(host, rcon.port, rcon.password, "saveworld", resp);
    if (st != RconStatus::Ok)
      note("normal", "RCON saveworld failed (status " + std::to_string(static_cast<int>(st)) +
                         "), continuing with stop");
  }

  // 2) Confirm a fresh save: poll <map>.ark last-write vs now with FILETIME
  // math (the AutoIt string-date version had rollover bugs). Hard 2-min cap.
  if (opt.confirmFreshSave && !installRoot.empty() && !map.empty()) {
    const std::wstring savePath =
        toWide(installRoot + "\\ShooterGame\\Saved\\SavedArks\\" + map + ".ark");
    int waitedMs = 0;
    bool fresh = false;
    for (;;) {
      WIN32_FILE_ATTRIBUTE_DATA fad{};
      if (GetFileAttributesExW(savePath.c_str(), GetFileExInfoStandard, &fad)) {
        FILETIME nowFt;
        GetSystemTimeAsFileTime(&nowFt);
        if (isSaveFresh(ticksOf(nowFt), ticksOf(fad.ftLastWriteTime), kSaveFreshWindowSec)) {
          fresh = true;
          break;
        }
      }
      if (waitedMs >= kSaveConfirmTimeoutMs) break;
      if (stopEv) {
        if (WaitForSingleObject(stopEv, kSaveConfirmPollMs) == WAIT_OBJECT_0)
          return StopStatus::NoHandle;  // shutdown abort: leave the server alone
      } else {
        Sleep(kSaveConfirmPollMs);
      }
      waitedMs += kSaveConfirmPollMs;
    }
    if (!fresh)
      note("normal",
           "Timed out waiting for a fresh save (" + map + ".ark), continuing with stop");
  }

  // 3) DoExit - best effort; on RCON failure fall through to wait + terminate.
  if (rcon.enabled && opt.sendDoExit) {
    RconClient rc;
    std::string resp;
    RconStatus st = rc.runCommand(host, rcon.port, rcon.password, "DoExit", resp);
    if (st != RconStatus::Ok)
      note("normal", "RCON DoExit failed (status " + std::to_string(static_cast<int>(st)) +
                         "), falling back to terminate");
  }

  // 4) Wait for a clean exit; terminate on timeout. Adopted-without-handle
  // servers get a fresh handle from the pid here.
  HANDLE waitH = owned;
  bool openedHere = false;
  if (!waitH) {
    waitH = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
    if (waitH) {
      openedHere = true;
    } else {
      HANDLE probe = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
      if (!probe) return StopStatus::CleanExit;  // already gone
      DWORD code = 0;
      const bool active = GetExitCodeProcess(probe, &code) && code == STILL_ACTIVE;
      CloseHandle(probe);
      return active ? StopStatus::NoHandle : StopStatus::CleanExit;
    }
  }

  StopStatus result;
  HANDLE hs[2] = {waitH, stopEv};
  const DWORD nWait = stopEv ? 2u : 1u;
  const DWORD w = WaitForMultipleObjects(nWait, hs, FALSE, static_cast<DWORD>(kGracefulExitMs));
  if (w == WAIT_OBJECT_0) {
    result = StopStatus::CleanExit;
  } else if (stopEv && w == WAIT_OBJECT_0 + 1) {
    // Supervisor shutdown aborted the stop - never terminate on this path.
    result = (WaitForSingleObject(waitH, 0) == WAIT_OBJECT_0) ? StopStatus::CleanExit
                                                              : StopStatus::NoHandle;
  } else if (w == WAIT_TIMEOUT) {
    TerminateProcess(waitH, 0);
    // Report "still running" when the kill did not take (TerminateProcess
    // denied, or the process outlives the reap wait): claiming a clean stop
    // would let the watcher re-adopt a live server and undo the user's Stop.
    result = (WaitForSingleObject(waitH, static_cast<DWORD>(kKillWaitMs)) == WAIT_OBJECT_0)
                 ? StopStatus::TerminatedAfterTimeout
                 : StopStatus::NoHandle;
  } else {
    // WAIT_FAILED - the wait itself is broken (a handle closed under us, a
    // handle without SYNCHRONIZE). NEVER terminate on a wait we do not
    // understand: AMU must not kill game servers.
    result = StopStatus::NoHandle;
  }
  if (openedHere) CloseHandle(waitH);
  return result;
}

}  // namespace

// --- pure functions ----------------------------------------------------------

SupAction decide(const DecideInput& in, const RestartPolicy& pol) {
  // An orchestrator update owns the server's lifecycle: never interfere.
  if (in.updating) return SupAction::None;

  if (in.desired == Desired::Running) {
    switch (in.actual) {
      case Actual::Stopped:
        // alive here means an adoption is pending (the watcher's sweep handles
        // it before decide on the same tick) - do not double-start.
        return in.processAlive ? SupAction::None : SupAction::Start;
      case Actual::Crashed:
        return (!in.processAlive && in.backoffElapsed) ? SupAction::Start : SupAction::None;
      case Actual::Running:
        if (in.processAlive) {
          return (in.uptimeSec >= pol.healthyResetSec && in.failures > 0)
                     ? SupAction::ResetFailures
                     : SupAction::None;
        }
        // Crash: desired Running + previously Running + process gone.
        return (in.failures + 1 >= pol.maxRetries) ? SupAction::GiveUp
                                                   : SupAction::ScheduleRestart;
      case Actual::Starting:
      case Actual::Stopping:
        return SupAction::None;  // a worker is in flight
      case Actual::FailedToStart:
        return SupAction::None;  // await an explicit user re-Start
    }
    return SupAction::None;
  }

  // desired == Stopped
  if (in.actual == Actual::Running) {
    if (!in.processAlive) return SupAction::MarkStopped;
    // Still alive: stop it - unless a previous stop failed and its retry damper
    // has not expired yet (each attempt re-runs saveworld + the fresh-save wait
    // + DoExit, so retrying every tick would hammer the server with RCON).
    return in.stopRetryElapsed ? SupAction::Stop : SupAction::None;
  }
  return SupAction::None;
}

bool stopLeftProcessRunning(StopStatus st) { return st == StopStatus::NoHandle; }

int backoffMs(int failures, const RestartPolicy& pol) {
  if (pol.baseBackoffMs <= 0) return 0;
  if (failures < 0) failures = 0;
  // Cap the exponent so the 64-bit shift can never overflow (2^31 * 2^30 < 2^63).
  const int exp = failures > 30 ? 30 : failures;
  const long long v = static_cast<long long>(pol.baseBackoffMs) << exp;
  if (v >= static_cast<long long>(pol.maxBackoffMs)) return pol.maxBackoffMs;
  return static_cast<int>(v);
}

bool imageMatchesInstall(const std::wstring& imagePath, const std::string& installRoot) {
  if (imagePath.empty() || installRoot.empty()) return false;
  const std::wstring img = normalizePathW(imagePath);
  const size_t slash = img.find_last_of(L'\\');
  if (slash == std::wstring::npos || slash == 0) return false;
  const std::wstring imageDir = img.substr(0, slash);
  const std::wstring root = normalizePathW(toWide(installRoot));
  if (root.empty()) return false;
  // Compare the image's parent DIRECTORY (not the exe name) so ASE and ASA -
  // both living in ...\ShooterGame\Binaries\Win64 - match one server row.
  return imageDir == root + L"\\shootergame\\binaries\\win64";
}

int64_t fileAgeSeconds(int64_t nowTicks, int64_t lastWriteTicks) {
  return (nowTicks - lastWriteTicks) / 10'000'000LL;
}

bool isSaveFresh(int64_t nowTicks, int64_t lastWriteTicks, int windowSec) {
  const int64_t age = fileAgeSeconds(nowTicks, lastWriteTicks);
  return age >= 0 && age <= windowSec;
}

// --- detection ---------------------------------------------------------------

std::vector<DetectedProc> detectAllArkProcesses() {
  std::vector<DetectedProc> out;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return out;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      // Cheap name filter BEFORE OpenProcess.
      ArkGame game;
      if (equalsNoCaseW(pe.szExeFile, L"ShooterGameServer.exe")) {
        game = ArkGame::ASE;
      } else if (equalsNoCaseW(pe.szExeFile, L"ArkAscendedServer.exe")) {
        game = ArkGame::ASA;
      } else {
        continue;
      }
      HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
      if (!h) continue;  // access denied / pid died - skip
      std::wstring buf(32768, L'\0');
      DWORD len = static_cast<DWORD>(buf.size());
      if (QueryFullProcessImageNameW(h, 0, buf.data(), &len)) {
        DetectedProc p;
        p.pid = pe.th32ProcessID;
        p.game = game;
        p.imagePath.assign(buf.data(), len);
        out.push_back(std::move(p));
      }
      CloseHandle(h);
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return out;
}

uint32_t detectPid(const std::string& installRoot) {
  for (const auto& p : detectAllArkProcesses())
    if (imageMatchesInstall(p.imagePath, installRoot)) return p.pid;
  return 0;
}

// --- Supervisor ----------------------------------------------------------------

Supervisor::~Supervisor() { shutdown(); }

void Supervisor::start(LaunchProvider launch, RconProvider rcon, LogSink logSink) {
  std::lock_guard<std::mutex> lk(mu_);
  if (running_) return;
  launchProvider_ = std::move(launch);
  rconProvider_ = std::move(rcon);
  logSink_ = std::move(logSink);
  wakeEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);  // auto-reset
  stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);   // manual-reset
  if (!wakeEvent_ || !stopEvent_) {
    if (wakeEvent_) CloseHandle(static_cast<HANDLE>(wakeEvent_));
    if (stopEvent_) CloseHandle(static_cast<HANDLE>(stopEvent_));
    wakeEvent_ = stopEvent_ = nullptr;
    return;
  }
  running_ = true;
  watcher_ = std::thread(&Supervisor::watcherLoop, this);
}

void Supervisor::shutdown() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!running_) return;
    // Clearing running_ under mu_ closes the door: beginWork is only ever
    // called with running_ == true, so no NEW in-flight slot can appear after
    // this point and the drain below cannot miss a worker.
    running_ = false;
    SetEvent(static_cast<HANDLE>(stopEvent_));
  }
  if (watcher_.joinable()) watcher_.join();
  // Wait for in-flight workers to drain (stopEvent_ aborts their long waits
  // quickly) so no worker still uses a handle we are about to close. The
  // COUNTER is the authority, not a scan over servers_: an entry can be
  // untracked while its worker still runs, and stopForUpdate holds a slot
  // without owning a thread at all - both are invisible to such a scan, and
  // closing stopEvent_ under a running stop sequence would make its wait fail
  // and fall into the TerminateProcess branch (killing a game server).
  while (inFlight_.load(std::memory_order_acquire) != 0) Sleep(50);
  {
    std::lock_guard<std::mutex> lk(mu_);
    // Close owned process handles only - the game servers keep running.
    for (auto& s : servers_) {
      if (s.hProcess) {
        CloseHandle(static_cast<HANDLE>(s.hProcess));
        s.hProcess = nullptr;
      }
    }
    if (wakeEvent_) {
      CloseHandle(static_cast<HANDLE>(wakeEvent_));
      wakeEvent_ = nullptr;
    }
    if (stopEvent_) {
      CloseHandle(static_cast<HANDLE>(stopEvent_));
      stopEvent_ = nullptr;
    }
  }
}

void Supervisor::track(int64_t serverId, const std::string& installRoot, const std::string& map,
                       ArkGame game, bool autoRestart) {
  std::lock_guard<std::mutex> lk(mu_);
  Managed* s = nullptr;
  for (auto& m : servers_) {
    if (m.serverId != serverId) continue;
    // Deliberately NOT find(): this also revives an entry whose erase untrack()
    // deferred to a still-running worker. Reviving keeps ONE entry per server,
    // so a re-track can never produce a second worker for the same server while
    // the first is still inside its stop sequence.
    m.untracked = false;
    s = &m;
    break;
  }
  if (!s) {
    Managed m;
    m.serverId = serverId;
    servers_.push_back(std::move(m));
    s = &servers_.back();
  }
  s->installRoot = installRoot;
  s->map = map;
  s->game = game;
  s->autoRestart = autoRestart;
  if (running_) SetEvent(static_cast<HANDLE>(wakeEvent_));
}

void Supervisor::untrack(int64_t serverId) {
  std::lock_guard<std::mutex> lk(mu_);
  for (auto it = servers_.begin(); it != servers_.end(); ++it) {
    if (it->serverId != serverId || it->untracked) continue;
    if (it->busy) {
      // A worker (or stopForUpdate) owns this entry and its handle. Erasing it
      // here would hide that worker from shutdown()'s drain - it would keep
      // running against a closed stopEvent_ and a destroyed logSink_ - and a
      // following track() would let a SECOND worker start for the same server.
      // Mark it instead; endWork() closes the handle and erases it.
      it->untracked = true;
      it->desired = Desired::Stopped;
      return;
    }
    if (it->hProcess) CloseHandle(static_cast<HANDLE>(it->hProcess));
    servers_.erase(it);
    return;
  }
}

void Supervisor::requestStart(int64_t serverId) {
  std::lock_guard<std::mutex> lk(mu_);
  Managed* s = find(serverId);
  if (!s) return;
  s->desired = Desired::Running;
  // An explicit user Start resets the failure/backoff state (design section 4).
  s->failures = 0;
  s->nextRestartAtTicks = 0;
  s->nextStopRetryAtTicks = 0;
  if (s->actual == Actual::FailedToStart || s->actual == Actual::Crashed)
    s->actual = Actual::Stopped;
  if (running_) SetEvent(static_cast<HANDLE>(wakeEvent_));
}

void Supervisor::requestStop(int64_t serverId) {
  std::lock_guard<std::mutex> lk(mu_);
  Managed* s = find(serverId);
  if (!s) return;
  s->desired = Desired::Stopped;
  // An explicit user Stop always retries immediately, even right after a stop
  // that failed and armed the retry damper.
  s->nextStopRetryAtTicks = 0;
  if (running_) SetEvent(static_cast<HANDLE>(wakeEvent_));
}

void Supervisor::requestRestart(int64_t serverId) {
  uint64_t epoch = 0;
  {
    std::lock_guard<std::mutex> lk(mu_);
    Managed* s = find(serverId);
    if (!s) return;
    if (!running_ || s->actual != Actual::Running || s->busy || s->updating) {
      // Not (cleanly) running, or no watcher to dispatch on: behave like
      // requestStart. If a worker is in flight the desired flip makes the
      // watcher start it after it finishes.
      s->desired = Desired::Running;
      s->failures = 0;
      s->nextRestartAtTicks = 0;
      s->nextStopRetryAtTicks = 0;
      if (s->actual == Actual::FailedToStart || s->actual == Actual::Crashed)
        s->actual = Actual::Stopped;
      if (running_) SetEvent(static_cast<HANDLE>(wakeEvent_));
      return;
    }
    s->desired = Desired::Running;
    s->actual = Actual::Stopping;
    s->detail = "restarting";
    epoch = beginWork(*s);  // last: claims the slot spawnWorker/endWork releases
  }
  if (spawnWorker(serverId, epoch, Work::Restart)) return;
  // The thread could not be created (spawnWorker already released busy and the
  // in-flight slot): the server was never touched, so put it back to Running.
  std::lock_guard<std::mutex> lk(mu_);
  Managed* s = findEpoch(serverId, epoch);
  if (!s) return;
  s->actual = Actual::Running;
  s->detail = "restart failed: could not create the worker thread";
}

StopStatus Supervisor::stopForUpdate(int64_t serverId, const StopOptions& opt, bool* wasRunning) {
  if (wasRunning) *wasRunning = false;

  // `updating` parks the watcher for this server completely (decide() returns
  // None for every input) and ONLY startAfterUpdate clears it again - which the
  // orchestrator calls only when wasRunning came back true. Every path that
  // does not hand the server over to startAfterUpdate must therefore clear the
  // flag itself, or the server stays unmanaged (no crash detection, no restart,
  // a Start button that silently does nothing) until AMU restarts. Hence a
  // scope guard: it makes future early returns (and throws) safe by
  // construction. It is declared BEFORE any lock so it runs with mu_ released,
  // and it owns BOTH releases because their order matters - endWork must be the
  // last thing that touches *this (shutdown() may be waiting on exactly that
  // slot before the Supervisor is destroyed).
  bool flagged = false;     // we set `updating`
  bool handedOver = false;  // ...and startAfterUpdate will clear it
  uint64_t epoch = 0;       // non-zero once an in-flight slot is claimed
  ScopeGuard guard([this, serverId, &flagged, &handedOver, &epoch] {
    if (flagged && !handedOver) {
      std::lock_guard<std::mutex> lk(mu_);
      Managed* s = find(serverId);
      if (s) {
        s->updating = false;
        // The server was not running (or we could not take it over) and nobody
        // will start it after the update: park it as Stopped. Unlike
        // `updating`, that still allows adoption, crash bookkeeping and an
        // explicit user Start.
        s->desired = Desired::Stopped;
      }
    }
    if (epoch != 0) endWork(serverId, epoch);
  });

  uint32_t pid = 0;
  void* h = nullptr;
  void* stopEv = nullptr;
  std::string root, map;
  {
    std::unique_lock<std::mutex> lk(mu_);
    Managed* s = find(serverId);
    // Not tracked, or no watcher running: never claim an in-flight slot outside
    // running_ - shutdown() drains that counter before it closes stopEvent_.
    if (!s || !running_) return StopStatus::AlreadyStopped;
    // Flag first: from here the watcher takes no action for this server.
    s->updating = true;
    flagged = true;
    s->desired = Desired::Stopped;
    // Wait out any in-flight start/stop worker before taking over the handle.
    while (s->busy) {
      lk.unlock();
      Sleep(50);
      lk.lock();
      s = find(serverId);
      if (!s || !running_) return StopStatus::AlreadyStopped;
    }
    const bool wr = s->actual == Actual::Running || s->actual == Actual::Starting;
    if (!wr || (s->pid == 0 && !s->hProcess)) return StopStatus::AlreadyStopped;
    if (wasRunning) *wasRunning = true;
    s->actual = Actual::Stopping;
    s->detail = "stopping for update";
    pid = s->pid;
    h = s->hProcess;
    root = s->installRoot;
    map = s->map;
    // Read under the lock: shutdown() nulls stopEvent_, but only after draining
    // the slot claimed right below, so this value stays valid for our sequence.
    stopEv = stopEvent_;
    epoch = beginWork(*s);  // last, and the guard above now owns the release
  }

  RconEndpoint ep;
  if (rconProvider_) ep = rconProvider_(serverId);  // reads ini - never under the lock
  const StopStatus st = runStopSequence(
      pid, h, root, map, ep, opt, stopEv,
      [this, serverId](const char* type, const std::string& msg) { log(serverId, type, msg); });
  const bool stillRunning = stopLeftProcessRunning(st);

  {
    std::lock_guard<std::mutex> lk(mu_);
    Managed* s = findEpoch(serverId, epoch);
    if (!s) {
      if (h) CloseHandle(static_cast<HANDLE>(h));  // untracked while we were stopping
    } else if (stillRunning) {
      // Could not take the process down: keep pid + handle + Running so the
      // watcher keeps seeing the truth instead of re-adopting it later.
      s->actual = Actual::Running;
      s->detail = std::string("could not stop for update (") + stopStatusName(st) + ")";
    } else {
      if (s->hProcess) {
        CloseHandle(static_cast<HANDLE>(s->hProcess));
        s->hProcess = nullptr;
      }
      s->pid = 0;
      s->actual = Actual::Stopped;
      s->detail = std::string("stopped for update (") + stopStatusName(st) + ")";
    }
  }
  // wasRunning is true here, so the orchestrator calls startAfterUpdate - it
  // owns `updating` (and the desired state) from this point on.
  handedOver = true;
  log(serverId, "normal",
      (stillRunning ? "Could not stop server " : "Stopped server ") + std::to_string(serverId) +
          " for update (" + stopStatusName(st) + ")");
  return st;
}

void Supervisor::startAfterUpdate(int64_t serverId) {
  std::lock_guard<std::mutex> lk(mu_);
  Managed* s = find(serverId);
  if (!s) return;
  s->updating = false;
  s->desired = Desired::Running;
  s->failures = 0;
  s->nextRestartAtTicks = 0;
  s->nextStopRetryAtTicks = 0;
  if (s->actual == Actual::FailedToStart || s->actual == Actual::Crashed)
    s->actual = Actual::Stopped;
  if (running_) SetEvent(static_cast<HANDLE>(wakeEvent_));
}

ServerStatus Supervisor::status(int64_t serverId) {
  for (auto& st : statusAll())
    if (st.serverId == serverId) return st;
  ServerStatus st;
  st.serverId = serverId;
  st.state = "stopped";
  st.desired = "stopped";
  return st;
}

std::vector<ServerStatus> Supervisor::statusAll() {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<ServerStatus> out;
  out.reserve(servers_.size());
  for (const auto& s : servers_) {
    if (s.untracked) continue;  // erase deferred to a worker: already gone to the UI
    ServerStatus st;
    st.serverId = s.serverId;
    st.running = s.actual == Actual::Running;
    st.pid = s.pid;
    st.state = actualName(s.actual);
    st.desired = s.desired == Desired::Running ? "running" : "stopped";
    st.failures = s.failures;
    st.detail = !s.detail.empty() ? s.detail
                                  : (s.pid != 0 ? "PID " + std::to_string(s.pid) : std::string());
    out.push_back(std::move(st));
  }
  return out;
}

// --- watcher -------------------------------------------------------------------

void Supervisor::watcherLoop() {
  for (;;) {
    bool done = false;
    try {
      done = watcherTick();
    } catch (...) {
      // An exception escaping a thread entry function calls std::terminate, and
      // one bad tick (a throwing log sink, bad_alloc) must not take the watcher
      // - and with it every server's crash detection - down. Pause briefly so a
      // permanently failing tick cannot spin.
      Sleep(50);
    }
    if (done) break;
  }
}

bool Supervisor::watcherTick() {
  // Build the wait set: [stopEvent_, wakeEvent_, owned process handles...].
  // Process handles are DUPLICATED for the wait so a worker closing the
  // original under the lock can never invalidate our wait array mid-wait.
  std::vector<HANDLE> waits;
  std::vector<HANDLE> dups;
  // Reserved -> the push_backs below cannot throw (a throw would leak the dups).
  waits.reserve(static_cast<size_t>(MAXIMUM_WAIT_OBJECTS));
  dups.reserve(static_cast<size_t>(MAXIMUM_WAIT_OBJECTS));
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!running_) return true;
    waits.push_back(static_cast<HANDLE>(stopEvent_));
    waits.push_back(static_cast<HANDLE>(wakeEvent_));
    for (const auto& s : servers_) {
      if (waits.size() >= MAXIMUM_WAIT_OBJECTS) break;
      if (!s.hProcess) continue;
      HANDLE dup = nullptr;
      if (DuplicateHandle(GetCurrentProcess(), static_cast<HANDLE>(s.hProcess),
                          GetCurrentProcess(), &dup, SYNCHRONIZE, FALSE, 0)) {
        waits.push_back(dup);
        dups.push_back(dup);
      }
    }
  }
  const DWORD r = WaitForMultipleObjects(static_cast<DWORD>(waits.size()), waits.data(), FALSE,
                                         static_cast<DWORD>(kWatcherPollMs));
  for (HANDLE d : dups) CloseHandle(d);
  if (r == WAIT_OBJECT_0) return true;  // stopEvent_ -> shutdown

  // --- reconcile tick (wake, process exit, or poll timeout) ---
  const std::vector<DetectedProc> sweep = detectAllArkProcesses();  // outside the lock
  const int64_t now = nowTicks();

  struct PendingLog {
    int64_t id;
    const char* type;
    std::string msg;
  };
  struct Dispatch {
    int64_t id;
    uint64_t epoch;
    bool isStart;
  };
  std::vector<PendingLog> logs;
  std::vector<Dispatch> dispatches;

  // Every entry in `dispatches` holds an in-flight slot, and ONLY the dispatch
  // loop at the end of this function gives it back (via spawnWorker). If the
  // reconcile below throws (bad_alloc on a detail/log string), unwinding would
  // skip that loop and shutdown() would spin on the counter forever - so hand
  // the slots back here and leave the entries retryable. Dismissed the moment
  // the dispatch loop takes over.
  ScopeGuard slotGuard([this, &dispatches] {
    for (const auto& d : dispatches) {
      {
        std::lock_guard<std::mutex> lk(mu_);
        Managed* s = findEpoch(d.id, d.epoch);
        if (s) s->actual = d.isStart ? Actual::Stopped : Actual::Running;
      }
      endWork(d.id, d.epoch);
    }
  });

  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!running_) return true;
    // Reserved BEFORE any beginWork so the push_back that follows one cannot
    // throw and strand a busy entry holding an unreleased in-flight slot.
    dispatches.reserve(servers_.size());
    for (auto& s : servers_) {
      if (s.busy) continue;       // a worker owns this server's lifecycle right now
      if (s.untracked) continue;  // erase deferred to a worker (implies busy)

      // (a) Adoption: a tracked, stopped server that IS running out of its
      // installRoot (started outside AMU, or before AMU launched).
      if (s.actual == Actual::Stopped && !s.hProcess && !s.updating) {
        for (const auto& p : sweep) {
          if (!imageMatchesInstall(p.imagePath, s.installRoot)) continue;
          s.hProcess = OpenProcess(
              SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE,
              p.pid);  // may fail -> adopted without handle (pid-tracked)
          s.pid = p.pid;
          s.actual = Actual::Running;
          s.startedAtTicks = now;
          // Never auto-stop a server someone started on purpose: adopting
          // implies the operator wants it running. This can only be reached
          // when the server was CONFIRMED stopped - a stop that failed keeps
          // actual == Running, so it can never flip an explicit Stop back.
          if (s.desired == Desired::Stopped) s.desired = Desired::Running;
          s.detail = "adopted running process (PID " + std::to_string(p.pid) + ")";
          logs.push_back({s.serverId, "normal",
                          "Adopted running server " + std::to_string(s.serverId) + " (PID " +
                              std::to_string(p.pid) + ")"});
          break;
        }
      }

      // (b) Liveness: owned handle is authoritative; pid-only adoptees are
      // checked against this tick's snapshot.
      bool alive = false;
      if (s.hProcess) {
        alive = WaitForSingleObject(static_cast<HANDLE>(s.hProcess), 0) != WAIT_OBJECT_0;
      } else if (s.pid != 0) {
        for (const auto& p : sweep) {
          if (p.pid == s.pid && imageMatchesInstall(p.imagePath, s.installRoot)) {
            alive = true;
            break;
          }
        }
      }

      DecideInput di;
      di.desired = s.desired;
      di.actual = s.actual;
      di.processAlive = alive;
      di.updating = s.updating;
      di.failures = s.failures;
      di.uptimeSec = s.startedAtTicks > 0 ? (now - s.startedAtTicks) / 10'000'000LL : 0;
      di.backoffElapsed = now >= s.nextRestartAtTicks;
      di.stopRetryElapsed = now >= s.nextStopRetryAtTicks;
      SupAction action = decide(di, policy_);

      // The launch-table toggle: a crash with auto-restart off is reported
      // and parked (desired -> Stopped) instead of restarted.
      if ((action == SupAction::ScheduleRestart || action == SupAction::GiveUp) &&
          !s.autoRestart) {
        DWORD ec = 0;
        if (s.hProcess) {
          GetExitCodeProcess(static_cast<HANDLE>(s.hProcess), &ec);
          CloseHandle(static_cast<HANDLE>(s.hProcess));
          s.hProcess = nullptr;
        }
        s.lastExitCode = ec;
        s.pid = 0;
        s.actual = Actual::Crashed;
        s.desired = Desired::Stopped;
        s.detail = "crashed (exit " + std::to_string(ec) + "), auto-restart disabled";
        logs.push_back({s.serverId, "normal",
                        "Server " + std::to_string(s.serverId) + " " + s.detail});
        continue;
      }

      switch (action) {
        case SupAction::None:
          break;
        case SupAction::Start:
          s.actual = Actual::Starting;
          s.detail = "start pending";
          // beginWork LAST: it claims the in-flight slot, and the reserved
          // push_back below is the only step left before the dispatch loop
          // hands that slot to spawnWorker (which always releases it).
          dispatches.push_back({s.serverId, beginWork(s), true});
          break;
        case SupAction::Stop:
          s.actual = Actual::Stopping;
          s.detail = "stop pending";
          dispatches.push_back({s.serverId, beginWork(s), false});
          break;
        case SupAction::MarkStopped: {
          DWORD ec = 0;
          if (s.hProcess) {
            GetExitCodeProcess(static_cast<HANDLE>(s.hProcess), &ec);
            CloseHandle(static_cast<HANDLE>(s.hProcess));
            s.hProcess = nullptr;
          }
          s.lastExitCode = ec;
          s.pid = 0;
          s.actual = Actual::Stopped;
          s.detail = "stopped";
          logs.push_back({s.serverId, "normal",
                          "Server " + std::to_string(s.serverId) + " stopped (exit code " +
                              std::to_string(ec) + ")"});
          break;
        }
        case SupAction::ScheduleRestart: {
          DWORD ec = 0;
          if (s.hProcess) {
            GetExitCodeProcess(static_cast<HANDLE>(s.hProcess), &ec);
            CloseHandle(static_cast<HANDLE>(s.hProcess));
            s.hProcess = nullptr;
          }
          s.lastExitCode = ec;
          s.pid = 0;
          const int waitMs = backoffMs(s.failures, policy_);
          s.failures += 1;
          s.actual = Actual::Crashed;
          s.nextRestartAtTicks = now + static_cast<int64_t>(waitMs) * 10'000;
          s.detail = "crashed (exit " + std::to_string(ec) + "), restart in " +
                     std::to_string(waitMs / 1000) + "s (attempt " +
                     std::to_string(s.failures) + "/" + std::to_string(policy_.maxRetries) + ")";
          logs.push_back({s.serverId, "normal",
                          "Server " + std::to_string(s.serverId) + " " + s.detail});
          break;
        }
        case SupAction::GiveUp: {
          DWORD ec = 0;
          if (s.hProcess) {
            GetExitCodeProcess(static_cast<HANDLE>(s.hProcess), &ec);
            CloseHandle(static_cast<HANDLE>(s.hProcess));
            s.hProcess = nullptr;
          }
          s.lastExitCode = ec;
          s.pid = 0;
          s.failures += 1;
          s.actual = Actual::FailedToStart;
          s.detail = "crashed (exit " + std::to_string(ec) + "); giving up after " +
                     std::to_string(s.failures) + " failures";
          logs.push_back({s.serverId, "normal",
                          "Server " + std::to_string(s.serverId) + " " + s.detail});
          break;
        }
        case SupAction::ResetFailures:
          s.failures = 0;
          logs.push_back({s.serverId, "debug",
                          "Server " + std::to_string(s.serverId) +
                              " healthy uptime reached, failure counter reset"});
          break;
      }
    }
  }

  // Worker dispatch and logging AFTER dropping the lock (the sink writes
  // sqlite; thread spawn need not serialize with state). Dispatch goes FIRST:
  // every entry in `dispatches` holds an in-flight slot, and a throwing log
  // sink must never be able to strand one.
  slotGuard.dismiss();
  for (const auto& d : dispatches) {
    if (spawnWorker(d.id, d.epoch, d.isStart ? Work::Start : Work::Stop)) continue;
    // The thread could not be created. spawnWorker already released busy and
    // the in-flight slot; leave the entry in a state the next tick retries
    // from, with a delay so a persistent failure cannot spin.
    try {
      {
        std::lock_guard<std::mutex> lk(mu_);
        Managed* s = findEpoch(d.id, d.epoch);
        if (!s) continue;
        if (d.isStart) {
          s->nextRestartAtTicks =
              now + static_cast<int64_t>(backoffMs(s->failures, policy_)) * 10'000;
          s->failures += 1;
          s->actual = Actual::Crashed;  // Crashed + backoff -> Start again later
        } else {
          s->nextStopRetryAtTicks = now + static_cast<int64_t>(kStopRetryMs) * 10'000;
          s->actual = Actual::Running;  // nothing was stopped; retry after the damper
        }
        s->detail = "could not create the worker thread";
      }
      log(d.id, "normal",
          "Server " + std::to_string(d.id) + ": could not create the worker thread, retrying");
    } catch (...) {
      // The slot is already released; the next tick reconciles the entry.
    }
  }
  for (const auto& l : logs) log(l.id, l.type, l.msg);
  return false;
}

// --- workers -------------------------------------------------------------------

void Supervisor::runWork(int64_t serverId, uint64_t epoch, Work kind) {
  if (kind == Work::Start) {
    doStart(serverId, epoch);
    return;
  }
  doStop(serverId, epoch, StopOptions{});
  if (kind != Work::Restart) return;
  bool proceed = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    Managed* s = findEpoch(serverId, epoch);
    // actual == Stopped is load-bearing: a stop that could NOT take the process
    // down leaves it Running, and starting on top of that would put a SECOND
    // server process on the same install.
    proceed = s && running_ && !s->untracked && !s->updating &&
              s->desired == Desired::Running && s->actual == Actual::Stopped;
    if (proceed) s->actual = Actual::Starting;
  }
  if (proceed) doStart(serverId, epoch);
}

void Supervisor::doStart(int64_t serverId, uint64_t epoch) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    // Untracked / shutting down between dispatch and here: do not launch a
    // server nobody is going to manage (we would have to abandon its handle).
    const Managed* s = findEpoch(serverId, epoch);
    if (!s || s->untracked || !running_) return;
  }
  if (!launchProvider_) return;

  // Build the command OUTSIDE the lock - the provider reads db + ini live.
  const LaunchCommand cmd = launchProvider_(serverId);
  if (cmd.exePath.empty()) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      Managed* s = findEpoch(serverId, epoch);
      if (s) {
        s->actual = Actual::FailedToStart;
        s->detail = "launch provider returned no exe path";
      }
    }
    log(serverId, "normal",
        "Failed to start server " + std::to_string(serverId) + ": no launch command");
    return;
  }

  const std::wstring exeW = toWide(cmd.exePath);
  const std::wstring dirW = toWide(cmd.workingDir);
  // CreateProcessW may WRITE to lpCommandLine - hand it a mutable copy.
  std::wstring cmdBuf = toWide(cmd.commandLine);
  if (cmdBuf.empty()) cmdBuf = L"\"" + exeW + L"\"";

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  const BOOL ok =
      CreateProcessW(exeW.c_str(), &cmdBuf[0], nullptr, nullptr, FALSE,
                     CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT, nullptr,
                     dirW.empty() ? nullptr : dirW.c_str(), &si, &pi);
  if (!ok) {
    const DWORD err = GetLastError();
    {
      std::lock_guard<std::mutex> lk(mu_);
      Managed* s = findEpoch(serverId, epoch);
      if (s) {
        s->actual = Actual::FailedToStart;
        s->lastExitCode = err;
        s->pid = 0;
        s->detail = "CreateProcess failed (error " + std::to_string(err) + ")";
      }
    }
    log(serverId, "normal",
        "Failed to start server " + std::to_string(serverId) + ": CreateProcessW error " +
            std::to_string(err));
    return;
  }
  CloseHandle(pi.hThread);

  bool adopted = false;  // the entry took ownership of pi.hProcess
  {
    std::lock_guard<std::mutex> lk(mu_);
    Managed* s = findEpoch(serverId, epoch);
    if (s && running_ && !s->untracked) {
      s->pid = pi.dwProcessId;
      s->hProcess = pi.hProcess;
      adopted = true;  // set before anything that could throw below
      s->actual = Actual::Starting;
      s->detail = "starting (PID " + std::to_string(pi.dwProcessId) + ")";
    }
  }
  if (!adopted) {
    // Untracked / supervisor shut down mid-start: keep the game server running
    // (never kill it), just drop our handle.
    CloseHandle(pi.hProcess);
    log(serverId, "normal",
        "Started server " + std::to_string(serverId) + " (PID " +
            std::to_string(pi.dwProcessId) + ") but it is no longer managed - handle released");
    return;
  }

  // Instant-exit detection window (a .bat swallowed these; we can observe them).
  const DWORD w = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(kStartupGraceMs));
  const int64_t now = nowTicks();
  if (w == WAIT_OBJECT_0) {
    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    bool gaveUp = false;
    int failures = 0;
    {
      std::lock_guard<std::mutex> lk(mu_);
      Managed* s = findEpoch(serverId, epoch);
      // Null the entry's reference BEFORE closing: the watcher duplicates
      // hProcess for its wait set without caring whether a worker is busy.
      if (s && s->hProcess == pi.hProcess) {
        s->hProcess = nullptr;
        s->pid = 0;
        s->lastExitCode = ec;
        const int prev = s->failures;
        s->failures = prev + 1;
        failures = s->failures;
        // An instant exit is usually a misconfig, not a transient crash:
        // FailedToStart after 2 tries instead of backing off up to maxRetries.
        if (s->failures >= 2) {
          gaveUp = true;
          s->actual = Actual::FailedToStart;
          s->detail =
              "exited during startup (exit " + std::to_string(ec) + "), giving up";
        } else {
          s->actual = Actual::Crashed;
          s->nextRestartAtTicks =
              now + static_cast<int64_t>(backoffMs(prev, policy_)) * 10'000;
          s->detail = "exited during startup (exit " + std::to_string(ec) + "), retrying";
        }
      }
    }
    // Ours on every path: the entry either handed it back just now, is gone, or
    // holds a different handle - closing it here is what keeps the process
    // handle from leaking for the rest of the app's lifetime.
    CloseHandle(pi.hProcess);
    log(serverId, "normal",
        "Server " + std::to_string(serverId) + " exited during startup (exit code " +
            std::to_string(ec) + (gaveUp ? "), giving up after " + std::to_string(failures) +
                                      " attempts"
                                : "), retry scheduled"));
  } else {
    bool kept = false;  // the entry still owns pi.hProcess
    {
      std::lock_guard<std::mutex> lk(mu_);
      Managed* s = findEpoch(serverId, epoch);
      if (s && s->hProcess == pi.hProcess) {
        kept = true;
        s->actual = Actual::Running;
        s->startedAtTicks = now;
        s->detail = "PID " + std::to_string(pi.dwProcessId);
      }
    }
    // Untracked (or the handle was replaced) while we waited out the startup
    // grace window: nobody owns pi.hProcess but us.
    if (!kept) CloseHandle(pi.hProcess);
    log(serverId, "normal",
        "Started server " + std::to_string(serverId) + " (PID " +
            std::to_string(pi.dwProcessId) + ")");
  }
}

void Supervisor::doStop(int64_t serverId, uint64_t epoch, const StopOptions& opt) {
  uint32_t pid = 0;
  void* h = nullptr;
  void* stopEv = nullptr;
  std::string root, map;
  {
    std::lock_guard<std::mutex> lk(mu_);
    Managed* s = findEpoch(serverId, epoch);
    if (!s) return;
    pid = s->pid;
    h = s->hProcess;
    root = s->installRoot;
    map = s->map;
    // Read under the lock: shutdown() nulls stopEvent_, but only AFTER draining
    // the in-flight slot this worker holds, so the value stays valid for the
    // whole sequence. Reading it unsynchronized could hand runStopSequence a
    // closed handle - its wait would fail and fall into TerminateProcess.
    stopEv = stopEvent_;
  }

  RconEndpoint ep;
  if (rconProvider_) ep = rconProvider_(serverId);  // reads ini - never under the lock

  StopStatus st;
  if (!h && pid == 0) {
    st = StopStatus::AlreadyStopped;
  } else {
    st = runStopSequence(
        pid, h, root, map, ep, opt, stopEv,
        [this, serverId](const char* type, const std::string& msg) { log(serverId, type, msg); });
  }
  const bool stillRunning = stopLeftProcessRunning(st);
  const int64_t now = nowTicks();

  {
    std::lock_guard<std::mutex> lk(mu_);
    Managed* s = findEpoch(serverId, epoch);
    if (!s) {
      if (h) CloseHandle(static_cast<HANDLE>(h));  // untracked while we were stopping
    } else if (stillRunning) {
      // The process is provably still alive (no rights to terminate it, or a
      // shutdown abort left it alone). Claiming Stopped here would be a lie the
      // next tick "repairs" by re-adopting the process and flipping desired
      // back to Running - silently undoing the user's Stop. Keep pid + handle +
      // Running, keep desired Stopped, and damp the retry.
      s->actual = Actual::Running;
      s->nextStopRetryAtTicks = now + static_cast<int64_t>(kStopRetryMs) * 10'000;
      s->detail = "stop failed (" + std::string(stopStatusName(st)) + "), PID " +
                  std::to_string(s->pid) + " still running";
    } else {
      if (s->hProcess) {
        CloseHandle(static_cast<HANDLE>(s->hProcess));
        s->hProcess = nullptr;
      }
      s->pid = 0;
      s->actual = Actual::Stopped;
      s->detail = std::string("stopped (") + stopStatusName(st) + ")";
    }
  }
  log(serverId, "normal",
      (stillRunning ? "Could not stop server " : "Stopped server ") + std::to_string(serverId) +
          " (" + stopStatusName(st) + ")");
}

// --- worker bookkeeping ----------------------------------------------------------

uint64_t Supervisor::beginWork(Managed& s) {
  s.busy = true;
  s.epoch = ++epochSeq_;  // unique per dispatch, monotonic, never reused
  // Claimed BEFORE the worker exists so shutdown() can never miss it. Released
  // by endWork - from the worker tail, or by spawnWorker when no thread was
  // created. Callers hold mu_ and have checked running_.
  inFlight_.fetch_add(1, std::memory_order_acq_rel);
  return s.epoch;
}

void Supervisor::endWork(int64_t serverId, uint64_t epoch) noexcept {
  try {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto it = servers_.begin(); it != servers_.end(); ++it) {
      if (it->serverId != serverId || it->epoch != epoch) continue;
      it->busy = false;
      if (it->untracked) {
        // untrack() deferred the erase to us because this worker was in flight.
        if (it->hProcess) CloseHandle(static_cast<HANDLE>(it->hProcess));
        servers_.erase(it);
      }
      break;
    }
    if (running_ && wakeEvent_) SetEvent(static_cast<HANDLE>(wakeEvent_));
  } catch (...) {
    // Releasing the slot below matters more than any of the above.
  }
  // MUST be the last touch of *this on this thread: shutdown() waits for this
  // counter to hit zero before it closes stopEvent_/wakeEvent_ and lets the
  // Supervisor (mu_, logSink_, servers_) be destroyed.
  inFlight_.fetch_sub(1, std::memory_order_acq_rel);
}

bool Supervisor::spawnWorker(int64_t serverId, uint64_t epoch, Work kind) noexcept {
  try {
    // Trivially copyable captures only - nothing here can throw before the
    // std::thread constructor, which is inside the try.
    std::thread([this, serverId, epoch, kind] {
      try {
        runWork(serverId, epoch, kind);
      } catch (...) {
        // An exception escaping a thread entry function calls std::terminate.
      }
      endWork(serverId, epoch);
    }).detach();
    return true;
  } catch (...) {
    // No thread: give the slot (and busy) back, or shutdown() would spin
    // forever and the server would stay unmanaged.
    endWork(serverId, epoch);
    return false;
  }
}

Supervisor::Managed* Supervisor::find(int64_t serverId) {
  for (auto& s : servers_)
    if (s.serverId == serverId && !s.untracked) return &s;
  return nullptr;
}

Supervisor::Managed* Supervisor::findEpoch(int64_t serverId, uint64_t epoch) {
  // Epochs are unique per dispatch, so this matches ONLY the entry the worker
  // was dispatched for - never a recreated one, and never one whose worker slot
  // was handed on. Untracked entries stay visible here: their own worker keeps
  // updating them until endWork erases them.
  for (auto& s : servers_)
    if (s.serverId == serverId && s.epoch == epoch) return &s;
  return nullptr;
}

void Supervisor::log(int64_t serverId, const char* type, const std::string& msg) noexcept {
  // Callers guarantee mu_ is NOT held here - the sink writes sqlite. A throwing
  // sink must never escape into a worker/watcher tail (that would terminate the
  // process, or strand a busy entry); a lost log line is the cheaper failure.
  if (!logSink_) return;
  try {
    logSink_(serverId, type, msg);
  } catch (...) {
  }
}

}  // namespace amucore
