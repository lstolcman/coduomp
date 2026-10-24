# MSVC x86 supports x87 arithmetic through /arch:IA32; MSVC x64 does not.
# The existing source paths still use software x87 at selected sites on both
# targets, so neither configuration establishes whole-program retail parity.
set(CODUO_FP_FAITHFUL auto CACHE STRING "Floating-point policy: auto, strict, or relaxed")
set_property(CACHE CODUO_FP_FAITHFUL PROPERTY STRINGS auto strict relaxed)
if(NOT CODUO_FP_FAITHFUL MATCHES "^(auto|strict|relaxed)$")
    message(FATAL_ERROR "CODUO_FP_FAITHFUL must be auto, strict, or relaxed.")
endif()
if(CODUO_FP_FAITHFUL STREQUAL "strict")
    message(FATAL_ERROR
        "The current MSVC source paths have not established whole-program "
        "floating-point parity. MSVC x86 supports x87 arithmetic, but selected "
        "sites still use software x87 and host transcendental fallbacks. "
        "Use the default auto policy or relaxed to build with these limits.")
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
if(CODUOMP_32BIT_X86)
    set(CODUOMP_FLOAT_MODEL "x87 compiler arithmetic (/arch:IA32); software x87 at selected source sites")
    message(STATUS "MSVC x86 is the default preset because it supports native x87 arithmetic.")
else()
    set(CODUOMP_FLOAT_MODEL "SSE2 compiler arithmetic; software x87 at selected source sites")
    message(WARNING
        "MSVC x64 cannot generate x87 arithmetic from C/C++. Native floating-point "
        "expressions use SSE2 or newer instructions and may differ from the original "
        "game's x87 results. Software x87 covers only selected source sites. "
        "Use the msvc preset (Win32) for x87 compiler arithmetic. "
        "See docs/client-building.md for the architecture and fidelity limits.")
endif()
message(STATUS "Whole-program floating-point parity remains unverified on both MSVC targets.")

# System libraries required by the client executable. The static SDL2, libjpeg,
# libcurl and minizip builds used by the Makefiles need these as well.
set(CODUOMP_CLIENT_SYSTEM_LIBS
	opengl32 winmm ws2_32 advapi32 crypt32 bcrypt
	user32 gdi32 shell32 version ole32 oleaut32 uuid
	imm32 setupapi iphlpapi secur32 wldap32 ddraw dxguid)

set(CODUOMP_UI_SYSTEM_LIBS user32 gdi32)
