set(NEXUS_CORE_SOURCES
  core/data_paths.cpp
  core/url_utils.cpp
)

add_library(nexus_core STATIC ${NEXUS_CORE_SOURCES})
target_include_directories(nexus_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nexus_core PUBLIC shell32.lib ole32.lib)
target_compile_options(nexus_core PRIVATE /W4 /WX /utf-8 /EHsc)

set(NEXUS_NATIVE_SOURCES
  src/app.cpp
  src/asset_store.cpp
  src/browser_window.cpp
  src/content_client.cpp
  src/ipc_router.cpp
  src/main.cpp
  src/privacy_service.cpp
  src/render_process_handler.cpp
  src/scheme_handler.cpp
  src/settings.cpp
  src/shell_client.cpp
)

add_library(nexus SHARED ${NEXUS_NATIVE_SOURCES})
SET_LIBRARY_TARGET_PROPERTIES(nexus)
target_include_directories(nexus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(nexus PRIVATE /W4 /WX /utf-8 /EHsc)
target_compile_definitions(nexus PRIVATE NEXUS_VERSION="${PROJECT_VERSION}")

# CEF's import library. Debug builds would need the Debug CEF distribution,
# which the pinned minimal archive does not contain.
ADD_LOGICAL_TARGET(libcef_lib "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

add_dependencies(nexus libcef_dll_wrapper)
target_link_libraries(nexus PRIVATE
  nexus_core
  libcef_lib
  libcef_dll_wrapper
  ${CEF_STANDARD_LIBS}
  dwmapi.lib
)

set_target_properties(nexus PROPERTIES
  OUTPUT_NAME "nexus"
  PREFIX ""
  SUFFIX ".dll"
  RUNTIME_OUTPUT_DIRECTORY "${NEXUS_OUTPUT_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY "${NEXUS_OUTPUT_DIR}"
  PDB_OUTPUT_DIRECTORY "${NEXUS_OUTPUT_DIR}"
)

# The CEF sandbox on Windows requires the bootstrap executable. It loads the
# client DLL whose name matches the EXE stem (nexus.exe -> nexus.dll) from the
# same directory, initializes the sandbox broker, and calls RunWinMain.
add_custom_command(TARGET nexus POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CEF_BINARY_DIR_RELEASE}/bootstrap.exe"
          "$<TARGET_FILE_DIR:nexus>/nexus.exe"
  COMMENT "Staging CEF bootstrap as nexus.exe"
  VERBATIM
)

COPY_FILES(nexus "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${NEXUS_OUTPUT_DIR}")
COPY_FILES(nexus "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${NEXUS_OUTPUT_DIR}")

# The Windows sandbox uses a low-privilege app container (LPAC) for renderer
# and GPU processes. Grant the AppContainer SID read/execute access.
add_custom_command(TARGET nexus POST_BUILD
  COMMAND icacls "$<TARGET_FILE_DIR:nexus>" /grant "*S-1-15-2-2:(OI)(CI)(RX)"
  COMMENT "Granting LPAC read/execute access to the runtime directory"
  VERBATIM
)

# The React shell and the local new-tab page are served by the nexus:// scheme
# handler from <exe>/ui. Staging is a build graph target so frontend edits are
# picked up without relinking the DLL.
set(NEXUS_FRONTEND_DIST "${CMAKE_CURRENT_SOURCE_DIR}/../frontend/dist" CACHE PATH
    "Built React shell assets")
if(NEXUS_BUILD_FRONTEND AND NOT EXISTS "${NEXUS_FRONTEND_DIST}/index.html")
  message(FATAL_ERROR
    "Frontend assets not found at ${NEXUS_FRONTEND_DIST}.\n"
    "Run: corepack pnpm --dir apps/desktop/frontend build")
endif()

if(EXISTS "${NEXUS_FRONTEND_DIST}/index.html")
  add_custom_target(nexus_ui ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${NEXUS_OUTPUT_DIR}/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${NEXUS_FRONTEND_DIST}" "${NEXUS_OUTPUT_DIR}/ui"
    COMMENT "Staging NEXUS shell assets"
    VERBATIM
  )
  add_dependencies(nexus nexus_ui)
else()
  message(WARNING
    "NEXUS frontend assets are missing; the application will not start "
    "until ${NEXUS_OUTPUT_DIR}/ui/index.html exists.")
endif()

# Native unit tests (no CEF message loop required).
add_executable(nexus_core_tests tests/core_tests.cpp)
target_link_libraries(nexus_core_tests PRIVATE nexus_core)
target_compile_options(nexus_core_tests PRIVATE /W4 /WX /utf-8 /EHsc)
set_target_properties(nexus_core_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${NEXUS_OUTPUT_DIR}"
)
add_test(NAME nexus_core_tests COMMAND nexus_core_tests)
