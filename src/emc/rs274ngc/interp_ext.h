/********************************************************************
* Description: interp_ext.h
*   Extension API for the RS274NGC interpreter.
*   Allows cmod/gomod handlers to register remap prologs/epilogs
*   and O-word subroutine handlers, replacing the removed Python
*   dispatch.
*
*   Types are defined in GMI-generated headers:
*     interp_ctx_api.h  — interp_ctx_callbacks_t (handler context)
*     interp_ext_api.h  — callback typedefs + registration struct
*
*   This header provides the C-linkage functions implemented in
*   interp_ext.cc (exported from librs274.so).
*
* License: GPL Version 2
********************************************************************/
#ifndef INTERP_EXT_H
#define INTERP_EXT_H

#define INTERP_CTX_API_CGO  // skip gomc_api.h registry helpers
#define INTERP_EXT_API_CGO
#include "interp_ext_api.h"
#undef INTERP_CTX_API_CGO
#undef INTERP_EXT_API_CGO

#ifdef __cplusplus
extern "C" {
#endif

/* --- C-linkage registration API (exported from librs274.so) --- */

int interp_ext_register_oword(void *interp, const char *name,
                              interp_ext_oword_fn_cb fn, void *user);

int interp_ext_register_remap_prolog(void *interp, const char *name,
                                     interp_ext_remap_prolog_fn_cb fn, void *user);

int interp_ext_register_remap_epilog(void *interp, const char *name,
                                     interp_ext_remap_epilog_fn_cb fn, void *user);

/* Query whether a handler is registered (used by interpreter dispatch) */
int interp_ext_has_oword(void *interp, const char *name);
int interp_ext_has_remap_handler(void *interp, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* INTERP_EXT_H */
