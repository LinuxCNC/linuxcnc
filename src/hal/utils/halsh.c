//    Copyright 2007-2013, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <tcl.h>
#include "halcmd.h"
#include <hal.h>

extern int comp_id; // from halcmd.c, set by halcmd_startup()

Tcl_Interp *target_interp = NULL;
static int pending_cr = 0;

static void halError(Tcl_Interp *interp, int result) {
    // Usually, halcmd leaves a good string message via halcmd_error()
    // but just in case, fall back to using the result of the halcmd api call as
    // a negative errno value...
    if(!*Tcl_GetStringResult(interp))
	Tcl_SetResult(interp, strerror(-result), TCL_VOLATILE);
}

static int refcount = 0;

static void shutdown(void) {
    if(refcount > 0) {
        refcount --;
        if(refcount == 0) halcmd_shutdown();
    }
}

static int init() {
    int result = 0;
    if(refcount == 0) {
        result = halcmd_startup(0);
        atexit(shutdown);
    }
    if(result == 0) {
        refcount ++;
    }
    return result;
}
static void halExit(ClientData d) {
    (void)d;
    shutdown();
}

static int halCmd(ClientData cd, Tcl_Interp *interp, int argc, const char **argv) {
    (void)cd;
    int result;
    Tcl_ResetResult(interp);

    if(argc < 2) {
        Tcl_AppendResult(interp,
                "wrong # args: should be \"", argv[0], " command ...\"", NULL);
        return TCL_ERROR;
    }

    if(strcmp(argv[1], "--commands") == 0)
    {
        int i;
        Tcl_ResetResult(interp);
        for(i=0; i<halcmd_ncommands; i++)
            Tcl_AppendElement(interp, halcmd_commands[i].name);
        return TCL_OK;
    }

    target_interp = interp;
    pending_cr = 0;
    result = halcmd_parse_cmd((char **)argv+1);
    target_interp = NULL;

    if(result == 0) return TCL_OK;
    halError(interp, result);
    return TCL_ERROR;
}

// hal_stream: thin Tcl binding over the HAL stream API.
//
//   hal_stream attach KEY TYPESTRING   ->  handle (integer)
//   hal_stream depth HANDLE
//   hal_stream maxdepth HANDLE
//   hal_stream readable HANDLE
//   hal_stream read HANDLE             ->  list of one record's elements
//   hal_stream drain HANDLE            ->  flat list of all queued records
//   hal_stream detach HANDLE
//
// Streams attach to the HAL component already created by halcmd_startup.
#define HALSH_MAX_STREAMS 16
static struct halsh_stream_slot {
    hal_stream_t stream;
    int active;
} halsh_streams[HALSH_MAX_STREAMS];

static int halsh_format_one(Tcl_Interp *interp,
                            hal_stream_t *s, int idx,
                            hal_stream_data_u v) {
    char buf[64];
    switch(hal_stream_element_type(s, idx)) {
        case HAL_BIT:   snprintf(buf, sizeof(buf), "%d", v.b ? 1 : 0); break;
        case HAL_FLOAT: snprintf(buf, sizeof(buf), "%g", v.f); break;
        case HAL_S32:   snprintf(buf, sizeof(buf), "%lld", (long long)v.s); break;
        case HAL_U32:   snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v.u); break;
        case HAL_S64:   snprintf(buf, sizeof(buf), "%lld", (long long)v.l); break;
        case HAL_U64:   snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v.k); break;
        default:        snprintf(buf, sizeof(buf), "0"); break;
    }
    Tcl_AppendElement(interp, buf);
    return TCL_OK;
}

static int halsh_get_handle(Tcl_Interp *interp, const char *s, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0' || v < 0 || v >= HALSH_MAX_STREAMS || !halsh_streams[v].active) {
        Tcl_AppendResult(interp, "bad hal_stream handle: ", s, NULL);
        return TCL_ERROR;
    }
    *out = (int)v;
    return TCL_OK;
}

static int halStreamCmd(ClientData cd, Tcl_Interp *interp, int argc, const char **argv) {
    (void)cd;
    Tcl_ResetResult(interp);
    if (argc < 2) {
        Tcl_AppendResult(interp,
            "wrong # args: should be \"", argv[0], " subcommand ?args?\"", NULL);
        return TCL_ERROR;
    }
    const char *sub = argv[1];

    if (strcmp(sub, "attach") == 0) {
        if (argc != 4) {
            Tcl_AppendResult(interp, "args: attach KEY TYPESTRING", NULL);
            return TCL_ERROR;
        }
        char *end;
        long key = strtol(argv[2], &end, 0);
        if (*end != '\0') {
            Tcl_AppendResult(interp, "bad key: ", argv[2], NULL);
            return TCL_ERROR;
        }
        int idx = -1;
        for (int i = 0; i < HALSH_MAX_STREAMS; i++) {
            if (!halsh_streams[i].active) { idx = i; break; }
        }
        if (idx < 0) {
            Tcl_AppendResult(interp, "no free hal_stream slots", NULL);
            return TCL_ERROR;
        }
        int r = hal_stream_attach(&halsh_streams[idx].stream, comp_id,
                                  (int)key, argv[3]);
        if (r < 0) {
            Tcl_AppendResult(interp, "hal_stream_attach: ",
                             strerror(-r), NULL);
            return TCL_ERROR;
        }
        halsh_streams[idx].active = 1;
        char buf[16]; snprintf(buf, sizeof(buf), "%d", idx);
        Tcl_AppendResult(interp, buf, NULL);
        return TCL_OK;
    }

    if (argc < 3) {
        Tcl_AppendResult(interp, "args: ", sub, " HANDLE", NULL);
        return TCL_ERROR;
    }
    int h;
    if (halsh_get_handle(interp, argv[2], &h) != TCL_OK) return TCL_ERROR;
    hal_stream_t *s = &halsh_streams[h].stream;

    if (strcmp(sub, "depth") == 0) {
        char buf[16]; snprintf(buf, sizeof(buf), "%d", hal_stream_depth(s));
        Tcl_AppendResult(interp, buf, NULL);
        return TCL_OK;
    }
    if (strcmp(sub, "maxdepth") == 0) {
        char buf[16]; snprintf(buf, sizeof(buf), "%d", hal_stream_maxdepth(s));
        Tcl_AppendResult(interp, buf, NULL);
        return TCL_OK;
    }
    if (strcmp(sub, "readable") == 0) {
        Tcl_AppendResult(interp, hal_stream_readable(s) ? "1" : "0", NULL);
        return TCL_OK;
    }
    if (strcmp(sub, "read") == 0) {
        int n = hal_stream_element_count(s);
        if (n <= 0) return TCL_OK;
        hal_stream_data_u *buf = (hal_stream_data_u*)malloc(sizeof(*buf) * n);
        if (!buf) {
            Tcl_AppendResult(interp, "out of memory", NULL);
            return TCL_ERROR;
        }
        unsigned sampleno;
        int r = hal_stream_read(s, buf, &sampleno);
        if (r < 0) { free(buf); return TCL_OK; } // empty result
        for (int i = 0; i < n; i++) halsh_format_one(interp, s, i, buf[i]);
        free(buf);
        return TCL_OK;
    }
    if (strcmp(sub, "drain") == 0) {
        int n = hal_stream_element_count(s);
        if (n <= 0) return TCL_OK;
        hal_stream_data_u *buf = (hal_stream_data_u*)malloc(sizeof(*buf) * n);
        if (!buf) {
            Tcl_AppendResult(interp, "out of memory", NULL);
            return TCL_ERROR;
        }
        unsigned sampleno;
        while (hal_stream_readable(s)) {
            if (hal_stream_read(s, buf, &sampleno) < 0) break;
            for (int i = 0; i < n; i++) halsh_format_one(interp, s, i, buf[i]);
        }
        free(buf);
        return TCL_OK;
    }
    if (strcmp(sub, "detach") == 0) {
        hal_stream_detach(s);
        halsh_streams[h].active = 0;
        return TCL_OK;
    }
    Tcl_AppendResult(interp, "unknown hal_stream subcommand: ", sub, NULL);
    return TCL_ERROR;
}

int Hal_Init(Tcl_Interp *interp) {
    int result = init();
    if(result < 0) {
	Tcl_ResetResult(interp);
	halError(interp, result);
	return TCL_ERROR;
    }

    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL)
    {
        return TCL_ERROR;
    }

    Tcl_CreateCommand(interp, "hal", halCmd, 0, halExit);
    Tcl_CreateCommand(interp, "hal_stream", halStreamCmd, 0, NULL);

    Tcl_PkgProvide(interp, "Hal", "1.0");
    return TCL_OK;
}

#ifndef BUFFERLEN
#define BUFFERLEN 1024
#endif

void halcmd_output(const char *format, ...) {
    char buf[BUFFERLEN + 1];
    va_list ap;
    int len;

    va_start(ap, format);
    vsnprintf(buf, BUFFERLEN, format, ap);
    va_end(ap);

    if(pending_cr) 
	Tcl_AppendResult(target_interp, "\n", NULL);
    len = strlen(buf);
    if(buf[len-1] == '\n') {
	buf[len-1] = 0;
	pending_cr = 1;
    } else {
	pending_cr = 0;
    }
    Tcl_AppendResult(target_interp, buf, NULL);
}

void halcmd_error(const char *format, ...) {
    char buf[BUFFERLEN + 1];
    va_list ap;
    int len;

    va_start(ap, format);
    vsnprintf(buf, BUFFERLEN, format, ap);
    va_end(ap);

    if(pending_cr) 
	Tcl_AppendResult(target_interp, "\n", NULL);
    len = strlen(buf);
    if(buf[len-1] == '\n') {
	buf[len-1] = 0;
	pending_cr = 1;
    } else {
	pending_cr = 0;
    }
    Tcl_AppendResult(target_interp, buf, NULL);
}

void halcmd_warning(const char *format, ...) {
    char buf[BUFFERLEN + 1];
    va_list ap;
    int len;

    va_start(ap, format);
    vsnprintf(buf, BUFFERLEN, format, ap);
    va_end(ap);

    if(pending_cr) 
	Tcl_AppendResult(target_interp, "\n", NULL);
    len = strlen(buf);
    if(buf[len-1] == '\n') {
	buf[len-1] = 0;
	pending_cr = 1;
    } else {
	pending_cr = 0;
    }
    Tcl_AppendResult(target_interp, buf, NULL);
}

void halcmd_info(const char *format, ...) {
    char buf[BUFFERLEN + 1];
    va_list ap;
    int len;

    va_start(ap, format);
    vsnprintf(buf, BUFFERLEN, format, ap);
    va_end(ap);

    if(pending_cr) 
	Tcl_AppendResult(target_interp, "\n", NULL);
    len = strlen(buf);
    if(buf[len-1] == '\n') {
	buf[len-1] = 0;
	pending_cr = 1;
    } else {
	pending_cr = 0;
    }
    Tcl_AppendResult(target_interp, buf, NULL);
}

