// AMU (C++ rewrite) - Dear ImGui + Direct3D11 shell.
// Milestone 1: premium-styled UI shell with STUB data (no amucore wiring yet).
// D3D11/Win32 boilerplate adapted from the official Dear ImGui example.

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#ifdef IMGUI_ENABLE_FREETYPE
#include "imgui_freetype.h"
#endif

#include "amucore/version.h"

// ---- Direct3D state -------------------------------------------------------
static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static UINT                    g_ResizeWidth = 0, g_ResizeHeight = 0;

static float S = 1.0f;  // UI scale (monitor DPI). Multiply custom pixel sizes by this.
static HWND g_hwnd = nullptr;

static const float kHeaderH = 60.0f;  // logical header height (title bar); scaled by S at use

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static void ApplyPremiumTheme();
static void DrawAmuUI();

// ---- Links (from the AutoIt build) ---------------------------------------
static const char* URL_ONDSEN   = "https://ondsen.ch";
static const char* URL_PAYPAL   = "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=7H8LA6ZSYS752";
static const char* URL_WORKSHOP = "https://steamcommunity.com/sharedfiles/filedetails/?id=";

// ---- Premium palette (mockup C) ------------------------------------------
namespace col {
static const ImU32 accent      = IM_COL32(0x3D, 0xDC, 0x84, 0xFF);
static const ImU32 accent_hov  = IM_COL32(0x54, 0xE8, 0x99, 0xFF);
static const ImU32 accent_act  = IM_COL32(0x2E, 0xB0, 0x69, 0xFF);
static const ImU32 accent_txt  = IM_COL32(0x06, 0x2A, 0x18, 0xFF);
static const ImU32 accent_dim  = IM_COL32(0x14, 0x39, 0x24, 0xFF);
static const ImU32 danger      = IM_COL32(0xFF, 0x6B, 0x74, 0xFF);
static const ImU32 danger_dim  = IM_COL32(0x33, 0x18, 0x1B, 0xFF);
static const ImU32 warn        = IM_COL32(0xF5, 0xB4, 0x4A, 0xFF);
static const ImU32 warn_dim    = IM_COL32(0x3A, 0x2F, 0x12, 0xFF);
static const ImU32 text        = IM_COL32(0xEC, 0xF0, 0xF5, 0xFF);
static const ImU32 muted       = IM_COL32(0x9A, 0xA6, 0xB4, 0xFF);
static const ImU32 chip_bg     = IM_COL32(0x26, 0x2C, 0x36, 0xFF);
static const ImU32 card_active = IM_COL32(0x1F, 0x25, 0x30, 0xFF);
static const ImU32 card_hover  = IM_COL32(0x17, 0x1C, 0x25, 0xFF);
static const ImU32 panel       = IM_COL32(0x12, 0x15, 0x1C, 0xFF);
static const ImU32 section     = IM_COL32(0x6C, 0x76, 0x86, 0xFF);  // sidebar section header
}  // namespace col

// ---- Stub data (replaced by amucore later) --------------------------------
struct Mod { const char* id; const char* name; int state; };  // 0 inst,1 incompl,2 not,3 removed
struct ServerData { const char* name; const char* idStr; const char* map;
                    const char* path; const Mod* mods; int modCount; };

static const Mod kMods0[] = {
    { "731604991",  "Structures Plus",  0 },
    { "895711211",  "Awesome SpyGlass", 1 },
    { "1404697612", "Super Structures", 2 },
    { "632091170",  "Old Mod XYZ",      3 },
};
static const Mod kMods1[] = {
    { "1609138312", "Dino Storage v2",       0 },
    { "821530042",  "Automated Ark",         0 },
    { "1178308359", "Krakens Better Dinos",  2 },
};
static const ServerData kServers[2] = {
    { "Server 1", "1", "TheIsland", "D:\\ARK\\srv1\\ShooterGame", kMods0, 4 },
    { "Server 2", "2", "Ragnarok",  "D:\\ARK\\srv2\\ShooterGame", kMods1, 3 },
};

static int g_screen = 1;        // 0 = Server, 1 = Mods, 2 = Logs
static int g_currentServer = 0;
static int g_selectedMod = 0;
static bool g_logDebug = false;
static ImFont* g_titleFont = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
  ImGui_ImplWin32_EnableDpiAwareness();

  WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
                     hInstance, nullptr, nullptr, nullptr, nullptr,
                     L"AMU_Main", nullptr };
  ::RegisterClassExW(&wc);
  HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"ARK Mod Updater",
                              WS_OVERLAPPEDWINDOW, 100, 100, 1000, 660,
                              nullptr, nullptr, wc.hInstance, nullptr);
  g_hwnd = hwnd;

  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;

  S = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
  if (S < 1.0f) S = 1.0f;

  // Frameless: keep a 1px DWM frame so the window still casts a shadow, and force a
  // frame recalculation so WM_NCCALCSIZE (which strips the title bar) takes effect.
  MARGINS margins = {0, 0, 0, 1};
  ::DwmExtendFrameIntoClientArea(hwnd, &margins);
  ::SetWindowPos(hwnd, nullptr, 0, 0, (int)(1000 * S), (int)(660 * S),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
  ::ShowWindow(hwnd, SW_SHOWDEFAULT);
  ::UpdateWindow(hwnd);

  char exePath[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, exePath, MAX_PATH);
  std::string dir(exePath);
  const size_t slash = dir.find_last_of("\\/");
  if (slash != std::string::npos) dir = dir.substr(0, slash);
  const std::string robotoPath = dir + "\\assets\\Roboto-Light.ttf";
  ImFontConfig cfg;
#ifndef IMGUI_ENABLE_FREETYPE
  cfg.OversampleH = 2;
  cfg.OversampleV = 2;
#endif
  const char* candidates[] = {
      "C:\\Windows\\Fonts\\segoeui.ttf",  // clean, highly readable Windows UI font
      robotoPath.c_str(),                 // bundled fallback (Roboto Light)
  };
  const char* loadedFont = nullptr;
  for (const char* f : candidates) {
    if (GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES) {
      if (io.Fonts->AddFontFromFileTTF(f, 20.0f * S, &cfg)) { loadedFont = f; break; }
    }
  }
  if (!loadedFont) io.FontGlobalScale = S;

  // Title font (bold, larger) for the header bar.
  const char* titleCands[] = { "C:\\Windows\\Fonts\\segoeuib.ttf",
                               loadedFont ? loadedFont : "" };
  for (const char* f : titleCands) {
    if (f && *f && GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES) {
      g_titleFont = io.Fonts->AddFontFromFileTTF(f, 22.0f * S, &cfg);
      if (g_titleFont) break;
    }
  }

  ApplyPremiumTheme();
  ImGui::GetStyle().ScaleAllSizes(S);

  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

  bool done = false;
  while (!done) {
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
    if (done) break;

    if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
      CleanupRenderTarget();
      g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight,
                                  DXGI_FORMAT_UNKNOWN, 0);
      g_ResizeWidth = g_ResizeHeight = 0;
      CreateRenderTarget();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    DrawAmuUI();

    ImGui::Render();
    const float clear[4] = { 0.055f, 0.066f, 0.086f, 1.0f };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  CleanupDeviceD3D();
  ::DestroyWindow(hwnd);
  ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return 0;
}

// ---- UI helpers -----------------------------------------------------------
static void OpenUrl(const char* url) {
  ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

// Show the hand cursor while hovering the last-drawn interactive item.
static void HandOnHover() {
  if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

// Plain button that shows the hand cursor on hover.
static bool Btn(const char* label) {
  const bool r = ImGui::Button(label);
  HandOnHover();
  return r;
}

static bool PrimaryButton(const char* label) {
  ImGui::PushStyleColor(ImGuiCol_Button, col::accent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col::accent_hov);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, col::accent_act);
  ImGui::PushStyleColor(ImGuiCol_Text, col::accent_txt);
  const bool r = ImGui::Button(label);
  HandOnHover();
  ImGui::PopStyleColor(4);
  return r;
}

static void LinkText(const char* label, const char* url) {
  ImGui::TextColored(ImColor(col::accent), "%s", label);
  if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(mn.x, mx.y), ImVec2(mx.x, mx.y),
                                        col::accent, 1.0f);
  }
  if (ImGui::IsItemClicked()) OpenUrl(url);
}

// Subtle inline link: muted text, accent + underline on hover. Caller positions it.
static void InlineLink(const char* label, const char* url) {
  ImGui::TextColored(ImColor(col::muted), "%s", label);
  if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddText(mn, col::accent, label);  // brighten
    ImGui::GetWindowDrawList()->AddLine(ImVec2(mn.x, mx.y), ImVec2(mx.x, mx.y),
                                        col::accent, 1.0f);
  }
  if (ImGui::IsItemClicked()) OpenUrl(url);
}

static void Pill(const char* text, ImU32 bg, ImU32 fg) {
  const ImVec2 sz = ImGui::CalcTextSize(text);
  const float px = 9.0f * S, py = 3.0f * S;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + sz.x + px * 2, p.y + sz.y + py * 2), bg, 10.0f * S);
  dl->AddText(ImVec2(p.x + px, p.y + py), fg, text);
  ImGui::Dummy(ImVec2(sz.x + px * 2, sz.y + py * 2));
}

static void SectionHeader(const char* text) {
  ImGui::Dummy(ImVec2(0, 8 * S));
  ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);
  ImGui::TextColored(ImColor(col::section), "%s", text);
  ImGui::Dummy(ImVec2(0, 4 * S));
}

// A uniform sidebar nav row: optional status dot, label, optional count badge,
// with hover + active highlight (accent left bar). Returns true when clicked.
static bool NavItem(const char* id, const char* label, bool active, ImU32 dot = 0,
                    int badge = -1) {
  const float w = ImGui::GetContentRegionAvail().x;
  const float fs = ImGui::GetFontSize();
  const float h = fs + 18 * S;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  const bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
  const bool hov = ImGui::IsItemHovered();
  if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  if (active) {
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col::card_active, 8 * S);
    dl->AddRectFilled(p, ImVec2(p.x + 3 * S, p.y + h), col::accent, 8 * S);
  } else if (hov) {
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col::card_hover, 8 * S);
  }

  const float midy = p.y + h * 0.5f;
  float tx = p.x + 14 * S;
  if (dot) {
    dl->AddCircleFilled(ImVec2(tx + 3 * S, midy), 4 * S, dot);
    tx += 16 * S;
  }
  dl->AddText(ImVec2(tx, midy - fs * 0.5f), active ? col::text : col::muted, label);

  if (badge >= 0) {
    char b[16];
    snprintf(b, sizeof(b), "%d", badge);
    const ImVec2 ts = ImGui::CalcTextSize(b);
    const float bw = ts.x + 12 * S, bh = fs + 2 * S;
    const float bx = p.x + w - bw - 12 * S, by = midy - bh * 0.5f;
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), col::chip_bg, bh * 0.5f);
    dl->AddText(ImVec2(bx + 6 * S, midy - ts.y * 0.5f), col::muted, b);
  }
  ImGui::Dummy(ImVec2(0, 3 * S));
  return clicked;
}

// A website-style segmented pill control (connected segments, active one filled).
static int Segmented(const char* const* labels, int n, int sel) {
  const float fs = ImGui::GetFontSize();
  const float h = fs + 14 * S;
  const float padx = 18 * S;
  float widths[6] = {};
  float total = 0;
  for (int i = 0; i < n && i < 6; ++i) {
    widths[i] = ImGui::CalcTextSize(labels[i]).x + padx * 2;
    total += widths[i];
  }
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + total, p.y + h), col::panel, h * 0.5f);
  float x = p.x;
  for (int i = 0; i < n && i < 6; ++i) {
    const bool active = (sel == i);
    if (active)
      dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + widths[i], p.y + h), col::accent, h * 0.5f);
    const ImVec2 ts = ImGui::CalcTextSize(labels[i]);
    dl->AddText(ImVec2(x + (widths[i] - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f),
                active ? col::accent_txt : col::muted, labels[i]);
    ImGui::SetCursorScreenPos(ImVec2(x, p.y));
    if (ImGui::InvisibleButton(labels[i], ImVec2(widths[i], h))) sel = i;
    HandOnHover();
    x += widths[i];
  }
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(total, h));
  return sel;
}

static void DrawModsScreen() {
  const ServerData& sv = kServers[g_currentServer];
  if (g_selectedMod >= sv.modCount) g_selectedMod = 0;

  ImGui::TextColored(ImColor(col::text), "Mods from:");
  ImGui::SameLine();
  ImGui::TextColored(ImColor(col::accent), "%s", sv.name);
  ImGui::Dummy(ImVec2(0, 8 * S));

  ImGui::BeginChild("modstable", ImVec2(-240 * S, -70 * S), false);
  ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                       ImGuiTableFlags_PadOuterX;
  if (ImGui::BeginTable("mods", 4, tf)) {
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28 * S);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 100 * S);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < sv.modCount; ++i) {
      const Mod& m = sv.mods[i];
      ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFontSize() + 12 * S);
      if (m.state == 3)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, col::danger_dim);

      ImGui::TableSetColumnIndex(0);
      char label[32];
      snprintf(label, sizeof(label), "%d##row%d", i + 1, i);
      if (ImGui::Selectable(label, g_selectedMod == i,
                            ImGuiSelectableFlags_SpanAllColumns))
        g_selectedMod = i;
      HandOnHover();

      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(ImColor(col::muted), "%s", m.id);

      ImGui::TableSetColumnIndex(2);
      switch (m.state) {
        case 0: Pill("Installed - 06-14", col::accent_dim, col::accent); break;
        case 1: Pill("Incomplete", col::warn_dim, col::warn); break;
        case 2: Pill("Not installed", col::chip_bg, col::muted); break;
        default: Pill("Removed", col::danger_dim, col::danger); break;
      }

      ImGui::TableSetColumnIndex(3);
      ImGui::TextColored(ImColor(m.state == 3 ? col::danger : col::text), "%s", m.name);
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("preview", ImVec2(228 * S, -70 * S), true);
  const Mod& sel = sv.mods[g_selectedMod];
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float pw = ImGui::GetContentRegionAvail().x;
  const float ph = 150 * S;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + pw, p.y + ph), col::card_active, 8.0f * S);
  const char* phTxt = "no preview yet";
  const ImVec2 ts = ImGui::CalcTextSize(phTxt);
  dl->AddText(ImVec2(p.x + (pw - ts.x) * 0.5f, p.y + ph * 0.5f - ts.y * 0.5f),
              col::muted, phTxt);
  ImGui::Dummy(ImVec2(pw, ph));
  ImGui::Dummy(ImVec2(0, 10 * S));
  ImGui::TextColored(ImColor(col::text), "%s", sel.name);
  ImGui::TextColored(ImColor(col::muted), "ID %s", sel.id);
  ImGui::Dummy(ImVec2(0, 10 * S));

  ImGui::TextColored(ImColor(col::muted), "Status");
  ImGui::SameLine(96 * S);
  switch (sel.state) {
    case 0: Pill("Installed", col::accent_dim, col::accent); break;
    case 1: Pill("Incomplete", col::warn_dim, col::warn); break;
    case 2: Pill("Not installed", col::chip_bg, col::muted); break;
    default: Pill("Removed", col::danger_dim, col::danger); break;
  }
  ImGui::TextColored(ImColor(col::muted), "Size");
  ImGui::SameLine(96 * S);
  ImGui::TextColored(ImColor(col::text), "245 MB");
  ImGui::TextColored(ImColor(col::muted), "Updated");
  ImGui::SameLine(96 * S);
  ImGui::TextColored(ImColor(col::text), "2026-06-14");
  ImGui::Dummy(ImVec2(0, 10 * S));
  const std::string wsUrl = std::string(URL_WORKSHOP) + sel.id;
  LinkText("View on Workshop", wsUrl.c_str());
  ImGui::EndChild();

  ImGui::Dummy(ImVec2(0, 6 * S));
  ImGui::Indent(12 * S);  // align the action bar with the table columns
  PrimaryButton("Update all Mods");
  ImGui::SameLine();
  Btn("Update selected");
  ImGui::SameLine();
  Btn("Add Mod");
  ImGui::SameLine();
  Btn("Remove Mod");
  ImGui::Unindent(12 * S);
}

static void FormField(const char* label, const char* value, float width) {
  ImGui::TextColored(ImColor(col::muted), "%s", label);
  char buf[256];
  snprintf(buf, sizeof(buf), "%s", value);
  ImGui::PushID(label);
  ImGui::SetNextItemWidth(width);
  ImGui::InputText("##f", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
  ImGui::PopID();
  ImGui::Dummy(ImVec2(0, 6 * S));
}

static void DrawServerScreen() {
  const ServerData& sv = kServers[g_currentServer];
  ImGui::TextColored(ImColor(col::text), "%s", sv.name);
  ImGui::SameLine();
  ImGui::TextColored(ImColor(col::muted), "- ID %s", sv.idStr);
  ImGui::Dummy(ImVec2(0, 10 * S));

  const float half = (ImGui::GetContentRegionAvail().x - 16 * S) * 0.5f;
  ImGui::BeginGroup();
  FormField("Name", sv.name, half);
  FormField("Path", sv.path, half);
  FormField("RCON IP", "127.0.0.1", half);
  ImGui::EndGroup();
  ImGui::SameLine(0, 16 * S);
  ImGui::BeginGroup();
  FormField("Map", sv.map, half);
  FormField("Startscript", "start.bat", half);
  FormField("RCON Port", "27020", half);
  ImGui::EndGroup();

  ImGui::Dummy(ImVec2(0, 10 * S));
  PrimaryButton("Save Server");
  ImGui::SameLine();
  Btn("Update this Server");
  ImGui::SameLine();
  if (Btn("Show Mods")) g_screen = 1;
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, ImColor(col::danger).Value);
  Btn("Delete");
  ImGui::PopStyleColor();
}

static void DrawLogsScreen() {
  ImGui::Checkbox("Debug", &g_logDebug);
  HandOnHover();
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 84 * S);
  Btn("Clear Log");
  ImGui::Dummy(ImVec2(0, 8 * S));

  ImGui::BeginChild("logbox", ImVec2(0, 0), true);
  struct Line { const char* t; ImU32 c; const char* m; bool debug; };
  static const Line lines[] = {
      { "18:42:07", col::accent, "Updated Mod 895711211 (Awesome SpyGlass)", false },
      { "18:42:01", col::text,   "Installed Mod 731604991 (Structures Plus)", false },
      { "18:41:55", IM_COL32(0x7F, 0xB0, 0xE8, 0xFF),
        "[steamcmd] Success. Downloaded item 731604991", true },
      { "18:41:54", col::muted,  "[debug] 2 active mods resolved, 1 update needed", true },
      { "18:41:30", col::danger, "RCON: saveworld timeout, proceeding", false },
      { "18:41:12", col::text,   "Broadcast: server restart in 5 min", false },
  };
  for (const Line& l : lines) {
    if (l.debug && !g_logDebug) continue;
    ImGui::TextColored(ImColor(col::muted), "%s", l.t);
    ImGui::SameLine();
    ImGui::TextColored(ImColor(l.c), "%s", l.m);
  }
  ImGui::EndChild();
}

// Custom minimize / maximize / close buttons at the right of the (frameless) header.
static void CaptionButtons(float headerH) {
  const float bw = 46 * S;
  const float startX = ImGui::GetWindowWidth() - bw * 3;
  const ImVec2 wp = ImGui::GetWindowPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  for (int i = 0; i < 3; ++i) {
    ImGui::SetCursorPos(ImVec2(startX + bw * i, 0));
    char id[8];
    snprintf(id, sizeof(id), "##cb%d", i);
    ImGui::InvisibleButton(id, ImVec2(bw, headerH));
    const bool hov = ImGui::IsItemHovered();
    if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    const ImVec2 o(wp.x + startX + bw * i, wp.y);
    const bool isClose = (i == 2);
    if (hov)
      dl->AddRectFilled(o, ImVec2(o.x + bw, o.y + headerH),
                        isClose ? IM_COL32(0xDF, 0x35, 0x35, 0xFF) : col::card_hover);

    const float cx = o.x + bw * 0.5f, cy = o.y + headerH * 0.5f, s2 = 5 * S, th = 1.4f * S;
    const ImU32 ic = (hov && isClose) ? IM_COL32(0xFF, 0xFF, 0xFF, 0xFF) : col::muted;
    if (i == 0) {
      dl->AddLine(ImVec2(cx - s2, cy), ImVec2(cx + s2, cy), ic, th);
    } else if (i == 1) {
      dl->AddRect(ImVec2(cx - s2, cy - s2), ImVec2(cx + s2, cy + s2), ic, 0.0f, 0, th);
    } else {
      dl->AddLine(ImVec2(cx - s2, cy - s2), ImVec2(cx + s2, cy + s2), ic, th);
      dl->AddLine(ImVec2(cx - s2, cy + s2), ImVec2(cx + s2, cy - s2), ic, th);
    }

    if (ImGui::IsItemClicked()) {
      if (i == 0) ::ShowWindow(g_hwnd, SW_MINIMIZE);
      else if (i == 1) ::ShowWindow(g_hwnd, ::IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
      else ::PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    }
  }
}

static void DrawAmuUI() {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("root", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoNavFocus);
  ImGui::PopStyleVar();

  // Header bar (full width): green logo mark + bold title + window buttons.
  const float headerH = kHeaderH * S;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, col::panel);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::BeginChild("header", ImVec2(0, headerH), false, ImGuiWindowFlags_NoScrollbar);
  {
    ImDrawList* hdl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const float padx = 26 * S;
    const float midy = wp.y + headerH * 0.5f;
    const float sq = 26 * S;
    hdl->AddRectFilled(ImVec2(wp.x + padx, midy - sq * 0.5f),
                       ImVec2(wp.x + padx + sq, midy + sq * 0.5f), col::accent, 7 * S);
    ImFont* tf = g_titleFont ? g_titleFont : ImGui::GetFont();
    const float th = tf->FontSize;
    hdl->AddText(tf, th, ImVec2(wp.x + padx + sq + 14 * S, midy - th * 0.5f),
                 col::text, "ARK MOD UPDATER");
  }
  CaptionButtons(headerH);
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  // Body (sidebar + content), leaving room for the bottom footer bar.
  // Reserve the footer height PLUS one ItemSpacing (the auto gap between the stacked
  // child windows) so the footer bar isn't pushed off the bottom of the window.
  const float footerBarH = 34 * S;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::BeginChild("body", ImVec2(0, -(footerBarH + ImGui::GetStyle().ItemSpacing.y)), false);
  ImGui::PopStyleVar();

  // Sidebar (navigation only)
  ImGui::PushStyleColor(ImGuiCol_ChildBg, col::panel);
  ImGui::BeginChild("sidebar", ImVec2(210 * S, 0), false);
  SectionHeader("SERVERS");
  for (int i = 0; i < 2; ++i) {
    const bool active = (g_screen != 2 && g_currentServer == i);
    if (NavItem(kServers[i].name, kServers[i].name, active,
                i == 0 ? col::accent : col::danger, kServers[i].modCount)) {
      g_currentServer = i;
      if (g_screen == 2) g_screen = 1;
    }
  }
  NavItem("addsrv", "+ Add Server", false);
  SectionHeader("SYSTEM");
  if (NavItem("logs", "Logs", g_screen == 2)) g_screen = 2;
  ImGui::EndChild();
  ImGui::PopStyleColor();

  ImGui::SameLine();

  // Main content
  ImGui::BeginChild("main", ImVec2(0, 0), false);
  if (g_screen == 2) {
    DrawLogsScreen();
  } else {
    static const char* const kSegs[2] = {"Server", "Mods"};
    g_screen = Segmented(kSegs, 2, g_screen);
    ImGui::Dummy(ImVec2(0, 12 * S));
    if (g_screen == 0) DrawServerScreen();
    else DrawModsScreen();
  }
  ImGui::EndChild();

  ImGui::EndChild();  // body

  // Footer bar (full width, subtle links).
  ImGui::PushStyleColor(ImGuiCol_ChildBg, col::panel);
  ImGui::BeginChild("footerbar", ImVec2(0, footerBarH), false, ImGuiWindowFlags_NoScrollbar);
  {
    const float fs = ImGui::GetFontSize();
    const float ly = (footerBarH - fs) * 0.5f;
    const float padx = ImGui::GetStyle().WindowPadding.x;
    ImGui::SetCursorPos(ImVec2(padx, ly));
    InlineLink("ondsen.ch", URL_ONDSEN);
    const float dw = ImGui::CalcTextSize("Donate a beer (PayPal)").x;
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - dw - padx, ly));
    InlineLink("Donate a beer (PayPal)", URL_PAYPAL);
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();

  // Dividers: under the header, above the footer bar, and between sidebar and content.
  ImDrawList* rdl = ImGui::GetWindowDrawList();
  const ImU32 line = IM_COL32(0x23, 0x28, 0x33, 0xFF);
  const float x0 = vp->WorkPos.x, y0 = vp->WorkPos.y;
  const float hy = y0 + headerH;
  const float fy = y0 + vp->WorkSize.y - footerBarH;
  rdl->AddLine(ImVec2(x0, hy), ImVec2(x0 + vp->WorkSize.x, hy), line, 1.0f);
  rdl->AddLine(ImVec2(x0, fy), ImVec2(x0 + vp->WorkSize.x, fy), line, 1.0f);
  rdl->AddLine(ImVec2(x0 + 210 * S, hy), ImVec2(x0 + 210 * S, fy), line, 1.0f);
  ImGui::End();
}

static void ApplyPremiumTheme() {
  ImGui::StyleColorsDark();
  ImGuiStyle& s = ImGui::GetStyle();
  s.WindowRounding = 0.0f;
  s.ChildRounding = 10.0f;
  s.FrameRounding = 8.0f;
  s.GrabRounding = 8.0f;
  s.PopupRounding = 8.0f;
  s.FrameBorderSize = 0.0f;
  s.WindowPadding = ImVec2(28, 20);
  s.FramePadding = ImVec2(12, 7);
  s.ItemSpacing = ImVec2(9, 9);
  s.CellPadding = ImVec2(10, 6);
  s.ScrollbarSize = 12.0f;

  ImVec4* c = s.Colors;
  c[ImGuiCol_WindowBg]      = ImColor(IM_COL32(0x0E, 0x11, 0x16, 0xFF));
  c[ImGuiCol_ChildBg]       = ImColor(IM_COL32(0x0E, 0x11, 0x16, 0xFF));
  c[ImGuiCol_PopupBg]       = ImColor(IM_COL32(0x1F, 0x25, 0x30, 0xFF));
  c[ImGuiCol_Text]          = ImColor(col::text);
  c[ImGuiCol_TextDisabled]  = ImColor(col::muted);
  c[ImGuiCol_CheckMark]     = ImColor(col::accent);
  c[ImGuiCol_Border]        = ImColor(IM_COL32(0x23, 0x28, 0x33, 0xFF));
  c[ImGuiCol_FrameBg]       = ImColor(IM_COL32(0x16, 0x1A, 0x22, 0xFF));
  c[ImGuiCol_FrameBgHovered]= ImColor(IM_COL32(0x1F, 0x25, 0x30, 0xFF));
  c[ImGuiCol_FrameBgActive] = ImColor(IM_COL32(0x1F, 0x25, 0x30, 0xFF));
  c[ImGuiCol_Button]        = ImColor(IM_COL32(0x1F, 0x25, 0x30, 0xFF));
  c[ImGuiCol_ButtonHovered] = ImColor(IM_COL32(0x2A, 0x32, 0x41, 0xFF));
  c[ImGuiCol_ButtonActive]  = ImColor(IM_COL32(0x33, 0x3D, 0x4E, 0xFF));
  c[ImGuiCol_Header]        = ImColor(IM_COL32(0x17, 0x33, 0x24, 0xFF));
  c[ImGuiCol_HeaderHovered] = ImColor(IM_COL32(0x1C, 0x3B, 0x2B, 0xFF));
  c[ImGuiCol_HeaderActive]  = ImColor(IM_COL32(0x20, 0x42, 0x30, 0xFF));
  c[ImGuiCol_TableHeaderBg] = ImColor(IM_COL32(0x12, 0x15, 0x1C, 0xFF));
  c[ImGuiCol_TableRowBg]    = ImColor(IM_COL32(0x0E, 0x11, 0x16, 0xFF));
  c[ImGuiCol_TableRowBgAlt] = ImColor(IM_COL32(0x11, 0x14, 0x1B, 0xFF));
  c[ImGuiCol_TableBorderLight] = ImColor(IM_COL32(0x1C, 0x20, 0x29, 0xFF));
  c[ImGuiCol_ScrollbarBg]   = ImColor(IM_COL32(0x00, 0x00, 0x00, 0x00));
  c[ImGuiCol_ScrollbarGrab] = ImColor(IM_COL32(0x2A, 0x31, 0x40, 0xFF));
}

// ---- D3D11 boilerplate ----------------------------------------------------
static bool CreateDeviceD3D(HWND hWnd) {
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0,
                                       D3D_FEATURE_LEVEL_10_0 };
  D3D_FEATURE_LEVEL fl;
  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
      D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl,
      &g_pd3dDeviceContext);
  if (hr == DXGI_ERROR_UNSUPPORTED)
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl,
        &g_pd3dDeviceContext);
  if (hr != S_OK) return false;

  CreateRenderTarget();
  return true;
}

static void CleanupDeviceD3D() {
  CleanupRenderTarget();
  if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
  if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
  if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
  ID3D11Texture2D* pBackBuffer = nullptr;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  if (pBackBuffer) {
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
  }
}

static void CleanupRenderTarget() {
  if (g_mainRenderTargetView) {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = nullptr;
  }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
  switch (msg) {
    case WM_NCCALCSIZE:
      if (wParam) {
        // Strip the standard title bar / borders: the client area becomes the whole
        // window. When maximized, inset by the frame so content isn't clipped off-screen.
        if (::IsZoomed(hWnd)) {
          NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
          const int fx = ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER);
          const int fy = ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER);
          p->rgrc[0].left += fx;
          p->rgrc[0].right -= fx;
          p->rgrc[0].top += fy;
          p->rgrc[0].bottom -= fy;
        }
        return 0;
      }
      break;
    case WM_NCHITTEST: {
      const int border = ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER);
      RECT rc;
      ::GetWindowRect(hWnd, &rc);
      const int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
      if (!::IsZoomed(hWnd)) {
        const bool l = x < rc.left + border, r = x >= rc.right - border;
        const bool t = y < rc.top + border, b = y >= rc.bottom - border;
        if (t && l) return HTTOPLEFT;
        if (t && r) return HTTOPRIGHT;
        if (b && l) return HTBOTTOMLEFT;
        if (b && r) return HTBOTTOMRIGHT;
        if (l) return HTLEFT;
        if (r) return HTRIGHT;
        if (t) return HTTOP;
        if (b) return HTBOTTOM;
      }
      const int headerPx = static_cast<int>(kHeaderH * S);
      const int btnsW = static_cast<int>(46 * 3 * S);
      if (y < rc.top + headerPx && x < rc.right - btnsW) return HTCAPTION;
      return HTCLIENT;
    }
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED) return 0;
      g_ResizeWidth = (UINT)LOWORD(lParam);
      g_ResizeHeight = (UINT)HIWORD(lParam);
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      return 0;
  }
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
