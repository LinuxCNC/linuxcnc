/*
 * inifile.h - Legacy C API compatibility header
 *
 * New code should use iniparse.h directly.
 * This header provides the old iniFind/iniFindInt/iniFindDouble names.
 *
 * License: GPL Version 2
 */

#ifndef LINUXCNC_INIFILE_H
#define LINUXCNC_INIFILE_H

#include <iniparse.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *iniFind(FILE *fp, const char *tag, const char *section) {
    return ini_find(fp, tag, section, 1);
}

static inline const int iniFindInt(FILE *fp, const char *tag, const char *section, int *result) {
    return ini_find_int(fp, tag, section, result);
}

static inline const int iniFindDouble(FILE *fp, const char *tag, const char *section, double *result) {
    return ini_find_double(fp, tag, section, result);
}

static inline int TildeExpansion(const char *file, char *path, size_t size) {
    return ini_tilde_expansion(file, path, size) == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* LINUXCNC_INIFILE_H */
