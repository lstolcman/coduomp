include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

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
