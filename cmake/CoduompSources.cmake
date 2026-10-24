#-----------------------------------------------------------------
# Source membership
#
# build-mk/ owns every target's source list. Read the same manifests the
# Makefiles include instead of keeping a second copy here.
#-----------------------------------------------------------------

# Client executable and client-module membership:
#   CLIENT_ENGINE_TARGET_*       CoDUOMP executable
#   CLIENT_ALL_C_SOURCES         uo_cgame_mp_x86
coduomp_manifest_read("build-mk/client-targets.mk")
set(CODUOMP_CGAME_SOURCES		${CLIENT_ALL_C_SOURCES})
set(CODUOMP_CLIENT_C_SOURCES	${CLIENT_ENGINE_TARGET_C_SOURCES})
set(CODUOMP_CLIENT_CXX_SOURCES	${CLIENT_ENGINE_TARGET_CXX_SOURCES})
coduomp_manifest_reset()

# UI module membership: ALL_C_SOURCES = UI_C_SOURCES + SHARED_C_SOURCES.
coduomp_manifest_read("build-mk/ui.mk")
set(CODUOMP_UI_SOURCES ${ALL_C_SOURCES})
coduomp_manifest_reset()

# Game module membership: the subsystem lists are the target's own selection,
# plus the libm and BG ownership lists from the shared manifests.
coduomp_manifest_read("build-mk/game.mk")
set(CODUOMP_GAME_SOURCES
	${SRCS}
	${LIBM_SRCS}
	${CRT_COMPAT_SRCS}
	${QCOMMON_SRCS}
	${MATH_SRCS}
	${BG_SRCS})
coduomp_manifest_reset()
