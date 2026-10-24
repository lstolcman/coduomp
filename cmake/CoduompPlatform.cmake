# MSVC does not provide an 80-bit C long double, even with /arch:IA32.
# Use the existing emulated bodies and retain the Makefile's explicit opt-in
# for incomplete whole-program floating-point fidelity.
set(CODUO_FP_FAITHFUL auto CACHE STRING "Floating-point policy: auto, strict, or relaxed")
set_property(CACHE CODUO_FP_FAITHFUL PROPERTY STRINGS auto strict relaxed)
if(NOT CODUO_FP_FAITHFUL STREQUAL "relaxed")
    message(FATAL_ERROR
        "MSVC cannot satisfy the native x87/long-double policy. Use the MinGW "
        "Makefile build, or explicitly select -DCODUO_FP_FAITHFUL=relaxed. "
        "The emulated sites preserve their operation graphs, but remaining "
        "native arithmetic and transcendental fallbacks are not retail-equivalent.")
endif()
string(TOUPPER "${MSVC_C_ARCHITECTURE_ID}" _msvc_arch)
if(_msvc_arch STREQUAL "X86" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CODUOMP_ARCH x86)
    set(CODUOMP_32BIT_X86 TRUE)
elseif(_msvc_arch STREQUAL "X64")
    set(CODUOMP_ARCH x64)
    set(CODUOMP_32BIT_X86 FALSE)
else()
    message(FATAL_ERROR "This MSVC build supports only x86 and x64.")
endif()
set(CODUOMP_EMULATE_X87 1)
set(CODUOMP_FLOAT_MODEL "SoftFloat at emulated sites; whole-program fidelity incomplete")
message(WARNING "${CODUOMP_FLOAT_MODEL}")

# System libraries required by the client executable. The static SDL2, libjpeg,
# libcurl and minizip builds used by the Makefiles need these as well.
set(CODUOMP_CLIENT_SYSTEM_LIBS
	opengl32 winmm ws2_32 advapi32 crypt32 bcrypt
	user32 gdi32 shell32 version ole32 oleaut32 uuid
	imm32 setupapi iphlpapi secur32 wldap32 ddraw dxguid)

set(CODUOMP_UI_SYSTEM_LIBS user32 gdi32)
