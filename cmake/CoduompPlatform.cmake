#-----------------------------------------------------------------
# Platform settings
#
# MSVC generates the Visual Studio solution for Windows. The native branch
# mirrors build-mk/server.mk for the Linux dedicated server: same source flags,
# same x87 floating-point policy, same link libraries.
#-----------------------------------------------------------------

math(EXPR CODUOMP_POINTER_BITS "8 * ${CMAKE_SIZEOF_VOID_P}")

if(MSVC)

# Only a 32-bit x86 target reproduces the retail floating-point model: it is
# natively all-x87, so every float expression evaluates in 80-bit and narrows at
# the store points the sources encode.
set(CODUOMP_32BIT_X86 FALSE)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	if(NOT DEFINED MSVC_C_ARCHITECTURE_ID OR MSVC_C_ARCHITECTURE_ID STREQUAL "X86")
		set(CODUOMP_32BIT_X86 TRUE)
	endif()
endif()

if(MSVC_C_ARCHITECTURE_ID STREQUAL "X86")
	set(CODUOMP_ARCH "x86")
elseif(MSVC_C_ARCHITECTURE_ID STREQUAL "X64")
	set(CODUOMP_ARCH "x64")
elseif(MSVC_C_ARCHITECTURE_ID STREQUAL "ARM64")
	set(CODUOMP_ARCH "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(CODUOMP_ARCH "x86")
else()
	set(CODUOMP_ARCH "x64")
endif()

# EMULATE_X87 selects the source-level software-x87 variants. Native x87 codegen
# is available here, so the recovered canonical bodies are used unchanged.
set(CODUOMP_EMULATE_X87 0)

if(CODUOMP_32BIT_X86)
	# Retail x87 code generation: /arch:IA32 disables SSE float math.
	add_compile_options(/arch:IA32)
	set(CODUOMP_FLOAT_MODEL "native x87 (/arch:IA32)")
else()
	message(WARNING
		"CoDUOMP: this is not a Win32 configuration. MSVC cannot emit x87 code "
		"or an 80-bit long double on 64-bit targets, so floating-point results "
		"are not retail-equivalent. Re-run CMake with -A Win32 for the faithful "
		"32-bit configuration.")
	set(CODUOMP_FLOAT_MODEL "native SSE (not retail-equivalent)")
endif()

# The recovered source uses the same warning discipline as the Makefiles, but
# MSVC's warning set differs, so the level stays low (as in the parent project)
# and warnings are not errors. C5105 fires inside the Windows SDK headers
# (winbase.h) on a macro expansion containing `defined`; no project source
# controls that expansion, so it stays suppressed for the SDK, the recovered
# sources, and the bundled libraries alike.
add_compile_options(/W1 /utf-8 /wd5105)

# MSBuild imports Directory.Build.props from the directory above each generated
# project. Generate one next to the solution so the compiler's /MP budget
# follows the /m option the build command passes to MSBuild: no /m stays
# sequential, /m uses every processor, and /m:<n> uses n compiler processes per
# project. The client and SDL contain far more files than the solution has
# projects, so /MP is what shortens a full rebuild.
configure_file(
	"${CMAKE_CURRENT_LIST_DIR}/CoduompDirectory.Build.props"
	"${CMAKE_BINARY_DIR}/Directory.Build.props" COPYONLY)

add_compile_definitions(
	_CRT_SECURE_NO_WARNINGS
	_CRT_NONSTDC_NO_DEPRECATE
	_WINSOCK_DEPRECATED_NO_WARNINGS)

# System libraries required by the client executable. The static SDL2, libjpeg,
# libcurl and minizip builds used by the Makefiles need these as well.
set(CODUOMP_CLIENT_SYSTEM_LIBS
	opengl32 winmm ws2_32 advapi32 crypt32 bcrypt
	user32 gdi32 shell32 version ole32 oleaut32 uuid
	imm32 setupapi iphlpapi secur32 wldap32 ddraw dxguid)

set(CODUOMP_UI_SYSTEM_LIBS user32 gdi32)
set(CODUOMP_SERVER_SYSTEM_LIBS ws2_32)

else()

#-----------------------------------------------------------------
# Native GCC/Clang settings
#-----------------------------------------------------------------

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
		message(FATAL_ERROR
			"CoDUOMP: the native build supports x86 and x86-64 targets "
			"(detected '${CMAKE_SYSTEM_PROCESSOR}').")
	endif()
	set(CODUOMP_ARCH "x86")
	set(CODUOMP_32BIT_X86 TRUE)
	set(CODUOMP_MODULE_ARCH "i386")
else()
	set(CODUOMP_ARCH "x64")
	set(CODUOMP_32BIT_X86 FALSE)
	set(CODUOMP_MODULE_ARCH "x86_64")
endif()

# x87 floating-point policy (build-mk/x87-policy.mk). The original is a 32-bit
# x87 binary, so the faithful build pins -O0 and either uses the native x87 unit
# (32-bit x86) or forces x87 evaluation with -mfpmath=387, and rounds every
# float assignment to 32 bits. A target without an x87 unit cannot reproduce the
# model, and the Makefile's relaxed mode needs the software-x87 sources, so stop
# with the reason instead of producing a lookalike build.
include(CheckCCompilerFlag)
set(CODUOMP_FLOAT_FLAGS "")
if(CODUOMP_32BIT_X86)
	set(CODUOMP_FLOAT_FLAGS "-O0 -fexcess-precision=fast")
	set(CODUOMP_FLOAT_MODEL "native x87 (32-bit)")
else()
	check_c_compiler_flag("-mfpmath=387" CODUOMP_HAVE_MFPMATH_387)
	if(CODUOMP_HAVE_MFPMATH_387)
		set(CODUOMP_FLOAT_FLAGS "-O0 -mfpmath=387 -fexcess-precision=fast")
		set(CODUOMP_FLOAT_MODEL "native x87 (-mfpmath=387)")
	endif()
endif()
if(NOT CODUOMP_FLOAT_FLAGS)
	message(FATAL_ERROR
		"CoDUOMP: a faithful server build needs GCC on x86 or x86-64 "
		"(-mfpmath=387); '${CMAKE_C_COMPILER_ID}' on "
		"'${CMAKE_SYSTEM_PROCESSOR}' cannot reproduce the original x87 "
		"floating-point model. Build with GCC instead, or use the Makefile "
		"targets with CODUO_FP_FAITHFUL=relaxed.")
endif()

# EMULATE_X87 selects the source-level software-x87 variants, which this target
# reaches with native x87 code generation and does not need.
set(CODUOMP_EMULATE_X87 0)

# The faithful model is pinned to -O0 and the Makefile build defines no NDEBUG,
# so keep the selected configuration from adding an optimization level or
# disabling assertions behind the target's back. Third-party code built in this
# tree (the bundled zlib) follows the same configuration flags.
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

# _GNU_SOURCE is the Linux define the Makefile server target uses.
# CODUO_ENGINE_ALLOW_NON_X87_FLOAT selects the C fallbacks for operations whose
# inline x87 assembly needs a 32-bit target (build-mk/x87-policy.mk).
set(CODUOMP_PLATFORM_DEFINES _GNU_SOURCE)
if(NOT CODUOMP_32BIT_X86)
	list(APPEND CODUOMP_PLATFORM_DEFINES CODUO_ENGINE_ALLOW_NON_X87_FLOAT=1)
endif()

# The Makefile server links the C++ runtime through g++ and imports dl, libm,
# pthread and zlib.
set(CODUOMP_SERVER_SYSTEM_LIBS dl m pthread)

endif()
