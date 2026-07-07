# AMU C++ sources

See the [main README](../README.md) at the repository root for the project
overview, architecture, and build instructions.

Layout:

| Directory  | Contents |
|------------|----------|
| `amucore/` | UI-independent core library (db, supervisor, orchestrator, serverconfig, ...) |
| `amu/`     | The Sciter.JS desktop app (`main_sciter.cpp` + `ui/main.html`) |
| `updater/` | Standalone self-updater (`amu_updater.exe`) |
| `tests/`   | doctest unit tests (`amu_tests`) |

Build: `cmake --preset default && cmake --build --preset default` (CMake ≥ 3.21,
Ninja, MSVC with C++20; dependencies are fetched via FetchContent).
