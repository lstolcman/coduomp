# Microsoft CRT long-double entry points use binary64. clang-cl's x87 target
# supplies the required extended functions through one private static library.
if(NOT CODUOMP_CLANG_CL)
    return()
endif()

set(_extended_math_sources
    acosl asinl cosl sinl expl logl powl fabsl floorl frexpl scalbnl
    __polevll __invtrigl __cosl __sinl __rem_pio2l __rem_pio2_large)
list(TRANSFORM _extended_math_sources PREPEND "${CMAKE_SOURCE_DIR}/vendor/musl-math/")
list(TRANSFORM _extended_math_sources APPEND ".c")
add_library(coduomp_extended_math STATIC
    ${_extended_math_sources} "${CMAKE_SOURCE_DIR}/src/compat/coduo_extended_math.c")
target_include_directories(coduomp_extended_math PRIVATE
    "${CMAKE_SOURCE_DIR}/cmake/clang-cl-x87"
    "${CMAKE_SOURCE_DIR}/vendor/musl-math" "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(coduomp_extended_math PRIVATE _USE_MATH_DEFINES)
target_compile_options(coduomp_extended_math PRIVATE
    /fp:strict /clang:-mlong-double-80 /clang:-ffp-eval-method=source
    /clang:-ffp-contract=off
    "/FI${CMAKE_SOURCE_DIR}/cmake/clang-cl-x87/musl-prefix.h")
