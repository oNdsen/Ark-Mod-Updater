# --- amu: the Sciter.JS shell (primary GUI) --------------------------------

# Pack ui/ into a Sciter resource blob compiled INTO amu.exe (loaded via
# this://app/ URLs). A loose ui\ folder next to the exe still wins at runtime
# (dev mode: HTML edits need no recompile) - end users need no ui folder.
# sciter.dll deliberately stays a separate file: static linking is a paid
# Sciter tier, and embedding+extracting a DLL is the AV-heuristic pattern the
# AutoIt app was cured of.
set(AMU_UI_RES ${CMAKE_CURRENT_BINARY_DIR}/ui_resources.cpp)
file(GLOB_RECURSE AMU_UI_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ui/*)
add_custom_command(OUTPUT ${AMU_UI_RES}
  COMMAND ${sciterjs_SOURCE_DIR}/bin/windows/packfolder.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/ui ${AMU_UI_RES} -v resources
  DEPENDS ${AMU_UI_FILES}
  COMMENT "Packing ui/ into amu.exe (Sciter resources)"
  VERBATIM)

add_executable(amu WIN32
  main_sciter.cpp
  ${sciterjs_SOURCE_DIR}/include/sciter-main.cpp
)
# The generated array is #included by main_sciter.cpp (not compiled standalone).
set_source_files_properties(main_sciter.cpp PROPERTIES OBJECT_DEPENDS ${AMU_UI_RES})
target_include_directories(amu PRIVATE ${sciterjs_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(amu PRIVATE SKIP_MAIN)
target_link_libraries(amu PRIVATE amucore shell32)
target_compile_features(amu PRIVATE cxx_std_20)
if(MSVC)
  # Sciter SDK headers are not /W4-clean; keep our own warnings via /W3 here.
  target_compile_options(amu PRIVATE /utf-8 /W3)
endif()

# Ship sciter.dll and the HTML/CSS UI next to amu.exe.
add_custom_command(TARGET amu POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${sciterjs_SOURCE_DIR}/bin/windows/x64/sciter.dll"
          "$<TARGET_FILE_DIR:amu>/sciter.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/ui" "$<TARGET_FILE_DIR:amu>/ui"
  VERBATIM)

# --- amu_imgui: the previous Dear ImGui shell, kept as a fallback ----------
add_executable(amu_imgui WIN32 EXCLUDE_FROM_ALL
  main.cpp
)
target_link_libraries(amu_imgui PRIVATE amucore imgui shell32)
target_compile_features(amu_imgui PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(amu_imgui PRIVATE /utf-8 /W4 /permissive-)
endif()
add_custom_command(TARGET amu_imgui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:amu_imgui>/assets"
  VERBATIM)
