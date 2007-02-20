#include <stdlib.h>
#include <stdio.h>
#include <tcl.h>
#include "halcmd.h"

Tcl_Interp *target_interp = NULL;

static void halError(Tcl_Interp *interp, int result) {
    switch(result) {
        case HAL_SUCCESS:
            Tcl_SetResult(interp, "Call Successful", TCL_STATIC); break;
        case HAL_UNSUP:
            Tcl_SetResult(interp, "Function not supported", TCL_STATIC); break;
        case HAL_BADVAR:
            Tcl_SetResult(interp, "Duplicate or not-found variable name", TCL_STATIC); break;
        case HAL_INVAL:
            Tcl_SetResult(interp, "Invalid argument", TCL_STATIC); break;
        case HAL_NOMEM:
            Tcl_SetResult(interp, "Not enough memory", TCL_STATIC); break;
        case HAL_LIMIT:
            Tcl_SetResult(interp, "Resource limit reached", TCL_STATIC); break;
        case HAL_PERM:
            Tcl_SetResult(interp, "Permission Denied", TCL_STATIC); break;
        case HAL_BUSY:
            Tcl_SetResult(interp, "Resources is busy or locked", TCL_STATIC);
            break;
        case HAL_NOTFND:
            Tcl_SetResult(interp, "Object not found", TCL_STATIC); break;
        case HAL_FAIL:
            Tcl_SetResult(interp, "Operation failed", TCL_STATIC); break;
        default: {
	    char buf[80];
	    snprintf(buf, 80, "Unknown HAL error code %d", result);
	    Tcl_SetResult(interp, buf, TCL_VOLATILE);
	    break;
	}
    }
}

static int refcount = 0;

static void shutdown(void) {
    if(refcount > 0) {
        refcount --;
        if(refcount == 0) halcmd_shutdown();
    }
}

static int init() {
    int result = HAL_SUCCESS;
    if(refcount == 0) {
        result = halcmd_startup();
        atexit(shutdown);
    }
    if(result == HAL_SUCCESS) {
        refcount ++;
    }
    return result;
}
static void halExit(ClientData d) {
    shutdown();
}

static int halCmd(ClientData cd, Tcl_Interp *interp, int argc, const char **argv) {
    int result;
    Tcl_ResetResult(interp);

    if(argc < 2) {
        Tcl_AppendResult(interp,
                "wrong # args: should be \"", argv[0], " command ...\"", NULL);
        return TCL_ERROR;
    }
    target_interp = interp;
    result = halcmd_parse_cmd((char **)argv+1);
    target_interp = NULL;

    if(result == HAL_SUCCESS) return TCL_OK;
    halError(interp, result);
    return TCL_ERROR;
}

int Hal_Init(Tcl_Interp *interp) {
    int result = init();
    if(result != HAL_SUCCESS) {
	Tcl_ResetResult(interp);
	halError(interp, result);
	return TCL_ERROR;
    }

    Tcl_CreateCommand(interp, "hal", halCmd, 0, halExit);

    return TCL_OK;
}

#ifndef BUFFERLEN
#define BUFFERLEN 1024
#endif

void halcmd_output(const char *format, ...) {
    char buf[BUFFERLEN + 1];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, BUFFERLEN, format, ap);
    va_end(ap);

    Tcl_AppendResult(target_interp, buf, NULL);
}

