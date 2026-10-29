# C11 requires an SDK that supports the conforming preprocessor. Visual Studio
# reports the selected SDK version; other generators check the actual headers.
if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION AND
   CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS "10.0.20348.0")
    message(FATAL_ERROR
        "C11 requires Windows SDK 10.0.20348.0 or newer; selected "
        "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}. Install and select a newer SDK.")
endif()

include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

block(SCOPE_FOR VARIABLES)
    cmake_push_check_state(RESET)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    if(CODUOMP_CLANG_CL)
        # SDK headers supplied through /imsvc are otherwise warning-silent.
        set(CMAKE_REQUIRED_FLAGS "/clang:-Wsystem-headers /clang:-Werror=expansion-to-defined")
    else()
        set(CMAKE_REQUIRED_FLAGS "/we5105")
    endif()
    check_c_source_compiles([[
        #define WIN32_LEAN_AND_MEAN
        #include <winsock2.h>
        #include <windows.h>
        int main(void) { return 0; }
    ]] CODUOMP_WINDOWS_SDK_C11_SUPPORTED)
    cmake_pop_check_state()
endblock()
if(NOT CODUOMP_WINDOWS_SDK_C11_SUPPORTED)
    message(FATAL_ERROR
        "The Windows SDK headers failed the C11 preprocessor check. "
        "Use Windows SDK 10.0.20348.0 or newer and check the CMake configure log.")
endif()

# Some MSVC releases report C5287 even after both enum operands are explicitly
# converted to int. Keep the warning enabled for each language whose compiler
# accepts that conversion; do not change recovered constants or layouts to
# work around a compiler diagnostic bug.
if(NOT CODUOMP_CLANG_CL)
    block(SCOPE_FOR VARIABLES)
        cmake_push_check_state(RESET)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        set(CMAKE_REQUIRED_FLAGS "/we5287")
        set(_enum_cast_check [[
            enum { first_limit = 1 };
            enum { second_limit = 2 };
            typedef char limits_ordered[(int)first_limit < (int)second_limit ? 1 : -1];
            int main(void) { return 0; }
        ]])
        check_c_source_compiles("${_enum_cast_check}" CODUOMP_C_ENUM_CASTS_WARNING_CLEAN)
        check_cxx_source_compiles("${_enum_cast_check}" CODUOMP_CXX_ENUM_CASTS_WARNING_CLEAN)
        cmake_pop_check_state()
    endblock()
    foreach(_language C CXX)
        if(NOT CODUOMP_${_language}_ENUM_CASTS_WARNING_CLEAN)
            message(STATUS "MSVC ${_language}: retaining the C5287 enum-cast diagnostic workaround")
        endif()
    endforeach()
endif()
