#include <doctest/doctest.h>

#include <string>

#include "amucore/supervisor.h"

using namespace amucore;

namespace {

constexpr int64_t kTick = 10'000'000;  // 100ns FILETIME ticks per second

DecideInput mk(Desired d, Actual a, bool alive, int failures = 0, int64_t uptimeSec = 0,
               bool backoffElapsed = false, bool updating = false) {
  DecideInput in;
  in.desired = d;
  in.actual = a;
  in.processAlive = alive;
  in.updating = updating;
  in.failures = failures;
  in.uptimeSec = uptimeSec;
  in.backoffElapsed = backoffElapsed;
  return in;
}

}  // namespace

// --- decide: the full transition table ---------------------------------------

TEST_CASE("decide: updating suppresses every action") {
  const RestartPolicy pol;
  const Desired ds[] = {Desired::Stopped, Desired::Running};
  const Actual as[] = {Actual::Stopped,  Actual::Starting, Actual::Running,
                       Actual::Stopping, Actual::Crashed,  Actual::FailedToStart};
  for (Desired d : ds) {
    for (Actual a : as) {
      for (bool alive : {false, true}) {
        // Even with maxed failures, elapsed backoff and long uptime.
        CHECK(decide(mk(d, a, alive, 10, 100000, true, true), pol) == SupAction::None);
      }
    }
  }
  // Specifically: a stop of an alive server is also suppressed while updating.
  CHECK(decide(mk(Desired::Stopped, Actual::Running, true, 0, 0, false, true), pol) ==
        SupAction::None);
  // And a crash during the update window is NOT restarted (the race from the
  // design doc section 6).
  CHECK(decide(mk(Desired::Running, Actual::Running, false, 0, 0, false, true), pol) ==
        SupAction::None);
}

TEST_CASE("decide: desired Running starts a stopped server") {
  const RestartPolicy pol;
  CHECK(decide(mk(Desired::Running, Actual::Stopped, false), pol) == SupAction::Start);
  // alive while Stopped == adoption pending on this very tick -> no double start.
  CHECK(decide(mk(Desired::Running, Actual::Stopped, true), pol) == SupAction::None);
}

TEST_CASE("decide: crashed server restarts only after the backoff elapsed") {
  const RestartPolicy pol;
  CHECK(decide(mk(Desired::Running, Actual::Crashed, false, 2, 0, true), pol) ==
        SupAction::Start);
  CHECK(decide(mk(Desired::Running, Actual::Crashed, false, 2, 0, false), pol) ==
        SupAction::None);
  // A (strange) live process while Crashed: never start a second instance.
  CHECK(decide(mk(Desired::Running, Actual::Crashed, true, 2, 0, true), pol) ==
        SupAction::None);
}

TEST_CASE("decide: healthy running server - None or ResetFailures") {
  const RestartPolicy pol;  // healthyResetSec = 300
  // No failures -> nothing to reset, regardless of uptime.
  CHECK(decide(mk(Desired::Running, Actual::Running, true, 0, 10), pol) == SupAction::None);
  CHECK(decide(mk(Desired::Running, Actual::Running, true, 0, 100000), pol) == SupAction::None);
  // Failures pending but uptime below the reset threshold -> None.
  CHECK(decide(mk(Desired::Running, Actual::Running, true, 3, 299), pol) == SupAction::None);
  // Uptime at/over the threshold with failures -> ResetFailures.
  CHECK(decide(mk(Desired::Running, Actual::Running, true, 3, 300), pol) ==
        SupAction::ResetFailures);
  CHECK(decide(mk(Desired::Running, Actual::Running, true, 1, 301), pol) ==
        SupAction::ResetFailures);
}

TEST_CASE("decide: crash detection - ScheduleRestart until maxRetries, then GiveUp") {
  const RestartPolicy pol;  // maxRetries = 5
  // failures + 1 < maxRetries -> another restart attempt.
  CHECK(decide(mk(Desired::Running, Actual::Running, false, 0), pol) ==
        SupAction::ScheduleRestart);
  CHECK(decide(mk(Desired::Running, Actual::Running, false, 3), pol) ==
        SupAction::ScheduleRestart);
  // failures + 1 >= maxRetries -> give up.
  CHECK(decide(mk(Desired::Running, Actual::Running, false, 4), pol) == SupAction::GiveUp);
  CHECK(decide(mk(Desired::Running, Actual::Running, false, 10), pol) == SupAction::GiveUp);

  RestartPolicy strict;
  strict.maxRetries = 1;
  CHECK(decide(mk(Desired::Running, Actual::Running, false, 0), strict) == SupAction::GiveUp);
}

TEST_CASE("decide: workers in flight and terminal states -> None") {
  const RestartPolicy pol;
  for (bool alive : {false, true}) {
    CHECK(decide(mk(Desired::Running, Actual::Starting, alive), pol) == SupAction::None);
    CHECK(decide(mk(Desired::Running, Actual::Stopping, alive), pol) == SupAction::None);
    // FailedToStart awaits an explicit user re-Start.
    CHECK(decide(mk(Desired::Running, Actual::FailedToStart, alive, 5, 0, true), pol) ==
          SupAction::None);
  }
}

TEST_CASE("decide: desired Stopped") {
  const RestartPolicy pol;
  // Alive -> stop it.
  CHECK(decide(mk(Desired::Stopped, Actual::Running, true), pol) == SupAction::Stop);
  // Died while desired Stopped -> normal stop, NEVER a restart.
  CHECK(decide(mk(Desired::Stopped, Actual::Running, false), pol) == SupAction::MarkStopped);
  // Everything else is None.
  const Actual rest[] = {Actual::Stopped, Actual::Starting, Actual::Stopping, Actual::Crashed,
                         Actual::FailedToStart};
  for (Actual a : rest) {
    for (bool alive : {false, true}) {
      CHECK(decide(mk(Desired::Stopped, a, alive, 3, 1000, true), pol) == SupAction::None);
    }
  }
}

// --- decide: the stop-retry damper ----------------------------------------------

namespace {

// desired Stopped + actual Running, i.e. the "please stop this server" state.
DecideInput mkStop(bool alive, bool stopRetryElapsed) {
  DecideInput in;
  in.desired = Desired::Stopped;
  in.actual = Actual::Running;
  in.processAlive = alive;
  in.stopRetryElapsed = stopRetryElapsed;
  return in;
}

}  // namespace

TEST_CASE("decide: stopRetryElapsed defaults to true - a first Stop is immediate") {
  const RestartPolicy pol;
  DecideInput in;  // deliberately built WITHOUT touching stopRetryElapsed
  in.desired = Desired::Stopped;
  in.actual = Actual::Running;
  in.processAlive = true;
  CHECK(in.stopRetryElapsed);
  CHECK(decide(in, pol) == SupAction::Stop);
}

TEST_CASE("decide: a stop that failed does not silently self-revert") {
  const RestartPolicy pol;
  // AMU unelevated / the ARK server elevated: the stop sequence comes back
  // NoHandle, so doStop keeps actual == Running (the process IS running) with
  // desired == Stopped and arms the retry damper. From there:
  CHECK(decide(mkStop(true, false), pol) == SupAction::None);  // damped, no RCON storm
  CHECK(decide(mkStop(true, true), pol) == SupAction::Stop);   // one retry per damper
  // ...and when the process finally goes away the damper must NOT block the
  // bookkeeping, or the entry would sit in "running" forever.
  CHECK(decide(mkStop(false, false), pol) == SupAction::MarkStopped);
  CHECK(decide(mkStop(false, true), pol) == SupAction::MarkStopped);
  // Crucially: never Start. The old code booked Stopped while the process was
  // still alive, and the adoption sweep then flipped desired back to Running.
}

TEST_CASE("decide: the stop damper cannot suppress anything else") {
  const RestartPolicy pol;
  // desired Running is unaffected by the stop damper on every actual state.
  const Actual as[] = {Actual::Stopped,  Actual::Starting, Actual::Running,
                       Actual::Stopping, Actual::Crashed,  Actual::FailedToStart};
  for (Actual a : as) {
    for (bool alive : {false, true}) {
      DecideInput damped = mk(Desired::Running, a, alive, 0, 0, true);
      damped.stopRetryElapsed = false;
      DecideInput fresh = damped;
      fresh.stopRetryElapsed = true;
      CHECK(decide(damped, pol) == decide(fresh, pol));
    }
  }
  // And an elapsed damper still loses against an update in progress.
  DecideInput upd = mkStop(true, true);
  upd.updating = true;
  CHECK(decide(upd, pol) == SupAction::None);
}

// --- stopLeftProcessRunning -------------------------------------------------------

TEST_CASE("stopLeftProcessRunning: only NoHandle means the process survived") {
  // NoHandle is the "stop did not complete" marker - no rights to terminate,
  // a kill that did not take, or a supervisor-shutdown abort.
  CHECK(stopLeftProcessRunning(StopStatus::NoHandle));
  // Everything else confirms the process is gone.
  CHECK_FALSE(stopLeftProcessRunning(StopStatus::CleanExit));
  CHECK_FALSE(stopLeftProcessRunning(StopStatus::TerminatedAfterTimeout));
  CHECK_FALSE(stopLeftProcessRunning(StopStatus::AlreadyStopped));
}

// --- backoffMs ----------------------------------------------------------------

TEST_CASE("backoffMs doubles per failure and caps at maxBackoffMs") {
  const RestartPolicy pol;  // base 5000, max 300000
  CHECK(backoffMs(0, pol) == 5000);
  CHECK(backoffMs(1, pol) == 10000);
  CHECK(backoffMs(2, pol) == 20000);
  CHECK(backoffMs(3, pol) == 40000);
  CHECK(backoffMs(5, pol) == 160000);
  CHECK(backoffMs(6, pol) == 300000);  // 5000 * 64 = 320000 -> capped
  CHECK(backoffMs(7, pol) == 300000);
}

TEST_CASE("backoffMs clamps negative failures to zero") {
  const RestartPolicy pol;
  CHECK(backoffMs(-1, pol) == 5000);
  CHECK(backoffMs(-1000, pol) == 5000);
}

TEST_CASE("backoffMs survives overflow-sized failure counts") {
  const RestartPolicy pol;
  CHECK(backoffMs(31, pol) == 300000);
  CHECK(backoffMs(63, pol) == 300000);   // would overflow a naive 64-bit shift chain
  CHECK(backoffMs(1000, pol) == 300000);
  CHECK(backoffMs(2147483647, pol) == 300000);

  // Large base with the exponent capped: no UB, still capped by max.
  RestartPolicy big;
  big.baseBackoffMs = 2147483647;  // INT_MAX
  big.maxBackoffMs = 2147483647;
  CHECK(backoffMs(1000, big) == 2147483647);

  // Small base, huge cap: exponent is clamped to 30 -> 1 << 30.
  RestartPolicy tiny;
  tiny.baseBackoffMs = 1;
  tiny.maxBackoffMs = 2147483647;
  CHECK(backoffMs(30, tiny) == 1073741824);
  CHECK(backoffMs(62, tiny) == 1073741824);  // exponent capped, not overflowed
}

TEST_CASE("backoffMs with a non-positive base is zero") {
  RestartPolicy pol;
  pol.baseBackoffMs = 0;
  CHECK(backoffMs(0, pol) == 0);
  CHECK(backoffMs(10, pol) == 0);
}

// --- imageMatchesInstall --------------------------------------------------------

TEST_CASE("imageMatchesInstall: exact ASE and ASA paths") {
  CHECK(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:\\ARK\\srv1"));
  CHECK(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ArkAscendedServer.exe", "D:\\ARK\\srv1"));
  // Any exe in the binaries dir matches - the compare is on the DIRECTORY.
  CHECK(imageMatchesInstall(L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\Whatever.exe",
                            "D:\\ARK\\srv1"));
}

TEST_CASE("imageMatchesInstall: case-insensitive") {
  CHECK(imageMatchesInstall(
      L"d:\\ark\\SRV1\\shootergame\\BINARIES\\win64\\SHOOTERGAMESERVER.EXE", "D:\\Ark\\Srv1"));
}

TEST_CASE("imageMatchesInstall: mixed and duplicate separators") {
  CHECK(imageMatchesInstall(L"D:/ARK/srv1/ShooterGame/Binaries/Win64/ShooterGameServer.exe",
                            "D:\\ARK\\srv1"));
  CHECK(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:/ARK/srv1"));
  CHECK(imageMatchesInstall(
      L"D:\\\\ARK\\\\srv1\\ShooterGame\\Binaries\\\\Win64\\ShooterGameServer.exe",
      "D:\\ARK\\srv1"));
  CHECK(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe",
      "D:\\ARK//srv1"));
}

TEST_CASE("imageMatchesInstall: trailing separator on the install root") {
  CHECK(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:\\ARK\\srv1\\"));
  CHECK(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:\\ARK\\srv1/"));
}

TEST_CASE("imageMatchesInstall: wrong directory does not match") {
  // Exe directly in the install root (not the binaries dir).
  CHECK_FALSE(imageMatchesInstall(L"D:\\ARK\\srv1\\ShooterGameServer.exe", "D:\\ARK\\srv1"));
  // One level short / one level deep.
  CHECK_FALSE(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\ShooterGameServer.exe", "D:\\ARK\\srv1"));
  CHECK_FALSE(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\sub\\ShooterGameServer.exe",
      "D:\\ARK\\srv1"));
  // Entirely different install.
  CHECK_FALSE(imageMatchesInstall(
      L"E:\\other\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:\\ARK\\srv1"));
}

TEST_CASE("imageMatchesInstall: nested-similar prefixes stay distinct") {
  // srv1 vs srv10 - a prefix compare would get this wrong.
  CHECK_FALSE(imageMatchesInstall(
      L"D:\\ARK\\srv10\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:\\ARK\\srv1"));
  CHECK_FALSE(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:\\ARK\\srv10"));
  CHECK(imageMatchesInstall(
      L"D:\\ARK\\srv10\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "D:\\ARK\\srv10"));
}

TEST_CASE("imageMatchesInstall: degenerate inputs") {
  CHECK_FALSE(imageMatchesInstall(L"", "D:\\ARK\\srv1"));
  CHECK_FALSE(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", ""));
  // No separator at all in the image path.
  CHECK_FALSE(imageMatchesInstall(L"ShooterGameServer.exe", "D:\\ARK\\srv1"));
  // Root that normalizes to nothing.
  CHECK_FALSE(imageMatchesInstall(
      L"D:\\ARK\\srv1\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe", "\\"));
}

// --- fileAgeSeconds / isSaveFresh ------------------------------------------------

TEST_CASE("fileAgeSeconds: basic tick math") {
  CHECK(fileAgeSeconds(0, 0) == 0);
  CHECK(fileAgeSeconds(30 * kTick, 0) == 30);
  CHECK(fileAgeSeconds(kTick, 0) == 1);
  // Sub-second remainders truncate toward zero.
  CHECK(fileAgeSeconds(kTick / 2, 0) == 0);
  CHECK(fileAgeSeconds(kTick + kTick / 2, 0) == 1);
}

TEST_CASE("fileAgeSeconds: negative when the file is newer than 'now'") {
  CHECK(fileAgeSeconds(0, 2 * kTick) == -2);
  // -0.5s truncates to 0 (still counts as not-in-the-future for freshness).
  CHECK(fileAgeSeconds(0, kTick / 2) == 0);
  CHECK(fileAgeSeconds(0, kTick + kTick / 2) == -1);
}

TEST_CASE("fileAgeSeconds: correct across rollover-sized values") {
  // Realistic 2020s-era FILETIME magnitude (~1.337e17 ticks since 1601).
  const int64_t now = 133'700'000'000'000'000LL;
  // Minute, hour, day and month boundaries - the AutoIt string-subtraction bug class.
  CHECK(fileAgeSeconds(now, now - 59 * kTick) == 59);
  CHECK(fileAgeSeconds(now, now - 61 * kTick) == 61);
  CHECK(fileAgeSeconds(now, now - 3600 * kTick) == 3600);
  CHECK(fileAgeSeconds(now, now - 86400 * kTick) == 86400);
  CHECK(fileAgeSeconds(now, now - 30LL * 86400 * kTick) == 2592000);
  // And huge diffs do not overflow.
  CHECK(fileAgeSeconds(now, 0) == now / kTick);
}

TEST_CASE("isSaveFresh: window boundaries") {
  const int64_t now = 133'700'000'000'000'000LL;
  CHECK(isSaveFresh(now, now));                    // age 0
  CHECK(isSaveFresh(now, now - 60 * kTick));       // age 60 == default window
  CHECK_FALSE(isSaveFresh(now, now - 61 * kTick));  // just outside
  CHECK_FALSE(isSaveFresh(now, now - 3600 * kTick));
}

TEST_CASE("isSaveFresh: future timestamps are not fresh") {
  const int64_t now = 133'700'000'000'000'000LL;
  // A file written >= 1s in the future (clock skew) is rejected...
  CHECK_FALSE(isSaveFresh(now, now + 2 * kTick));
  // ...but sub-second skew truncates to age 0 and passes.
  CHECK(isSaveFresh(now, now + kTick / 2));
}

TEST_CASE("isSaveFresh: custom window") {
  const int64_t now = 133'700'000'000'000'000LL;
  CHECK(isSaveFresh(now, now - 100 * kTick, 120));
  CHECK_FALSE(isSaveFresh(now, now - 100 * kTick, 99));
  CHECK(isSaveFresh(now, now, 0));                    // zero window: only age 0
  CHECK_FALSE(isSaveFresh(now, now - 1 * kTick, 0));
}
