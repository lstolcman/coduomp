# Miniaudio is built in on both architectures. Windows x86 also includes the
# optional Miles runtime loader; no retail DLL is needed while compiling.
if(NOT CODUOMP_BUILD_CLIENT)
    return()
endif()
set(CODUOMP_AUDIO_DEFINITION AUDIO_BACKEND_MINIAUDIO)
