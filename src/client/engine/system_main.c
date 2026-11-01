#include "platform/crt_boundary.h"
#include "platform/floating_point_boundary.h"
#include "platform/punkbuster_boundary.h"
#include "platform/sdl_platform.h"
#include "q_shared.h"
#include "server/server.h"
#include "system_console.h"
#include "system_info.h"
#include "system_input.h"
#include "system_localization.h"
#include "system_platform.h"

#include <SDL.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "system_process_lock.h"
#elif defined(__APPLE__)
#include "platform/macos_app_bundle.h"
#elif defined(__linux__)
#include <sys/resource.h>
#endif

enum {
    SYS_NATIVE_COMMAND_LINE_CAPACITY = 8192,
    SYS_IDLE_SLEEP_MSEC = 5,
    SYS_ALLOW_DUPLICATE_PREFIX_LENGTH = 9,
    SYS_CONSOLE_HIDDEN = 0
};

#if defined(_WIN32)
enum {
    SYS_WINDOWS_STDIO_STREAM_TARGET = 2048
};

/* NOT_FROM_ORIGINAL_SOURCE: reserve CRT streams for archives and filesystem
 * restarts before opening game data. MSVCRT supports at most 2048 streams;
 * preserve a higher limit if a different runtime already provides one. */
static void coduomp_windows_raise_open_file_limit(void)
{
    if (_getmaxstdio() < SYS_WINDOWS_STDIO_STREAM_TARGET &&
        _setmaxstdio(SYS_WINDOWS_STDIO_STREAM_TARGET) < 0) {
        perror("CoDUOMP: cannot raise open-file stream limit");
    }
}
#elif defined(__linux__)
enum {
    SYS_LINUX_OPEN_FILE_TARGET = 4096
};

/* NOT_FROM_ORIGINAL_SOURCE: Linux sessions can inherit a low descriptor soft
 * limit. Keep room for archives and filesystem restarts without lowering an
 * existing limit or changing the inherited hard limit. */
static void coduomp_linux_raise_open_file_limit(void)
{
    struct rlimit limits;
    rlim_t target = SYS_LINUX_OPEN_FILE_TARGET;

    if (getrlimit(RLIMIT_NOFILE, &limits) != 0) {
        perror("CoDUOMP: cannot read open-file limit");
        return;
    }
    if (limits.rlim_cur >= target)
        return;
    if (limits.rlim_max < target)
        target = limits.rlim_max;
    if (limits.rlim_cur >= target)
        return;

    limits.rlim_cur = target;
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0)
        perror("CoDUOMP: cannot raise open-file limit");
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: constructs the mutable command string consumed by
 * Com_ParseCommandLine from the native argc/argv process interface. Quoting is
 * retained for arguments containing whitespace so startup cvar values survive
 * the source-level conversion. */
static qboolean Sys_BuildNativeCommandLine(
    int argc, char **argv, char *commandLine, size_t commandLineSize)
{
    size_t cursor = 0;

    commandLine[0] = '\0';
    for (int argumentIndex = 1;
         argumentIndex < argc; ++argumentIndex) {
        const char *argument = argv[argumentIndex];
        const qboolean quote =
            strpbrk(argument, " \t") != NULL ? qtrue : qfalse;
        const size_t argumentLength = strlen(argument);
        const size_t required =
            argumentLength + (quote != qfalse ? 2U : 0U) +
            (cursor != 0 ? 1U : 0U);

        if (required >= commandLineSize - cursor)
            return qfalse;
        if (cursor != 0)
            commandLine[cursor++] = ' ';
        if (quote != qfalse)
            commandLine[cursor++] = '"';
        memcpy(&commandLine[cursor], argument, argumentLength);
        cursor += argumentLength;
        if (quote != qfalse)
            commandLine[cursor++] = '"';
        commandLine[cursor] = '\0';
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: native process entry point corresponding to the
 * recovered WinMain. SDL2main supplies the Windows WinMain wrapper, so this is
 * the only application entry point on every supported host. Subsystem
 * initialization and the frame loop preserve the original ordering; SDL
 * replaces only the Win32 application shell. */
int main(int argc, char **argv)
{
    char commandLine[SYS_NATIVE_COMMAND_LINE_CAPACITY];
    char workingDirectory[MAX_OSPATH];

    coduomp_restore_retail_x87_precision();

#if defined(_WIN32)
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): opt out of DPI
     * virtualization before the console, splash, or game window exists so
     * fullscreen fills the display exactly under Windows display scaling.
     * See coduomp_sys_enable_dpi_awareness. */
    coduomp_sys_enable_dpi_awareness();
    coduomp_windows_raise_open_file_limit();
    /* The original WinMain receives the executable instance handle; the SDL
     * entry wrapper does not, so resolve the same module handle directly. */
    sysApplicationInstance = (void *)GetModuleHandleA(NULL);
#elif defined(__APPLE__)
    if (coduomp_macos_prepare_launch(argc, argv) == 0)
        return EXIT_SUCCESS;
#elif defined(__linux__)
    coduomp_linux_raise_open_file_limit();
#endif

    if (Sys_BuildNativeCommandLine(
            argc, argv, commandLine, sizeof(commandLine)) == qfalse) {
        fputs("CoDUOMP: command line is too long\n", stderr);
        return EXIT_FAILURE;
    }

    (void)Sys_InitLocalization();

#if defined(_WIN32)
    if (argc < 2 ||
        coduo_crt_strnicmp(
            argv[1], "allowdupe",
            SYS_ALLOW_DUPLICATE_PREFIX_LENGTH) != 0 ||
        (int8_t)argv[1][SYS_ALLOW_DUPLICATE_PREFIX_LENGTH] >
            (int8_t)' ') {
        Sys_InitProcessLockFile();
        if (Sys_CheckProcessLock() == qfalse) {
            Sys_ShutdownLocalization();
            return EXIT_SUCCESS;
        }
    }
    (void)SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

    if (CoduoSDL_Init() == qfalse) {
        fputs("CoDUOMP: SDL initialization failed\n", stderr);
        Sys_ShutdownLocalization();
        return EXIT_FAILURE;
    }
    (void)atexit(CoduoSDL_Shutdown);

    Sys_InitHardwareInfo();
    sysExecutableChecksum = Sys_GetExecutableChecksum(sysApplicationInstance);

    Sys_CreateConsole();
    Sys_CreateSplashWindow();
    Sys_ShowSplashWindow();

    /* WinMain contains the same lazy clock initialization as Sys_Milliseconds
     * and discards the first resulting timestamp. */
    (void)Sys_Milliseconds();

    Com_Init(commandLine);
    (void)coduomp_crt_getcwd(
        workingDirectory, sizeof(workingDirectory));
    Com_Printf("Working directory: %s\n", workingDirectory);

    if ((dedicated == NULL || dedicated->integer == 0) &&
        com_viewlog->integer == 0) {
        Sys_ShowConsole(SYS_CONSOLE_HIDDEN, qfalse);
    }

    if (dedicated == NULL || dedicated->integer == 0)
        PB_InitializeClient(sysApplicationInstance);
    PB_InitializeServer();

    for (;;) {
        if (sysWindowMinimized != qfalse ||
            (dedicated != NULL && dedicated->integer != 0)) {
            SDL_Delay(SYS_IDLE_SLEEP_MSEC);
        }

        IN_Frame();
        Com_Frame();

        if (dedicated == NULL || dedicated->integer == 0)
            PB_ProcessClientEvents();
        PB_ProcessServerEvents();
    }
}
