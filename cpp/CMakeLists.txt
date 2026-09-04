cmake_minimum_required(VERSION 3.21)
project(amu VERSION 2.3.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  # /guard:cf + /GUARD:CF: Control-Flow Guard for every target built here,
  # including the FetchContent'd SQLite/zlib (add_compile_options applies to
  # the subdirectories added below). DYNAMICBASE/HIGHENTROPYVA/NXCOMPAT are the
  # x64 defaults, spelled out so a toolchain change cannot silently drop them.
  # Deliberately NOT /CETCOMPAT: sciter.dll runs in-process and is not ours; a
  # shadow-stack violation inside it would kill the app on CET-capable CPUs.
  # Besides the hardening itself this trims the "unsigned + no mitigations"
  # signals that ML scanners score (see the SignPath/VirusTotal notes).
  add_compile_options(/utf-8 /guard:cf)
  add_link_options(/GUARD:CF /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(FetchContent)

# FreeType: crisp font rasterization (large readability upgrade over imgui's stb rasterizer).
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG VER-2-13-3
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(freetype)

# Dear ImGui has no upstream CMakeLists -> fetch sources and build a small static lib.
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/freetype)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi imm32 freetype)

# doctest (has its own CMakeLists -> provides doctest::doctest)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE)
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

# Sciter.JS SDK: the GUI engine (HTML/CSS UI with native DirectWrite text).
# We use exactly three things from it: the headers, sciter.dll (x64) and
# packfolder.exe. The full archive also ships ~50 demo binaries for three
# architectures, and antivirus ML heuristics flag some of them (Sophos
# quarantines bin/windows/x86/window-mixin.exe) - a quarantined file makes
# CMake's extract step fail the whole configure with "Access is denied".
# So: download without extracting, then unpack only the subset we need. This
# also keeps the build tree small and the configure step fast.
FetchContent_Declare(sciterjs
  URL https://gitlab.com/sciter-engine/sciter-js-sdk/-/archive/main/sciter-js-sdk-main.zip
  DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(sciterjs)

set(SCITER_SDK_DIR ${sciterjs_SOURCE_DIR}/sdk)
if(NOT EXISTS ${SCITER_SDK_DIR}/include/sciter-x.h)
  file(GLOB SCITER_SDK_ZIP ${sciterjs_SOURCE_DIR}/*.zip)
  if(NOT SCITER_SDK_ZIP)
    message(FATAL_ERROR "Sciter SDK archive not found in ${sciterjs_SOURCE_DIR}")
  endif()
  message(STATUS "Unpacking the Sciter SDK subset (headers, sciter.dll, packfolder.exe)")
  file(ARCHIVE_EXTRACT
       INPUT ${SCITER_SDK_ZIP}
       DESTINATION ${sciterjs_SOURCE_DIR}/unpack
       PATTERNS "sciter-js-sdk-main/include/*"
                "sciter-js-sdk-main/bin/windows/x64/sciter.dll"
                "sciter-js-sdk-main/bin/windows/packfolder.exe")
  file(RENAME ${sciterjs_SOURCE_DIR}/unpack/sciter-js-sdk-main ${SCITER_SDK_DIR})
  file(REMOVE_RECURSE ${sciterjs_SOURCE_DIR}/unpack)
endif()

# zlib (static) for the ARK .z decompressor, linked into amucore.
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG v1.3.1
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(zlib)

# SQLite amalgamation (single-file C source) compiled directly into amucore.
# No CMakeLists inside the zip, so MakeAvailable just populates the tree and
# amucore/CMakeLists.txt adds ${sqlite_SOURCE_DIR}/sqlite3.c to its sources.
FetchContent_Declare(sqlite
  URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sqlite)

enable_testing()

add_subdirectory(amucore)
add_subdirectory(amu)
add_subdirectory(updater)
add_subdirectory(tests)
