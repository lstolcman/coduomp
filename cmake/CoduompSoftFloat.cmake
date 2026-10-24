# Use the vendored 8086 specialization and portable integer primitives. Source
# membership is the same object list as the maintained SoftFloat Makefile.
set(OBJ .c)
coduomp_manifest_read("vendor/softfloat/build/coduo-x87/Makefile")
set(_softfloat_sources "")
foreach(_source IN LISTS OBJS_PRIMITIVES OBJS_OTHERS)
    string(REGEX REPLACE "[.]o$" ".c" _source "${_source}")
    list(APPEND _softfloat_sources "${CMAKE_SOURCE_DIR}/vendor/softfloat/source/${_source}")
endforeach()
foreach(_source IN LISTS OBJS_SPECIALIZE)
    string(REGEX REPLACE "[.]o$" ".c" _source "${_source}")
    list(APPEND _softfloat_sources "${CMAKE_SOURCE_DIR}/vendor/softfloat/source/8086/${_source}")
endforeach()
coduomp_manifest_reset()
add_library(coduomp_softfloat STATIC ${_softfloat_sources})
target_include_directories(coduomp_softfloat
    PUBLIC "${CMAKE_SOURCE_DIR}/vendor/softfloat/source/include"
    PRIVATE "${CMAKE_SOURCE_DIR}/cmake/softfloat"
            "${CMAKE_SOURCE_DIR}/vendor/softfloat/source/8086")
target_compile_definitions(coduomp_softfloat PUBLIC
    SOFTFLOAT_FAST_INT64=1 LITTLEENDIAN=1 "THREAD_LOCAL=__declspec(thread)")
target_compile_definitions(coduomp_softfloat PRIVATE
    SOFTFLOAT_ROUND_ODD INLINE_LEVEL=0 SOFTFLOAT_FAST_DIV32TO16 SOFTFLOAT_FAST_DIV64TO32)
