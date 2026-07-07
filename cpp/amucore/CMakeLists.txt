add_library(amucore STATIC
  src/version.cpp
  src/steamcmd_parser.cpp
  src/mod_writer.cpp
  src/zunpack.cpp
  src/serverconfig.cpp
  src/workshop.cpp
  src/rcon.cpp
  src/credstore.cpp
  src/launchspec.cpp
  src/supervisor.cpp
  src/orchestrator.cpp
  src/updatecheck.cpp
  src/db.cpp
  ${sqlite_SOURCE_DIR}/sqlite3.c
)
target_include_directories(amucore PUBLIC include)
target_include_directories(amucore PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} ${sqlite_SOURCE_DIR})
target_link_libraries(amucore PRIVATE zlibstatic)
if(WIN32)
  # winhttp: workshop scrape + update check; ws2_32: RCON; crypt32: DPAPI;
  # bcrypt: SHA-256 (update manifest verification).
  target_link_libraries(amucore PRIVATE winhttp ws2_32 crypt32 bcrypt)
endif()
target_compile_features(amucore PUBLIC cxx_std_20)

# The SQLite amalgamation is C; silence its (many) benign warnings so a strict
# amucore build is not derailed by third-party code we do not maintain.
if(MSVC)
  set_source_files_properties(${sqlite_SOURCE_DIR}/sqlite3.c PROPERTIES COMPILE_FLAGS "/w")
endif()
