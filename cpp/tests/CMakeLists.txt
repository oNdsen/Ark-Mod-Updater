add_executable(amu_tests
  test_smoke.cpp
  test_steamcmd_parser.cpp
  test_mod_writer.cpp
  test_zunpack.cpp
  test_serverconfig.cpp
  test_workshop.cpp
  test_rcon.cpp
  test_credstore.cpp
  test_launchspec.cpp
  test_supervisor.cpp
  test_orchestrator.cpp
  test_updatecheck.cpp
  test_db.cpp
  test_warnplan.cpp
  test_schedule.cpp
  test_worldbackup.cpp
)
target_link_libraries(amu_tests PRIVATE amucore doctest::doctest)
target_include_directories(amu_tests PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})  # test_worldbackup.cpp inflates with zlib
target_link_libraries(amu_tests PRIVATE zlibstatic)
target_compile_features(amu_tests PRIVATE cxx_std_20)
target_compile_definitions(amu_tests PRIVATE
  AMU_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME amu_tests COMMAND amu_tests)
