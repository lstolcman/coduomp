#ifndef CODUOMP_COMPRESSION_BOUNDARY_H
#define CODUOMP_COMPRESSION_BOUNDARY_H

/*
 * Minizip's archive handle remains opaque. Use the selected dependency's
 * declarations: classic Minizip and Minizip-ng's compatibility API expose
 * different public record layouts despite sharing function and library names.
 * pkg-config and dependency prefixes may expose unzip.h directly or through
 * the minizip include directory.
 */
#if __has_include(<unzip.h>)
#include <unzip.h>
#elif __has_include(<minizip/unzip.h>)
#include <minizip/unzip.h>
#else
#error "Minizip development headers are required (classic Minizip or Minizip-ng compatibility API)"
#endif

#ifdef __cplusplus
extern "C" {
#endif

unzFile unzReOpen(const char *path, unzFile source);
int unzSetCurrentFileInfoPosition(unzFile file, unsigned long position);

#ifdef __cplusplus
}
#endif

#endif
