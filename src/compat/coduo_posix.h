/* NOT_FROM_ORIGINAL_SOURCE: host CRT spellings used by shared source. */
#ifndef CODUO_POSIX_H
#define CODUO_POSIX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_MSC_VER)
#include <io.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup
#define isatty _isatty
#define fileno _fileno
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#define S_ISCHR(mode) (((mode) & _S_IFMT) == _S_IFCHR)
#define S_ISFIFO(mode) (((mode) & _S_IFMT) == _S_IFIFO)
#define S_ISBLK(mode) 0
#else
#include <strings.h>
#include <unistd.h>
#endif

#endif
