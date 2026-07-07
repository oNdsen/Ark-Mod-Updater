// amu_updater.exe - the standalone self-updater for the C++ AMU.
//
// Launched by amu.exe as:  amu_updater.exe --wait-pid <pid> --dir "<installDir>"
// (installDir is the folder containing amu.exe; this exe lives in installDir\lib).
//
// Flow (generalizes the old single-exe update.au3 to a multi-file manifest):
//   1. wait for the old amu.exe to exit (--wait-pid)
//   2. GET version.txt + manifest.txt from the fixed update channel
//   3. download every manifest file into lib\update_staging\<relpath>
//   4. verify size + SHA-256 of every staged file (any mismatch aborts, no swap)
//   5. all-or-nothing swap: live -> .bak (15x1s retry for the possibly still
//      locked amu.exe), staged -> live; any failure rolls every .bak back
//   6. delete .bak files + staging, relaunch amu.exe

#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "amucore/updatecheck.h"

#pragma comment(lib, "comctl32.lib")

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kTitle[] = L"AMU Updater";

// Posted by the worker thread to the (main-thread) progress window.
constexpr UINT kMsgStatus = WM_APP + 1;    // wParam: wchar_t* (new[]), owned by receiver
constexpr UINT kMsgProgress = WM_APP + 2;  // wParam: pos, lParam: range (0 = keep range)
constexpr UINT kMsgDone = WM_APP + 3;      // wParam: exit code, lParam: wchar_t* error or null

HWND g_hwnd = nullptr;
HWND g_status = nullptr;
HWND g_progress = nullptr;
HBRUSH g_bgBrush = nullptr;
int g_exitCode = 1;
std::wstring g_error;  // shown as a MessageBox after the window closes

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------

std::wstring utf8ToWide(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                    static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return {};
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                      w.data(), n);
  return w;
}

std::string wideToUtf8(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                    static_cast<int>(w.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 0) return {};
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                      s.data(), n, nullptr, nullptr);
  return s;
}

// Path-traversal guard for manifest relpaths. Rejects absolute paths, drive
// letters, UNC prefixes and any ".." component - a hostile manifest must not
// be able to write outside the install folder.
bool isSafeRelPath(const std::string& p) {
  if (p.empty()) return false;
  if (p[0] == '/' || p[0] == '\\') return false;      // rooted or UNC
  if (p.find(':') != std::string::npos) return false; // drive / ADS
  size_t start = 0;
  for (size_t i = 0; i <= p.size(); ++i) {
    if (i == p.size() || p[i] == '/' || p[i] == '\\') {
      const std::string comp = p.substr(start, i - start);
      if (comp == "..") return false;
      if (comp.empty() && i != p.size()) return false;  // "a//b"
      start = i + 1;
    }
  }
  return true;
}

std::string withSlashes(std::string p, char from, char to) {
  for (char& c : p) {
    if (c == from) c = to;
  }
  return p;
}

void postStatus(const std::wstring& text) {
  wchar_t* copy = new wchar_t[text.size() + 1];
  memcpy(copy, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
  if (!PostMessageW(g_hwnd, kMsgStatus, reinterpret_cast<WPARAM>(copy), 0)) {
    delete[] copy;
  }
}

void postProgress(int pos, int range = 0) {
  PostMessageW(g_hwnd, kMsgProgress, static_cast<WPARAM>(pos),
               static_cast<LPARAM>(range));
}

void postDone(int exitCode, const std::wstring& error) {
  wchar_t* copy = nullptr;
  if (!error.empty()) {
    copy = new wchar_t[error.size() + 1];
    memcpy(copy, error.c_str(), (error.size() + 1) * sizeof(wchar_t));
  }
  if (!PostMessageW(g_hwnd, kMsgDone, static_cast<WPARAM>(exitCode),
                    reinterpret_cast<LPARAM>(copy))) {
    delete[] copy;
  }
}

std::string trimAscii(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' ||
                   s[e - 1] == '\n')) {
    --e;
  }
  return s.substr(b, e - b);
}

// ---------------------------------------------------------------------------
// worker: download, verify, swap
// ---------------------------------------------------------------------------

void runUpdate(const std::wstring& installDir) {
  using namespace amucore;

  const fs::path install(installDir);
  const fs::path staging = install / L"lib" / L"update_staging";

  auto cleanupStaging = [&staging]() {
    std::error_code ec;
    fs::remove_all(staging, ec);
  };

  // --- 1. version + manifest -----------------------------------------------
  postStatus(L"Checking for the latest version...");
  const std::wstring base = kUpdateBasePath;

  const std::string version =
      trimAscii(httpGetText(kUpdateHost, base + L"version.txt"));
  if (version.empty()) {
    postDone(1,
             L"Could not download the update information.\n"
             L"Please check your internet connection and try again.");
    return;
  }

  const std::string manifestText = httpGetText(kUpdateHost, base + L"manifest.txt");
  const Manifest manifest = parseManifest(manifestText, version);
  if (manifest.files.empty()) {
    postDone(1,
             L"The update manifest could not be downloaded or is invalid.\n"
             L"Please try again later.");
    return;
  }

  const int total = static_cast<int>(manifest.files.size());
  int step = 0;
  postProgress(0, total * 2);  // download+verify, then swap, one tick each

  // --- 2. download + verify into the staging folder ------------------------
  cleanupStaging();  // stale leftovers from an aborted run
  {
    std::error_code ec;
    fs::create_directories(staging, ec);
    if (ec) {
      postDone(1, L"Could not create the download folder inside the AMU "
                  L"directory.\nPlease check the folder permissions.");
      return;
    }
  }

  for (const ManifestFile& f : manifest.files) {
    if (!isSafeRelPath(f.path)) {
      cleanupStaging();
      postDone(1, L"The update manifest contains an unsafe file path.\n"
                  L"The update was aborted; nothing was changed.");
      return;
    }

    // GitHub release assets are flat: the URL uses the flattened asset name
    // (ui/main.html -> ui_main.html); staging/install keep the real relpath.
    const std::string diskRel = withSlashes(f.path, '/', '\\');
    const fs::path dest = staging / utf8ToWide(diskRel);
    const std::wstring wRel = utf8ToWide(withSlashes(f.path, '\\', '/'));
    const std::wstring wAsset = utf8ToWide(updateAssetName(f.path));

    {
      std::error_code ec;
      fs::create_directories(dest.parent_path(), ec);
    }

    postStatus(L"Downloading " + wRel + L"...");
    if (!httpDownloadFile(kUpdateHost, base + wAsset, wideToUtf8(dest.wstring()))) {
      cleanupStaging();
      postDone(1, L"Failed to download \"" + wRel +
                      L"\".\nPlease check your internet connection and try "
                      L"again.");
      return;
    }

    // Verify size AND SHA-256 before anything live is touched.
    std::error_code ec;
    const uintmax_t got = fs::file_size(dest, ec);
    const bool sizeOk = !ec && static_cast<int64_t>(got) == f.size;
    if (!sizeOk || sha256File(wideToUtf8(dest.wstring())) != f.sha256) {
      cleanupStaging();
      postDone(1, L"The downloaded file \"" + wRel +
                      L"\" failed the integrity check.\nThe update was "
                      L"aborted; the current version was kept.");
      return;
    }
    postProgress(++step);
  }

  // --- 3. all-or-nothing swap ----------------------------------------------
  postStatus(L"Installing the update...");
  // .bak that was moved aside -> the live path it must be restored to.
  std::vector<std::pair<fs::path, fs::path>> movedAside;
  bool swapOk = true;

  for (const ManifestFile& f : manifest.files) {
    const std::wstring rel = utf8ToWide(withSlashes(f.path, '/', '\\'));
    const fs::path live = install / rel;
    fs::path bak = live;
    bak += L".bak";
    const fs::path staged = staging / rel;

    {
      std::error_code ec;
      fs::create_directories(live.parent_path(), ec);  // new dirs in new versions
    }

    if (GetFileAttributesW(live.c_str()) != INVALID_FILE_ATTRIBUTES) {
      // amu.exe may still hold a lock while shutting down - retry like the
      // old update.au3 did (15 x 1s).
      bool renamed = false;
      for (int attempt = 0; attempt < 15; ++attempt) {
        if (MoveFileExW(live.c_str(), bak.c_str(), MOVEFILE_REPLACE_EXISTING)) {
          renamed = true;
          break;
        }
        Sleep(1000);
      }
      if (!renamed) {
        swapOk = false;
        break;
      }
      movedAside.emplace_back(bak, live);
    }

    if (!MoveFileExW(staged.c_str(), live.c_str(), MOVEFILE_REPLACE_EXISTING)) {
      swapOk = false;  // rollback below restores this file's .bak as well
      break;
    }
    postProgress(++step);
  }

  if (!swapOk) {
    for (auto it = movedAside.rbegin(); it != movedAside.rend(); ++it) {
      MoveFileExW(it->first.c_str(), it->second.c_str(),
                  MOVEFILE_REPLACE_EXISTING);
    }
    cleanupStaging();
    postDone(1, L"Could not replace the application files (a file may still "
                L"be in use).\nThe previous version was kept.");
    return;
  }

  for (const auto& [bak, live] : movedAside) {
    (void)live;
    DeleteFileW(bak.c_str());
  }
  cleanupStaging();

  // --- 4. relaunch ----------------------------------------------------------
  postStatus(L"Starting AMU...");
  const fs::path amuExe = install / L"amu.exe";
  ShellExecuteW(nullptr, L"open", amuExe.c_str(), nullptr, install.c_str(),
                SW_SHOWNORMAL);
  postDone(0, L"");
}

// ---------------------------------------------------------------------------
// progress window
// ---------------------------------------------------------------------------

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case kMsgStatus: {
      wchar_t* text = reinterpret_cast<wchar_t*>(wParam);
      SetWindowTextW(g_status, text);
      delete[] text;
      return 0;
    }
    case kMsgProgress:
      if (lParam != 0) {
        SendMessageW(g_progress, PBM_SETRANGE32, 0, lParam);
      }
      SendMessageW(g_progress, PBM_SETPOS, wParam, 0);
      return 0;
    case kMsgDone: {
      g_exitCode = static_cast<int>(wParam);
      wchar_t* err = reinterpret_cast<wchar_t*>(lParam);
      if (err) {
        g_error = err;
        delete[] err;
      }
      DestroyWindow(hwnd);
      return 0;
    }
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wParam);
      SetTextColor(dc, RGB(230, 230, 230));
      SetBkColor(dc, RGB(32, 32, 36));
      return reinterpret_cast<LRESULT>(g_bgBrush);
    }
    case WM_CLOSE:
      return 0;  // no user cancel mid-swap; the window closes itself when done
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool createProgressWindow(HINSTANCE hInstance) {
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_PROGRESS_CLASS};
  InitCommonControlsEx(&icc);

  g_bgBrush = CreateSolidBrush(RGB(32, 32, 36));

  WNDCLASSW wc = {};
  wc.lpfnWndProc = wndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hbrBackground = g_bgBrush;
  wc.lpszClassName = L"AmuUpdaterWnd";
  if (!RegisterClassW(&wc)) return false;

  const int clientW = 420;
  const int clientH = 96;
  RECT r = {0, 0, clientW, clientH};
  const DWORD style = WS_CAPTION | WS_SYSMENU;
  AdjustWindowRect(&r, style, FALSE);
  const int winW = r.right - r.left;
  const int winH = r.bottom - r.top;
  const int x = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
  const int y = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

  g_hwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, kTitle, style, x, y,
                           winW, winH, nullptr, nullptr, hInstance, nullptr);
  if (!g_hwnd) return false;

  g_status = CreateWindowExW(0, L"STATIC", L"Preparing update...",
                             WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, 16, 18,
                             clientW - 32, 20, g_hwnd, nullptr, hInstance,
                             nullptr);
  g_progress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
                               WS_CHILD | WS_VISIBLE, 16, 48, clientW - 32, 22,
                               g_hwnd, nullptr, hInstance, nullptr);
  if (!g_status || !g_progress) return false;

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  ShowWindow(g_hwnd, SW_SHOWNORMAL);
  UpdateWindow(g_hwnd);
  return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// entry point
// ---------------------------------------------------------------------------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrev*/,
                    PWSTR /*cmdLine*/, int /*nShow*/) {
  // --- CLI: --wait-pid <pid> --dir "<installDir>" ---------------------------
  DWORD waitPid = 0;
  std::wstring installDir;
  {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
      for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--wait-pid" && i + 1 < argc) {
          waitPid = wcstoul(argv[++i], nullptr, 10);
        } else if (arg == L"--dir" && i + 1 < argc) {
          installDir = argv[++i];
        }
      }
      LocalFree(argv);
    }
  }

  if (installDir.empty()) {
    // We live at <installDir>\lib\amu_updater.exe -> parent of the exe dir.
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    installDir = fs::path(exePath).parent_path().parent_path().wstring();
  }
  while (!installDir.empty() &&
         (installDir.back() == L'\\' || installDir.back() == L'/')) {
    installDir.pop_back();
  }
  if (installDir.empty()) {
    MessageBoxW(nullptr,
                L"Could not determine the AMU installation folder.\n"
                L"Please start the updater from AMU itself.",
                kTitle, MB_ICONERROR | MB_TOPMOST | MB_OK);
    return 1;
  }
  // Note: <installDir>\amu.exe may legitimately be missing (fresh layout /
  // interrupted previous update) - the manifest download restores it, so we
  // proceed either way.

  // --- wait for the old app to exit (it launched us and is closing) ---------
  if (waitPid != 0) {
    HANDLE proc = OpenProcess(SYNCHRONIZE, FALSE, waitPid);
    if (proc) {
      WaitForSingleObject(proc, 30000);
      CloseHandle(proc);
    }
  }

  if (!createProgressWindow(hInstance)) {
    MessageBoxW(nullptr, L"Could not create the updater window.", kTitle,
                MB_ICONERROR | MB_TOPMOST | MB_OK);
    return 1;
  }

  std::thread worker(runUpdate, installDir);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  worker.join();

  if (g_exitCode != 0 && !g_error.empty()) {
    MessageBoxW(nullptr, g_error.c_str(), kTitle,
                MB_ICONERROR | MB_TOPMOST | MB_OK);
  }
  return g_exitCode;
}
