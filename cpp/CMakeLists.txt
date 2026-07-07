cmake_minimum_required(VERSION 3.21)
project(amu VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8)
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

# Sciter.JS SDK: headers + prebuilt sciter.dll (the GUI engine; HTML/CSS UI,
# native DirectWrite text). Fetched as a source archive - no CMakeLists inside,
# so MakeAvailable just populates the tree.
FetchContent_Declare(sciterjs
  URL https://gitlab.com/sciter-engine/sciter-js-sdk/-/archive/main/sciter-js-sdk-main.zip)
FetchContent_MakeAvailable(sciterjs)

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
