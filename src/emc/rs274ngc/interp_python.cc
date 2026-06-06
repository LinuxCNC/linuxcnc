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
        status = ext_call_remap_prolog(funcname, calltype == PY_FINISH_PROLOG ? 1 : 0);
        break;

    case PY_EPILOG:
    case PY_FINISH_EPILOG:
        status = ext_call_remap_epilog(funcname, calltype == PY_FINISH_EPILOG ? 1 : 0);
        break;

    case PY_OWORDCALL:
    case PY_FINISH_OWORDCALL: {
        // Gather positional args from the call frame's local params (#1..#30)
        double args[30];
        int n_args = 0;
        for (int i = 0; i < 30; i++) {
            char pname[8];
            snprintf(pname, sizeof(pname), "%d", i + 1);
            int found = 0;
            double val = 0;
            find_named_param(pname, &found, &val);
            if (!found) break;
            args[n_args++] = val;
        }
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

    // Map ext return codes to interp return codes
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

    return result;
}

int Interp::py_execute(const char *cmd, bool as_file)
{
    ERS("py_execute: Python support has been removed");
    return INTERP_ERROR;
}
