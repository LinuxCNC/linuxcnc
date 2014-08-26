#ifndef _BACKTRACE_H
#define _BACKTRACE_H

#include <stdarg.h>

// the handler for printing backtrace lines
typedef void (*btprint_t)(const char *prefix, const char *fmt, ...);

#ifdef __cplusplus
extern "C" {
#endif

    void backtrace_init(const char *name);
    void backtrace(const char *prefix, const char *header,
		   btprint_t print, int skip);

#ifdef __cplusplus
}
#endif

#endif // _BACKTRACE_H
