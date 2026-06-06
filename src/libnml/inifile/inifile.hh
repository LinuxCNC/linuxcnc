/*
 * inifile.hh - C++ compatibility wrapper over iniparse C API
 *
 * This header provides the IniFile class interface used by legacy C++
 * consumers. New code should use iniparse.h directly.
 *
 * License: GPL Version 2
 */

#ifndef INIFILE_HH
#define INIFILE_HH

#include <iniparse.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus

class IniFile {
public:
    typedef enum {
        ERR_NONE                = 0x00,
        ERR_NOT_OPEN            = 0x01,
        ERR_SECTION_NOT_FOUND   = 0x02,
        ERR_TAG_NOT_FOUND       = 0x04,
        ERR_CONVERSION          = 0x08,
        ERR_LIMITS              = 0x10,
    } ErrorCode;

                                IniFile(int errMask=0, FILE *_fp=NULL)
                                    : fp(_fp), owned(false) {}
                                ~IniFile(void) { Close(); }

    bool                        Open(const char *file) {
                                    char path[PATH_MAX];
                                    if (fp && owned) { fclose(fp); fp = NULL; }
                                    if (ini_tilde_expansion(file, path, sizeof(path)))
                                        return false;
                                    fp = fopen(path, "r");
                                    if (fp) owned = true;
                                    return fp != NULL;
                                }

    bool                        Close(void) {
                                    if (fp && owned) fclose(fp);
                                    fp = NULL; owned = false;
                                    return true;
                                }

    bool                        IsOpen(void) { return fp != NULL; }

    const char *                Find(const char *tag, const char *section=NULL,
                                     int num = 1, int *lineno = NULL) {
                                    (void)lineno;
                                    return ini_find(fp, tag, section, num);
                                }

    ErrorCode                   Find(int *result,
                                     const char *tag, const char *section,
                                     int num=1) {
                                    const char *s = Find(tag, section, num);
                                    if (!s) return ERR_TAG_NOT_FOUND;
                                    int tmp;
                                    if (sscanf(s, "%i", &tmp) != 1)
                                        return ERR_CONVERSION;
                                    *result = tmp;
                                    return ERR_NONE;
                                }

    ErrorCode                   Find(double *result,
                                     const char *tag, const char *section,
                                     int num=1) {
                                    const char *s = Find(tag, section, num);
                                    if (!s) return ERR_TAG_NOT_FOUND;
                                    double tmp;
                                    if (sscanf(s, "%lf", &tmp) != 1)
                                        return ERR_CONVERSION;
                                    *result = tmp;
                                    return ERR_NONE;
                                }

    const char *                FindString(char *dest, size_t n,
                                     const char *tag, const char *section=NULL,
                                     int num = 1, int *lineno = NULL) {
                                    (void)lineno;
                                    const char *res = Find(tag, section, num);
                                    if (!res) return NULL;
                                    int r = snprintf(dest, n, "%s", res);
                                    if (r < 0 || (size_t)r >= n) return NULL;
                                    return dest;
                                }

    const char *                FindPath(char *dest, size_t n,
                                     const char *tag, const char *section=NULL,
                                     int num = 1, int *lineno = NULL) {
                                    (void)lineno;
                                    const char *res = Find(tag, section, num);
                                    if (!res) return NULL;
                                    if (ini_tilde_expansion(res, dest, n))
                                        return NULL;
                                    return dest;
                                }

    ErrorCode                   TildeExpansion(const char *file, char *path,
                                     size_t n) {
                                    return ini_tilde_expansion(file, path, n)
                                        ? ERR_CONVERSION : ERR_NONE;
                                }

private:
    FILE                        *fp;
    bool                        owned;
};

#endif /* __cplusplus */

#endif /* INIFILE_HH */
