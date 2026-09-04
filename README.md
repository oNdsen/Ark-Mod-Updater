<div align="center">

# 🦖 AMU — ARK Mod Updater

**The all-in-one manager for ARK dedicated servers on Windows** —
install & update Workshop mods, launch & supervise your servers,
and edit every last server setting. One small native app.

[![Latest release](https://img.shields.io/github/v/release/oNdsen/ark-mod-updater?style=for-the-badge&logo=github&color=3DDC84)](https://github.com/oNdsen/ark-mod-updater/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/oNdsen/ark-mod-updater/total?style=for-the-badge&color=3DDC84)](https://github.com/oNdsen/ark-mod-updater/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue?style=for-the-badge)](LICENSE)
[![Stars](https://img.shields.io/github/stars/oNdsen/ark-mod-updater?style=for-the-badge&color=F5A623)](https://github.com/oNdsen/ark-mod-updater/stargazers)

[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6?logo=windows&logoColor=white)](#-building)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](#-architecture)
[![UI: Sciter.JS](https://img.shields.io/badge/UI-Sciter.JS-8A2BE2)](https://sciter.com)
[![Tests](https://img.shields.io/badge/tests-199%20passing-3DDC84)](#-tests)
[![Issues](https://img.shields.io/github/issues/oNdsen/ark-mod-updater)](https://github.com/oNdsen/ark-mod-updater/issues)

[Download](https://github.com/oNdsen/ark-mod-updater/releases/latest) •
[Features](#-features) •
[Screenshots](#-screenshots) •
[Building](#-building) •
[License](#-license--copyright)

</div>

---

> **v2 is a ground-up C++20 rewrite** of the original AutoIt application
> (2017–2026): a tiny native binary, a fully testable core, and a hardened
> self-updater.

## What's new in v2.3

- **Automated map backups.** The world file, player profiles and tribes of a
  server go into one timestamped ZIP archive
  (`<server>_<map>_YYYY-MM-DD_HH-MM-SS.zip`, ZIP64 when it gets big) under
  `ShooterGame\Saved\Backups` or a folder of your choice, with a configurable
  rotation (default: the newest 10 are kept). Backups run **every *n* hours**
  (e.g. every 2 h) while AMU is open, **before every mod update** (on by
  default), and on **Backup now**. A running server is told to `saveworld`
  first and AMU waits for the fresh save.
- **Backup messages for your players.** Like the shutdown warnings: any
  number of RCON broadcasts with their own minute marks (`{min}`). Default:
  *Map backup in 1 minute*, then *Map backup now*. Remove every row for a
  silent backup.
- **Automation tab.** Everything AMU does on its own for a server lives on
  one tab: start with AMU, auto-restart on crash, shutdown warnings,
  scheduled update checks and map backups.
- Review round: two servers scheduled for the same minute both run (queued),
  a mod's load order can be changed with *Up* / *Down*, rolling backups of
  `GameUserSettings.ini` / `Game.ini` before every AMU write
  (`AMU-Backups`, 10 kept), and a dozen smaller fixes.

## What's new in v2.2

- **Shutdown warnings are back - and dynamic.** Before a server is stopped
  for a mod update, AMU broadcasts your messages via RCON: any number of
  steps, each with its own minute mark and text (`{min}` = minutes left),
  each one switchable. Default: 20 / 15 / 10 / 5 / 1 minutes. Remove every
  row to stop without a warning.
- **Scheduled update checks.** Any number of schedules (time of day,
  weekdays, one server or all): at that time AMU runs the same check as
  *Check for updates* and, if something is outdated, the full flow with your
  warnings. AMU has to be running at that time.

## What's new in v2.1

- The mod list shows Workshop previews, names and **Released / Updated /
  Installed** dates - also for mods that only ever lived in `ActiveMods=`.
- Smarter Mods-tab actions: *Install missing (N)*, *Check for updates*,
  *Install* / *Reinstall* per selection, and proper confirm dialogs.
- Configurator: server-then-gameplay category order with rows grouped by ini
  file, an *Only set* filter, collapsible categories, defaults and examples
  prefilled on click, scrollable family editors.
- Start / Stop / Restart follow the live server state.
- Setting definitions checked against the official wiki (`MultiHome`,
  `ModIDS`, `CustomLiveTuningUrl`).

## ✨ Features

- 🧩 **Mod install & update** — drives `steamcmd.exe` with live per-mod
  progress, unpacks ARK's `.z` archives, writes the `.mod` descriptors, and
  keeps `ActiveMods` in `GameUserSettings.ini` in sync. Names, preview images
  and release/update dates come straight from the Steam Workshop (cached
  locally under `lib/cache/previews`), and the buttons follow the state of
  your mod list - *Install missing*, *Check for updates*, *Reinstall*.
- 🚀 **Server launcher & supervisor** — AMU builds the server command line
  itself (no start scripts): start/stop/restart from the UI, graceful RCON
  shutdown (`saveworld` → confirmed fresh save → `DoExit`), crash detection
  with auto-restart & backoff, adoption of already-running servers, live
  status in the sidebar.
- 📡 **Update orchestration** — configurable countdown broadcasts to your
  players (any number of steps, `{min}` templating), scheduled update checks,
  a map backup before the install, all-or-nothing mod swap, and the server
  only restarts if it was running before.
- 🗜️ **Map backups** — timestamped ZIP archives of the world, profiles and
  tribes: every *n* hours, before every update and on demand, with rotation
  and RCON messages for your players.
- ⚙️ **Full server configurator** — ~370 settings for **ASE and ASA** from
  the community wiki, searchable, categorized and collapsible, with an
  *Only set* filter, typed editors, tooltips, click-to-edit defaults, and raw
  editors for the array-style setting families
  (`OverrideNamedEngramEntries`, `PerLevelStatsMultiplier[i]`, …) that
  normal INI tools cannot touch. Unrelated lines, comments and encoding are
  preserved byte-for-byte.
- 🔄 **Hardened self-updater** — updates ship as GitHub Releases with a
  SHA-256 manifest; the standalone `amu_updater.exe` verifies every file
  **before** swapping, with automatic rollback. AMU even bootstraps a
  missing SteamCMD by itself.
- 🔐 **DPAPI credential store** — Steam login data is encrypted per Windows
  user; nothing is ever stored in plain text.
- 🗃️ Servers, mods, settings and logs live in a local SQLite database.

## 📸 Screenshots

| Mods | Server & launch options |
| :---: | :---: |
| ![Mods view](docs/screenshots/mods.png) | ![Server view](docs/screenshots/server.png) |
| **Configurator** | **Live update run** |
| ![Configurator](docs/screenshots/config.png) | ![Update run](docs/screenshots/update.png) |

## 🏗 Architecture

Two layers, cleanly separated so the entire core is testable without a GUI:

```
┌─────────────────────────────┐
│  amu.exe (Sciter.JS shell)  │  HTML/CSS/JS UI, packed into the exe
├─────────────────────────────┤
│  amucore (static lib, C++20)│  everything below is unit-tested
└─────────────────────────────┘
```

| `amucore` module | Purpose |
| --- | --- |
| `db` | SQLite data layer (prepared statements only) |
| `serverconfig` | `GameUserSettings.ini` / `Game.ini` access incl. the raw rewriter for array-setting families |
| `launchspec` | Pure ASE/ASA command-line builder |
| `supervisor` | Process lifecycle: start, graceful RCON stop, crash auto-restart |
| `orchestrator` | The mod-update pipeline (SteamCMD → unpack → install → restart) |
| `warnplan` | Pre-shutdown warning plan: storage format, defaults, countdown steps |
| `schedule` | Scheduled update checks and backup intervals: due-time logic |
| `worldbackup` | Map backups: archive naming, include rules, rotation, the ZIP writer (zlib, ZIP64) |
| `backup` | The backup runner: player messages, `saveworld`, archive, rotation |
| `steamcmd_parser` | SteamCMD stdout → typed progress events |
| `zunpack` | ARK/Valve `.z` decompressor |
| `mod_writer` | `.mod` descriptor generator |
| `workshop` | Workshop page scraping (name, preview, posted/updated dates) |
| `rcon` | Minimal Source-RCON client |
| `credstore` | DPAPI encrypt/decrypt |
| `updatecheck` | Manifest + SHA-256 self-update client |

Plus [`cpp/updater`](cpp/updater) — the standalone `amu_updater.exe` that
applies verified updates while the app is closed.

## 🔨 Building

Requirements: **CMake ≥ 3.21**, **Ninja**, **MSVC** with C++20
(Visual Studio 2022 “Desktop development with C++” workload).
All dependencies are fetched automatically via CMake `FetchContent`
(SQLite, zlib, doctest, Sciter.JS SDK) — no manual setup.

```powershell
cd cpp
cmake --preset default
cmake --build --preset default
```

The binaries land in `cpp/build/` — `amu.exe` runs directly from there
(a loose `ui/` folder next to the exe overrides the packed-in UI for fast
HTML iteration).

### 🧪 Tests

```powershell
ctest --test-dir build --output-on-failure
# or directly: build/tests/amu_tests.exe
```

## 🔄 Updates & releases

Each [GitHub Release](https://github.com/oNdsen/ark-mod-updater/releases)
is the update channel — the app checks
`releases/latest/download/version.txt` and offers one-click updates:

- The install is just **three files**: `amu.exe` (UI packed in),
  `sciter.dll`, `lib/amu_updater.exe`.
- `manifest.txt` lists every file with its **SHA-256** — the updater
  verifies everything *before* touching your install, and rolls back on any
  failure.
- `steamcmd.exe` ships as an extra asset; AMU downloads it automatically
  when missing.
- `amu-cpp-<version>.zip` is attached for manual installs.

Maintainers: `publish.ps1` builds the whole asset set and prints the
matching `gh release create` command.

## 🙏 Credits

- [Sciter.JS](https://sciter.com) — the embedded HTML/CSS/JS UI engine (free tier)
- [Valve SteamCMD](https://developer.valvesoftware.com/wiki/SteamCMD) — Workshop downloads (Valve's software, not part of this project)
- [SQLite](https://sqlite.org) (public domain) • [zlib](https://zlib.net) (zlib license) • [doctest](https://github.com/doctest/doctest) (MIT)
- ARK server setting documentation based on the
  [ARK Community Wiki](https://ark.wiki.gg/wiki/Server_configuration)

## 📜 License & copyright

Licensed under the **[MIT License](LICENSE)** — free to use, modify and
redistribute, commercially or not. **One condition is not optional:** the
copyright notice

```
Copyright (c) 2026 oNdsen
```

**must remain** in all copies and substantial portions of the software,
including forks and derived projects (that is the retention clause of the
MIT license itself). Removing the notice is a license violation.

<div align="center">

---

**© 2026 [oNdsen](https://ondsen.ch)** — made with 🦖 for the ARK community

⭐ *If AMU saves you time, a star keeps the dinos fed.*

</div>
