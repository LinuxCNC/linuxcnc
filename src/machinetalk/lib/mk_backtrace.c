#include "mk-backtrace.h"

#include "config.h"

#if defined(HAVE_LIBBACKTRACE)

// custom libbacktrace install
#if  defined(HAVE_LIBBACKTRACE_BACKTRACE_H)
#include <libbacktrace/backtrace.h>
#include <libbacktrace/backtrace-supported.h>
#endif

// gcc-bundled install
#if defined(HAVE_BACKTRACE_H)
#include <backtrace.h>
#include <backtrace-supported.h>
#endif

#if defined(BACKTRACE_SUPPORTED)

#include <string.h>

struct bt_ctx {
    struct backtrace_state *state;
    int error;
    btprint_t print;
    const char *prefix;
};

static void error_callback(void *data, const char *msg, int errnum)
{
    struct bt_ctx *ctx = data;
    ctx->print(ctx->prefix, "ERROR decoding backtrace: %s (%d)", msg, errnum);
    ctx->error = 1;
}

static void syminfo_callback (void *data, uintptr_t pc,
			      const char *symname,
			      uintptr_t symval, uintptr_t symsize)
{
    struct bt_ctx *ctx = data;

    if (symname) {
	ctx->print(ctx->prefix, "%lx %s ??:0", (unsigned long)pc, symname);
    } else {
	ctx->print(ctx->prefix, "%lx ?? ??:0", (unsigned long)pc);
    }
}

static int full_callback(void *data, uintptr_t pc, const char *filename, int lineno, const char *function)
{
    struct bt_ctx *ctx = data;
    if (function) {
	ctx->print(ctx->prefix, "%-8.8lx %-16.16s (%s:%d)",
		   (unsigned long)pc,
		   function,
		   filename ? filename : "??",
		   lineno);
    } else {
	backtrace_syminfo (ctx->state, pc,
			   syminfo_callback,
			   error_callback, data);
    }
    return 0;
}

static int simple_callback(void *data, uintptr_t pc)
{
    struct bt_ctx *ctx = data;
    backtrace_pcinfo(ctx->state, pc,
		     full_callback,
		     error_callback, data);
    return 0;
}

static struct backtrace_state *state;

void backtrace_init(const char *name)
{
    state = backtrace_create_state (name, BACKTRACE_SUPPORTS_THREADS,
				    error_callback, NULL);
}

void backtrace(const char *prefix, const char *header, btprint_t print, int skip)
{
    if (prefix == NULL)
	prefix = "";
    struct bt_ctx ctx = {state, 0, print, prefix};
    if (header && strlen(header))
	print(prefix,  " --- %s backtrace: ---", header);
    backtrace_simple(state, skip, simple_callback, error_callback, &ctx);
    if (header && strlen(header))
	print(prefix,  " --------------------", header);
}

#endif // BACKTRACE_SUPPORTED
#endif // HAVE_LIBBACKTRACE

#if !defined(BACKTRACE_SUPPORTED) || ! defined(HAVE_LIBBACKTRACE)

// dummy versions
void backtrace_init(const char *name) {}
void backtrace(const char *prefix,const char *header, btprint_t print, int skip)
{
    print(prefix,
	  "(backtrace not available - libbacktrace not found during build)");
}

#endif
