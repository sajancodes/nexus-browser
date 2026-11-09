cmake_minimum_required(VERSION 3.25)

project(NEXUS VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32)
  message(FATAL_ERROR "NEXUS Milestone 1 supports Windows x64 only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "NEXUS requires a 64-bit toolchain (x64).")
endif()

# CEF 156 is built against the static MSVC runtime (/MT). Matching it avoids
# cross-runtime allocation and CRT-state bugs across the DLL boundary.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The CEF binary distribution is never committed. Fetch it with
# `python scripts/fetch_cef.py` before configuring.
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef/binary" CACHE PATH
    "Path to the CEF binary distribution")
if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
  message(FATAL_ERROR
    "CEF binary distribution not found at ${CEF_ROOT}.\n"
    "Run: python scripts/fetch_cef.py")
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# Build the CEF C++ wrapper once. Building the whole wrapper is slow, so keep
# it out of the default target graph until something links it.
add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper EXCLUDE_FROM_ALL)

set(NEXUS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH
    "Directory containing the runnable application and CEF runtime")

option(NEXUS_BUILD_FRONTEND
       "Require the built React frontend assets when configuring" ON)
enable_testing()

add_subdirectory(apps/desktop/native)

PRINT_CEF_CONFIG()
