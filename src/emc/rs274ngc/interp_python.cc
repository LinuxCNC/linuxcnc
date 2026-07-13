/*    This is a component of LinuxCNC
 *    Copyright 2011, 2012 Michael Haberler <git@mah.priv.at>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
// Python support has been removed. Dispatch now goes through the
// extension handler registry (interp_ext.cc).

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "interp_ext.h"

int Interp::py_reload()
{
    // No-op: nothing to reload with ext handlers
    return INTERP_OK;
}

bool Interp::is_pycallable(setup_pointer settings,
                           const char *module,
                           const char *funcname)
{
    if (!funcname) return false;

    if (module && strcmp(module, OWORD_MODULE) == 0)
        return ext_has_oword(funcname);

    if (module && strcmp(module, REMAP_MODULE) == 0)
        return ext_has_remap_handler(funcname);

    return false;
}

int Interp::pycall(setup_pointer settings,
                   context_pointer frame,
                   const char *module,
                   const char *funcname,
                   int calltype)
{
    if (!funcname) {
        ERS("pycall: NULL function name");
        return INTERP_ERROR;
    }

    int status;

    switch (calltype) {
    case PY_PROLOG:
    case PY_FINISH_PROLOG:
        // Distinguish a genuine dispatch failure (no handler registered) from a
        // handler that ran and returned INTERP_ERROR: only the former is a
        // failure of the *call* itself.  A handler that fails will have set a
        // saved error via ctx->set_error(), which must be preserved.
        if (!ext_has_remap_handler(funcname))
            ERS("remap prolog handler '%s' not registered", funcname);
        status = ext_call_remap_prolog(funcname, calltype == PY_FINISH_PROLOG ? 1 : 0);
        break;

    case PY_EPILOG:
    case PY_FINISH_EPILOG:
        if (!ext_has_remap_handler(funcname))
            ERS("remap epilog handler '%s' not registered", funcname);
        status = ext_call_remap_epilog(funcname, calltype == PY_FINISH_EPILOG ? 1 : 0);
        break;

    case PY_OWORDCALL:
    case PY_FINISH_OWORDCALL: {
        if (!ext_has_oword(funcname))
            ERS("O-word handler '%s' not registered", funcname);
        // Positional args are the numbered subroutine params #1..#n_args, set
        // up by execute_call for the O-word call; n_args comes from the
        // OWORD_N_ARGS named param.
        double args[INTERP_SUB_PARAMS];
        int n_args = 0;
        int found = 0;
        double nv = 0;
        find_named_param("n_args", &found, &nv);
        if (found && nv > 0) {
            n_args = (int)nv;
            if (n_args > INTERP_SUB_PARAMS)
                n_args = INTERP_SUB_PARAMS;
        }
        for (int i = 0; i < n_args; i++)
            args[i] = settings->parameters[INTERP_FIRST_SUBROUTINE_PARAM + i];
        double retval = 0;
        status = ext_call_oword(funcname, args, n_args, &retval,
                                calltype == PY_FINISH_OWORDCALL ? 1 : 0);
        if (status == INTERP_EXT_OK) {
            settings->return_value = retval;
            settings->value_returned = 1;
        }
        break;
    }

    case PY_BODY:
    case PY_FINISH_BODY:
        ERS("pycall(%s): remap python body not supported - use NGC sub or register a handler",
            funcname);
        return INTERP_ERROR;

    default:
        ERS("pycall(%s.%s): handler not registered",
            module ? module : "", funcname);
        return INTERP_ERROR;
    }

    // Map ext return codes to interp return codes.  This is the *handler's*
    // returned status, not the status of the call: stash it for
    // handler_returned() to convey (mirroring the classic Python path, where a
    // handler returning an int INTERP_ERROR left pycall's own status INTERP_OK
    // and the error was surfaced later).  Returning it directly here would trip
    // the caller's CHKS(status == INTERP_ERROR, "pycall(...) failed") and
    // clobber the handler's saved error message with a generic one.
    int result;
    switch (status) {
    case INTERP_EXT_OK:
        result = INTERP_OK;
        break;
    case INTERP_EXT_EXECUTE_FINISH:
        result = INTERP_EXECUTE_FINISH;
        break;
    default:
        result = INTERP_ERROR;
        break;
    }

    // Store for handler_returned() to pick up
    if (frame)
        frame->pystuff.last_status = result;

    // The call itself dispatched successfully; the handler's own status travels
    // via last_status.
    return INTERP_OK;
}

int Interp::py_execute(const char *cmd, bool as_file)
{
    ERS("py_execute: Python support has been removed");
    return INTERP_ERROR;
}
