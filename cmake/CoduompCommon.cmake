#-----------------------------------------------------------------
# Common build settings
#-----------------------------------------------------------------

set(CODUOMP_CLANG_CL FALSE)
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"
   AND CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS 22 OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 22)
        message(FATAL_ERROR "The clang-cl x64 profile requires Clang 22 or newer.")
    endif()
    set(CODUOMP_CLANG_CL TRUE)
    # Keep ordinary float/double calls on the Windows ABI. Global extended
    # evaluation mispasses variadic doubles in the tested Clang releases.
    set(CODUOMP_EXTENDED_FLAGS /fp:strict /clang:-mlong-double-80
        /clang:-ffp-eval-method=source /clang:-ffp-contract=off)
elseif(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR MSVC_VERSION LESS 1939)
    message(FATAL_ERROR
        "This CMake project requires MSVC 19.39+ or clang-cl 22+ for x64. "
        "Use the Makefile targets for other toolchains.")
endif()

# Language levels used by the Makefile targets (C11/C17).
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Retail modules link the CRT statically, so keep that property here too.
if(CODUOMP_STATIC_VCRT)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

#-----------------------------------------------------------------
# Output layout
#
# Everything stays inside the CMake binary directory (.workbench/build/msvc in the documented
# invocation). The executable and its
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
# lib/<config>, keeping the layout of every configuration predictable.
function(coduomp_target_output target)
	set(_subdir "${ARGN}")
	if(_subdir)
		set(_runtime "${CODUOMP_BIN_ROOT}/$<CONFIG>/${_subdir}")
	else()
		set(_runtime "${CODUOMP_BIN_ROOT}/$<CONFIG>")
	endif()
	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${_runtime}"
		ARCHIVE_OUTPUT_DIRECTORY "${CODUOMP_LIB_ROOT}/$<CONFIG>")
	set(${target}_RUNTIME_DIR "${_runtime}" PARENT_SCOPE)
endfunction()

# coduomp_apply_common(<target>)
#
# Keep compiler policy on product targets; dependency projects own their flags.
function(coduomp_apply_common target)
    target_compile_options(${target} PRIVATE
        /W1 /utf-8 /wd5287 /Od
        # Script error propagation deliberately crosses extern "C" entries.
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc->)
    if(CODUOMP_32BIT_X86)
        target_compile_options(${target} PRIVATE /arch:IA32)
    endif()
    target_compile_definitions(${target} PRIVATE
        _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
        _WINSOCK_DEPRECATED_NO_WARNINGS)
    if(CODUOMP_CLANG_CL)
        foreach(_flag IN LISTS CODUOMP_EXTENDED_FLAGS)
            target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:${_flag}>")
        endforeach()
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C,CXX>:/clang:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>")
        target_include_directories(${target} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/src/compat/clangcl")
    endif()
    target_link_libraries(${target} PRIVATE coduomp_softfloat)
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
