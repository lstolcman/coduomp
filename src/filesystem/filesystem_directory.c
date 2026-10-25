#include "filesystem.h"

#include "qcommon/com_sprintf.h"
#include "qcommon/q_string.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <dirent.h>
#endif
#include <string.h>

enum {
    FS_DIRECTORY_NAME_COMPARE_LIMIT = 99999
};

/*
 * Source: CoDUOMP.exe 0x00468dc0..0x00468eaf and coduo_lnxded
 * 0x080c82c8..0x080c835b.  The Windows body ignores the three managed names
 * ".", "..", and "CVS" with case-insensitive comparisons; Linux ignores only
 * "." and ".." with exact comparisons.  The host-API branches retain those
 * target behaviors when Windows behavior is built on a POSIX native host.
 */
qboolean FS_DirectoryHasNonDotEntries(const char *path)
{
#if defined(_WIN32)
    char searchPath[MAX_OSPATH];
    struct _finddata_t findData;
    intptr_t findHandle;

    Com_sprintf(searchPath, sizeof(searchPath), "%s\\*", path);
    findHandle = _findfirst(searchPath, &findData);
    if (findHandle == -1)
        return qfalse;

    do {
        if ((findData.attrib & _A_SUBDIR) == 0 ||
            (Q_stricmpn(findData.name, ".",
                        FS_DIRECTORY_NAME_COMPARE_LIMIT) != 0 &&
             Q_stricmpn(findData.name, "..",
                        FS_DIRECTORY_NAME_COMPARE_LIMIT) != 0 &&
             Q_stricmpn(findData.name, "CVS",
                        FS_DIRECTORY_NAME_COMPARE_LIMIT) != 0)) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            (void)_findclose(findHandle);
            return qtrue;
        }
    } while (_findnext(findHandle, &findData) != -1);

    (void)_findclose(findHandle);
    return qfalse;
#else
    DIR *const directory = opendir(path);

    if (directory == NULL)
        return qfalse;

    for (;;) {
        const struct dirent *const entry = readdir(directory);
        if (entry == NULL)
            break;
        if (Q_stricmpn(entry->d_name, ".",
                       FS_DIRECTORY_NAME_COMPARE_LIMIT) != 0 &&
            Q_stricmpn(entry->d_name, "..",
                       FS_DIRECTORY_NAME_COMPARE_LIMIT) != 0 &&
            Q_stricmpn(entry->d_name, "CVS",
                       FS_DIRECTORY_NAME_COMPARE_LIMIT) != 0) {
            (void)closedir(directory);
            return qtrue;
        }
    }

    (void)closedir(directory);
    return qfalse;
#endif
}
