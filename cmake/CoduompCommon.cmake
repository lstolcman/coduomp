#-----------------------------------------------------------------
# Common build settings
#-----------------------------------------------------------------

# The native (GCC/Clang) generator builds the recovered dedicated server with
# the same flags and x87 policy as build-mk/server.mk. MSVC generates the Visual
# Studio solution. The client executable and its modules are MSVC-only.
if(MSVC)
	set(CODUOMP_NATIVE FALSE)
else()
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR
			"CoDUOMP: the native CMake build supports Linux. "
			"Use the Makefile targets for the other platforms.")
	endif()
	set(CODUOMP_NATIVE TRUE)
endif()

# Language levels used by the Makefile targets: C11 everywhere, C++11 for the
# native server (build-mk/server.mk) and C++17 for the MSVC client.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(CODUOMP_NATIVE)
	set(CMAKE_CXX_STANDARD 11)
else()
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Retail modules link the CRT statically, so keep that property here too.
if(MSVC AND CODUOMP_STATIC_VCRT)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

#-----------------------------------------------------------------
# Output layout
#
# Everything stays inside the CMake binary directory (build/ in the documented
# invocation, which .gitignore already excludes). The executable and its
# modules are staged together because the engine resolves cgame/ui/game from
# <basepath>/uo/.
#-----------------------------------------------------------------
set(CODUOMP_BIN_ROOT "${CMAKE_BINARY_DIR}/bin")
set(CODUOMP_LIB_ROOT "${CMAKE_BINARY_DIR}/lib")

#-----------------------------------------------------------------
# Helpers
#-----------------------------------------------------------------

# coduomp_target_output(<target> [<subdir>])
#
# Stage the target in bin/<config>[/<subdir>] and its import library in
# lib/<config>, keeping the layout of every configuration predictable. The
# runtime directory also receives native shared libraries, which CMake keeps in
# LIBRARY_OUTPUT_DIRECTORY rather than RUNTIME_OUTPUT_DIRECTORY.
function(coduomp_target_output target)
	set(_subdir "${ARGN}")
	if(_subdir)
		set(_runtime "${CODUOMP_BIN_ROOT}/$<CONFIG>/${_subdir}")
	else()
		set(_runtime "${CODUOMP_BIN_ROOT}/$<CONFIG>")
	endif()
	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${_runtime}"
		LIBRARY_OUTPUT_DIRECTORY "${_runtime}"
		ARCHIVE_OUTPUT_DIRECTORY "${CODUOMP_LIB_ROOT}/$<CONFIG>")
	set(${target}_RUNTIME_DIR "${_runtime}" PARENT_SCOPE)
endfunction()

# coduomp_exclude_from_build(<target>)
#
# Clear the Visual Studio "Build" checkbox for a target whose dependencies are
# unavailable, so MSBuild skips it while the project stays in the solution.
function(coduomp_exclude_from_build target)
	set_target_properties(${target} PROPERTIES
		EXCLUDE_FROM_DEFAULT_BUILD TRUE)
endfunction()

# coduomp_apply_common(<target>)
#
# Project-target settings that must not leak into the bundled third-party
# libraries. On Windows the MinGW targets receive POSIX spellings (strcasecmp,
# ...) from the MinGW C library, so the forced include supplies the Microsoft
# CRT names without touching the recovered sources. The native build compiles
# the same sources with the warning level, integer/enum model, and x87 flags of
# build-mk/server.mk.
function(coduomp_apply_common target)
	if(MSVC)
		target_compile_options(${target} PRIVATE
			# The recovered headers compare MAX_* constants that live in different
			# unnamed enums inside compile-time assertions; C5287 is a style
			# diagnostic about those intentionally mixed domains.
			/wd5287
			"SHELL:/FI\"${CMAKE_SOURCE_DIR}/cmake/CoduompMsvcCompat.h\"")
	else()
		target_compile_options(${target} PRIVATE
			-Wall -Wextra -Werror -pedantic
			-fsigned-char -fno-short-enums -fwrapv
			${CODUOMP_FLOAT_FLAGS}
			# The scripting sources are C++; the Makefile target compiles them
			# with -Wno-pedantic because the recovered headers use C idioms.
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-pedantic>
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-c99-extensions>)
		target_compile_definitions(${target} PRIVATE
			${CODUOMP_PLATFORM_DEFINES})
	endif()
endfunction()

# coduomp_absolute_sources(<out-var> <source>...)
function(coduomp_absolute_sources out_var)
	set(_absolute "")
	foreach(_source IN LISTS ARGN)
		if(IS_ABSOLUTE "${_source}")
			set(_path "${_source}")
		else()
			set(_path "${CMAKE_SOURCE_DIR}/${_source}")
		endif()
		cmake_path(NORMAL_PATH _path)
		list(APPEND _absolute "${_path}")
	endforeach()
	list(REMOVE_DUPLICATES _absolute)
	set(${out_var} "${_absolute}" PARENT_SCOPE)
endfunction()

# coduomp_group_sources(<source>...)
#
# Recreate the source tree in the Visual Studio filters.
function(coduomp_group_sources)
	if(ARGN)
		source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${ARGN})
	endif()
endfunction()
