#include <tcl.h>
#include "halcmd.h"

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


static void halExit(ClientData d) {
    halcmd_shutdown();
}

static int halCmd(ClientData cd, Tcl_Interp *interp, int argc, const char **argv) {
    int result = parse_cmd((char **)argv+1);
    Tcl_ResetResult(interp);
    if(result == HAL_SUCCESS) return TCL_OK;
    halError(interp, result);
    return TCL_ERROR;
}

int Hal_Init(Tcl_Interp *interp) {
    int result = halcmd_startup();
    if(result != HAL_SUCCESS) {
	Tcl_ResetResult(interp);
	halError(interp, result);
	return TCL_ERROR;
    }

    Tcl_CreateCommand(interp, "hal", halCmd, 0, halExit);

    return TCL_OK;
}
