# amu_updater.exe - the standalone self-updater. Kept tiny and dependency-free
# (pure Win32 + amucore's updatecheck helpers); it must run while amu.exe is
# being replaced, so it can never link against sciter.dll or the UI.
add_executable(amu_updater WIN32 main.cpp)
target_link_libraries(amu_updater PRIVATE amucore)
target_compile_options(amu_updater PRIVATE /W3)
set_target_properties(amu_updater PROPERTIES OUTPUT_NAME "amu_updater")

# Ship next to the app's lib\ folder (the same place the AutoIt updater lived).
add_custom_command(TARGET amu_updater POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:amu>/lib
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:amu_updater> $<TARGET_FILE_DIR:amu>/lib/amu_updater.exe)
