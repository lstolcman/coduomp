# MSVC client and module build. Makefile targets remain authoritative on other
# toolchains; source ownership comes from their shared build-mk manifests.
#
# See docs/client-building.md for x86 Miles input and architecture selection.
# cmake --preset msvc -DMSS32_DLL=C:/Games/CoDUO/mss32.dll
# cmake --build --preset msvc
# The msvc preset selects x86; msvc-x64 explicitly selects the 64-bit port.
cmake_minimum_required(VERSION 3.24)

# Keep generated sources, downloads, and outputs below the root workbench.
cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${CMAKE_BINARY_DIR}" NORMALIZE _in_source)
set(_workbench "${CMAKE_SOURCE_DIR}/.workbench")
cmake_path(IS_PREFIX _workbench "${CMAKE_BINARY_DIR}" NORMALIZE _in_workbench)
if(_in_source AND NOT _in_workbench)
    message(FATAL_ERROR "Use -B .workbench/build/msvc for an in-repository build.")
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Single-configuration build type" FORCE)
endif()
project(CoDUOMP LANGUAGES C CXX RC)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CODUOMP_BUILD_CLIENT "Build the client executable" ON)
option(CODUOMP_BUILD_MODULES "Build cgame, UI, and game modules" ON)
option(CODUOMP_STATIC_VCRT "Link the MSVC runtime statically" ON)
option(BUNDLED_LIBS "Build pinned third-party dependencies" ON)
option(CODUOMP_CURL_STATIC "Link a static system libcurl (bundled curl is always static)" ON)
option(CODUOMP_RENDERER_GPU_PROFILE "Compile renderer GPU timing" OFF)
set(CODUOMP_DEP_PREFIX "" CACHE PATH "Prefix containing MSVC dependencies")
# The POSIX dedicated server remains a Makefile/MinGW target.
if(CODUOMP_BUILD_SERVER)
    message(FATAL_ERROR "MSVC dedicated-server support is not implemented; use the Makefile target.")
endif()

include(cmake/CoduompManifest.cmake)
include(cmake/CoduompCommon.cmake)
include(cmake/CoduompPlatform.cmake)
include(cmake/CoduompSoftFloat.cmake)
include(cmake/CoduompBundledLibs.cmake)
include(cmake/CoduompDependencies.cmake)
include(cmake/CoduompAudio.cmake)
include(cmake/CoduompSources.cmake)
include(cmake/CoduompTargets.cmake)
message(STATUS "MSVC ${CODUOMP_ARCH}; ${CODUOMP_FLOAT_MODEL}")
message(STATUS "Output: ${CODUOMP_BIN_ROOT}/<config>")
