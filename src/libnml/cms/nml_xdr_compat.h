/* Fixed-width XDR names used by cms_xup.cc but missing from some system
   RPC/XDR implementations. Prefer declared native routines; fill only gaps.
   HAVE_DECL_* come from config.h, which must be included first.

   License: LGPL Version 2 */
#ifndef NML_XDR_COMPAT_H
#define NML_XDR_COMPAT_H

#include <rpc/rpc.h>
#include <stdint.h>

/* An undefined HAVE_DECL_* would read as 0 below and mask a native routine. */
#if !defined(HAVE_DECL_XDR_INT8_T) || !defined(HAVE_DECL_XDR_UINT8_T) || \
    !defined(HAVE_DECL_XDR_UINT16_T) || !defined(HAVE_DECL_XDR_UINT32_T) || \
    !defined(HAVE_DECL_XDR_UINT64_T)
#error "XDR declaration results are missing (HAVE_DECL_XDR_*)"
#endif

/* Alternate unsigned names with the same 4/8-byte XDR encodings. */
#if !HAVE_DECL_XDR_UINT16_T
#define xdr_uint16_t xdr_u_int16_t
#endif
#if !HAVE_DECL_XDR_UINT32_T
#define xdr_uint32_t xdr_u_int32_t
#endif
#if !HAVE_DECL_XDR_UINT64_T
#define xdr_uint64_t xdr_u_int64_t
#endif

/* One 32-bit XDR word; decode narrows without range checks, as in
   glibc/libtirpc. Failed decodes leave *p unchanged. */
#if !HAVE_DECL_XDR_INT8_T
static inline bool_t nml_xdr_int8_t(XDR *xdrs, int8_t *p)
{
    int32_t t;
    switch (xdrs->x_op) {
    case XDR_ENCODE:
        t = *p;
        return (xdr_int32_t)(xdrs, &t);
    case XDR_DECODE:
        if (!(xdr_int32_t)(xdrs, &t))
            return FALSE;
        *p = (int8_t) t;
        return TRUE;
    case XDR_FREE:
        return TRUE;
    }
    return FALSE;
}
#define xdr_int8_t nml_xdr_int8_t
#endif

#if !HAVE_DECL_XDR_UINT8_T
static inline bool_t nml_xdr_uint8_t(XDR *xdrs, uint8_t *p)
{
    uint32_t t;
    switch (xdrs->x_op) {
    case XDR_ENCODE:
        t = *p;
        return (xdr_u_int32_t)(xdrs, &t);
    case XDR_DECODE:
        if (!(xdr_u_int32_t)(xdrs, &t))
            return FALSE;
        *p = (uint8_t) t;
        return TRUE;
    case XDR_FREE:
        return TRUE;
    }
    return FALSE;
}
#define xdr_uint8_t nml_xdr_uint8_t
#endif

#endif
