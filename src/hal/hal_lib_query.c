//
// HAL non-RT query API
//
// Copyright 2026  B.Stultiens
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of version 2 of the GNU General
// Public License as published by the Free Software Foundation.
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "hal.h"
#include "hal_priv.h"

//=====================================================
//
// HAL query API implementation of gets/sets and getp/setp
//
// Additionally adds retrieving the pin/param/signal
// reference for reading and writing using hal_getref_[ps].
//
// Replaces all (re-)implementations in other programs as
// one standard and unified API.
//
// NOTE: This API is not exported in the kernel.
//       It is a user-land thing and should only
//       be called from non-RT applications.
//
// Iteration of HAL's internal data structures is done using
// the hal_list_* functions. These iterate over any and all
// HAL structure with a user provided callback and may be
// used to extract useful data.
//
//=====================================================

static int set_common(hal_type_t type, const hal_query_value_u *v, hal_refs_u u, bool setport, bool isparam)
{
    // This is a special case because compatibility requires us to write to the
    // smaller field when addressing parameters. An old-style param has a
    // user-defined storage that may not be large enough for our new types and
    // would overwrite other memory.
    // FIXME: This code must be retired when we do the API break.
    if(isparam) {
        switch(type) {
        case HAL_BOOL: ((hal_data_u *)u.b)->b  = v->b; break;
        case HAL_S32:  ((hal_data_u *)u.s)->s  = v->s; break;
        case HAL_U32:  ((hal_data_u *)u.u)->u  = v->u; break;
        case HAL_SINT: ((hal_data_u *)u.s)->ls = v->s; break;
        case HAL_UINT: ((hal_data_u *)u.u)->lu = v->u; break;
        case HAL_REAL: ((hal_data_u *)u.r)->f  = v->r; break;
        default:
        case HAL_PORT: return -EBADF;
        }
        return 0;
    }
    switch(type) {
    case HAL_BOOL: hal_set_bool(u.b, v->b); break;
    case HAL_S32:  hal_set_si32(u.s, v->s); break;
    case HAL_U32:  hal_set_ui32(u.u, v->u); break;
    case HAL_SINT: hal_set_sint(u.s, v->s); break;
    case HAL_UINT: hal_set_uint(u.u, v->u); break;
    case HAL_REAL: hal_set_real(u.r, v->r); break;
    case HAL_PORT: {
        if(!setport)
            return -EBADF;
        // FIXME: This needs to be changed when the old API is removed
        rtapi_port port = hal_get_sint(u.s);
        if(0 != port && hal_port_buffer_size((hal_port_t *)u.s) > 0) {
            return -EISCONN;
        }
        int rv = halpr_port_alloc(v->u, (hal_port_t *)u.s);
        if(rv)
            return rv;
        } break;
    default:
        return -EBADF;
    }
    return 0;
}

//
// hal_set_s() - Set the value of a signal
//
// Query inputs:
//   q->name      - Signal to set
//   q->sig.type  - Optional: must match specific HAL_X type
//                  The q->sig.value must be set to the proper value,
//                  which will be used, if there is no callback set.
//                  The type is mandatory if no callback is specified.
//   q->sig.value - Mandatory if no callback is specified.
//
int hal_set_s(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_s: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !q->name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_s: Invalid query arguments\n");
        return -EINVAL;
    }
    if(0 == q->sig.type && !cb) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_s: Must have callback if type is not specified\n");
        return -EINVAL;
    }

    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_type_t qtp = q->sig.type; // Save original request signal type
    hal_sig_t *sig = halpr_find_sig_by_name(q->name);
    if(!sig) {
        // Signal name not found
        halpr_mutex_release();
        return -ENOENT;
    }
    if(HAL_PORT != sig->type && sig->writers > 0) {
        // Signal is not a port and has writers
        halpr_mutex_release();
        return -EACCES;
    }

    // Setup callback data
    q->qtype       = HAL_QTYPE_SIGNAL; // Handling a signal
    q->name        = sig->name;
    q->sig.type    = sig->type;       // of this type
    q->sig.writers = sig->writers;    // Signals have no direction
    q->sig.readers = sig->readers;    // Signals have no alias
    q->sig.bidirs  = sig->bidirs;     // Only relevant for pins

    if(!sig->data_ptr) {
        // Signal's data pointer is invalid
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_s: Signal '%s' has no data target\n", q->name);
        return -EIO;
    }
    q->sig.ref = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);

    if(0 != qtp && qtp != sig->type) {
        // Specific type requested and it didn't match
        halpr_mutex_release();
        return -EEXIST;
    }

    int rv = cb ? cb(q, arg) : 0;  // Callback to get the value

    if(!rv) {
        // Success, data to write must be in the value union
        hal_refs_u u = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);
        if((rv = set_common(sig->type, &q->sig.value, u, 1, 0)) < 0) {
            halpr_mutex_release();
            if(-EBADF == rv)
                rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_s: Signal '%s' has bad type %d\n", q->name, sig->type);
            return rv;
        }
    } // else something went wrong in the callback

    halpr_mutex_release();
    return rv;
}

//
// hal_set_p() - Set the value of a pin/param
//
// Returns an error if it matches a pin and it is connected.
//
// Query inputs:
//   q->name     - Pin/param to set
//   q->qtype    - Optional: HAL_QTYPE_PIN or HAL_QTYPE_PARAM to limit type
//   q->pp.type  - Optional: must match specific HAL_X type
//                 The q->pp.value must be set to the proper value,
//                 which will be used, if there is no callback set.
//                 The type is mandatory if no callback is specified.
//   q->pp.value - Mandatory if no callback is specified.
//
int hal_set_p(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_p: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !q->name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_p: Invalid query arguments\n");
        return -EINVAL;
    }
    if(0 == q->pp.type && !cb) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_p: Must have callback if type is not specified\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_type_t qtp = q->pp.type; // Save original request pin/param type
    hal_type_t t;
    hal_pin_t *pin = NULL;
    hal_refs_u u;
    bool isparam;
    // If only parameters requested, don't look for pins
    if(HAL_QTYPE_PARAM != q->qtype)
        pin = halpr_find_pin_by_name(q->name);
    if(!pin) {
        if(HAL_QTYPE_PIN == q->qtype) {
            // Only pin search requested and it was not found
            halpr_mutex_release();
            return -ENOENT;
        }
        // Not a pin, try param
        hal_param_t *param = halpr_find_param_by_name(q->name);
        if(!param) {
            // Neither pin nor param name found
            halpr_mutex_release();
            return -ENOENT;
        }
        // We have the param, copy the data, but we still can fail
        q->qtype     = HAL_QTYPE_PARAM;
        q->name      = param->name;
        q->pp.type   = t = param->type;
        q->pp.ref    = (hal_refs_u)(hal_sint_t)SHMPTR(param->data_ptr);
        q->pp.dir    = param->dir;
        q->pp.alias  = param->oldname ? ((hal_oldname_t *)SHMPTR(param->oldname))->name : NULL;
        q->pp.signal = NULL;
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(param->owner_ptr);
        q->pp.comp       = comp->name;
        q->pp.comp_id    = comp->comp_id;

        if(0 != qtp && qtp != param->type) {
            // Specific type requested and it didn't match
            halpr_mutex_release();
            return -EEXIST;
        }
        if(HAL_RO == param->dir) {
            // Param not writable
            halpr_mutex_release();
            return -EACCES;
        }
        if(!param->data_ptr) {
            // Param's data pointer is invalid
            halpr_mutex_release();
            rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_p: Param '%s' has no data target\n", q->name);
            return -EIO;
        }
        // FIXME: This can be targeted to param's data when the old API is retired
        u = (hal_refs_u)(hal_sint_t)SHMPTR(param->data_ptr);
        isparam = 1;
    } else {
        // We have a pin, copy the data, but we still can fail
        q->qtype     = HAL_QTYPE_PIN;
        q->name      = pin->name;
        q->pp.type   = t = pin->type;
        q->pp.ref    = (hal_refs_u)(hal_sint_t)&pin->dummysig;
        q->pp.dir    = pin->dir;
        q->pp.alias  = pin->oldname ? ((hal_oldname_t *)SHMPTR(pin->oldname))->name : NULL;
        q->pp.signal = NULL;
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(pin->owner_ptr);
        q->pp.comp       = comp->name;
        q->pp.comp_id    = comp->comp_id;

        // We still record the signal, even if we fail to write
        if(0 != pin->signal) {
            hal_sig_t *sig = (hal_sig_t *)SHMPTR(pin->signal);
            q->pp.signal = sig->name;
        }
        if(0 != qtp && qtp != pin->type) {
            // Specific type requested and it didn't match
            halpr_mutex_release();
            return -EEXIST;
        }
        if(HAL_OUT == pin->dir || 0 != pin->signal) {
            // Pin not writable
            halpr_mutex_release();
            return -EACCES;
        }
        u = (hal_refs_u)(hal_sint_t)&pin->dummysig;
        isparam = 0;
    }

    // Note: the callback is called while holding the mutex
    int rv = cb ? cb(q, arg) : 0; // Callback to get the value

    if(!rv) {
        // Success, data to write must be in the value union
        if((rv = set_common(t, &q->pp.value, u, 0, isparam)) < 0) {
            halpr_mutex_release();
            if(-EBADF == rv)
                rtapi_print_msg(RTAPI_MSG_ERR, "hal_set_p: %s '%s' has bad type %d\n", pin ? "Pin" : "Param", q->name, t);
            return rv;
        }
    } // else something went wrong in the callback

    halpr_mutex_release();
    return rv;
}

static int get_common(hal_type_t type, hal_query_value_u *v, hal_refs_u u, bool getport)
{
    switch(type) {
    case HAL_BOOL: v->b = hal_get_bool(u.b); break;
    case HAL_S32:  v->s = hal_get_si32(u.s); break;
    case HAL_U32:  v->u = hal_get_ui32(u.u); break;
    case HAL_SINT: v->s = hal_get_sint(u.s); break;
    case HAL_UINT: v->u = hal_get_uint(u.u); break;
    case HAL_REAL: v->r = hal_get_real(u.r); break;
    case HAL_PORT:
        if(!getport) {
            v->u = 0;
        } else {
            // FIXME: This needs to be changed when the old API is removed
            v->u = hal_port_buffer_size((hal_port_t *)u.s);
        }
        break;
    default:
        return -EBADF;
    }
    return 0;
}

//
// hal_get_s() - Retrieve the value of a signal
//
// Query inputs:
//   q->name     - Signal to get
//   q->sig.type - Optional: must match specific HAL_X type
//
int hal_get_s(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_get_s: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !q->name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_get_s: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_type_t qtp = q->sig.type; // Save original request signal type
    hal_sig_t *sig = halpr_find_sig_by_name(q->name);
    if(!sig) {
        // No signal by that name found
        halpr_mutex_release();
        return -ENOENT;
    }
    q->qtype       = HAL_QTYPE_SIGNAL;
    q->name        = sig->name;
    q->sig.type    = sig->type;
    q->sig.writers = sig->writers;
    q->sig.readers = sig->readers;
    q->sig.bidirs  = sig->bidirs;

    if(!sig->data_ptr) {
        // Signal's data pointer is invalid
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_get_s: Signal '%s' has no data target\n", q->name);
        return -EIO;
    }
    q->sig.ref = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);

    // We have the primary data copied, but fail if there is a type mismatch.
    if(0 != qtp && qtp != sig->type) {
        // A specific signal type was requested and it didn't match
        halpr_mutex_release();
        return -EEXIST;
    }
    int rv = get_common(sig->type, &q->sig.value, q->sig.ref, 1);
    if(!rv && cb)
        rv = cb(q, arg);
    halpr_mutex_release();
    return rv;
}

//
// hal_get_p() - Retrieve the value of a pin/param
//
// Returns the signal's value if a pin matches and it is connected.
//
// Query inputs:
//   q->name    - Pin/param to get
//   q->qtype   - Optional: HAL_QTYPE_PIN or HAL_QTYPE_PARAM to limit type
//   q->pp.type - Optional: must match specific HAL_X type
//
int hal_get_p(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_get_p: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !q->name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_get_p: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_type_t qtp = q->pp.type; // Save original request pin/param type
    hal_type_t t;
    hal_refs_u u;
    hal_pin_t *pin = NULL;
    // Only search for pins when requested
    if(HAL_QTYPE_PARAM != q->qtype)
        pin = halpr_find_pin_by_name(q->name);
    if(!pin) {
        if(HAL_QTYPE_PIN == q->qtype) {
            // Only pin search requested and it was not found
            halpr_mutex_release();
            return -ENOENT;
        }
        // Not a pin, try param
        hal_param_t *param = halpr_find_param_by_name(q->name);
        if(!param) {
            // Neither pin nor param name found
            halpr_mutex_release();
            return -ENOENT;
        }
        // We have the param, copy the data
        q->qtype     = HAL_QTYPE_PARAM;
        q->name      = param->name;
        q->pp.type   = t = param->type;
        q->pp.dir    = param->dir;
        q->pp.alias  = param->oldname ? ((hal_oldname_t *)SHMPTR(param->oldname))->name : NULL;
        q->pp.signal = NULL;
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(param->owner_ptr);
        q->pp.comp       = comp->name;
        q->pp.comp_id    = comp->comp_id;

        if(!param->data_ptr) {
            // Param's data pointer is invalid
            halpr_mutex_release();
            rtapi_print_msg(RTAPI_MSG_ERR, "hal_get_p: Param '%s' has no data target\n", q->name);
            return -EIO;
        }
        // FIXME: This can be targeted to param's data when the old API is retired
        q->pp.ref = u = (hal_refs_u)(hal_sint_t)SHMPTR(param->data_ptr);
    } else {
        // We have the pin, copy the data
        q->qtype    = HAL_QTYPE_PIN;
        q->name     = pin->name;
        q->pp.type  = t = pin->type;
        q->pp.dir   = pin->dir;
        q->pp.alias = pin->oldname ? ((hal_oldname_t *)SHMPTR(pin->oldname))->name : NULL;
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(pin->owner_ptr);
        q->pp.comp       = comp->name;
        q->pp.comp_id    = comp->comp_id;
        if(0 != pin->signal) {
            // Pin is connected, use the signal value
            hal_sig_t *sig = (hal_sig_t *)SHMPTR(pin->signal);
            q->pp.ref = u = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);
            q->pp.signal = sig->name;
        } else {
            // Unconnected, use the dummy value
            q->pp.ref = u = (hal_refs_u)(hal_sint_t)&pin->dummysig;
            q->pp.signal = NULL;
        }
    }

    // We have the primary data copied, but fail if there is a type mismatch.
    if(0 != qtp && qtp != t) {
        // Specific type requested and it didn't match
        halpr_mutex_release();
        return -EEXIST;
    }

    int rv = get_common(t, &q->pp.value, u, 0);
    if(!rv && cb)
        rv = cb(q, arg);
    halpr_mutex_release();
    return rv;
}

//
// hal_getref_s() - Retrieve the pin reference to read/write the signal
//
// Query inputs:
//   q->name    - Signal to get
//
int hal_getref_s(hal_query_t *q)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_getref_s: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !q->name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_getref_s: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_sig_t *sig = halpr_find_sig_by_name(q->name);
    if(!sig) {
        // Signal name not found
        halpr_mutex_release();
        return -ENOENT;
    }
    if(!sig->data_ptr) {
        // Signal's data pointer is invalid
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_getref_s: Signal '%s' has no data target\n", q->name);
        return -EIO;
    }
    // We have a signal. We don't really care whether it is writable or
    // readable. That is the caller's responsibility. This API is rather
    // low-level and only hides the HAL private stuff. The data access is
    // public as long as you use the hal_[gs]et_xxxx() interface.
    // Setup return data
    q->qtype       = HAL_QTYPE_SIGNAL; // Handling a signal
    q->name        = sig->name;
    q->sig.type    = sig->type;        // of this type
    q->sig.writers = sig->writers;
    q->sig.readers = sig->readers;
    q->sig.bidirs  = sig->bidirs;
    q->sig.ref     = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);
    halpr_mutex_release();
    return 0;
}

//
// hal_getref_p() - Retrieve the pin reference to read/write the pin/param
//
// It will return the signal reference if the pin is connected.
//
// Query inputs:
//   q->name    - Pin/param to get
//
int hal_getref_p(hal_query_t *q)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_getref_p: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !q->name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_getref_p: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_pin_t *pin = NULL;
    // If only parameters requested, don't look for pins
    if(q->qtype != HAL_QTYPE_PARAM)
        pin = halpr_find_pin_by_name(q->name);
    if(!pin) {
        if(q->qtype == HAL_QTYPE_PIN) {
            // Only pin search requested and it was not found
            halpr_mutex_release();
            return -ENOENT;
        }
        // Not a pin, try param
        hal_param_t *param = halpr_find_param_by_name(q->name);
        if(!param) {
            // Neither pin nor param name found
            halpr_mutex_release();
            return -ENOENT;
        }
        // We have the param, copy the data, but we still can fail
        q->qtype     = HAL_QTYPE_PARAM;
        q->name      = param->name;
        q->pp.type   = param->type;
        q->pp.dir    = param->dir;
        q->pp.alias  = param->oldname ? ((hal_oldname_t *)SHMPTR(param->oldname))->name : NULL;
        q->pp.signal = NULL;
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(param->owner_ptr);
        q->pp.comp       = comp->name;
        q->pp.comp_id    = comp->comp_id;

        if(!param->data_ptr) {
            // Param's data pointer is invalid
            halpr_mutex_release();
            rtapi_print_msg(RTAPI_MSG_ERR, "hal_getref_p: Param '%s' has no data target\n", q->name);
            return -EIO;
        }
        // FIXME: This can be targeted to param's data when the old API is retired
        q->pp.ref = (hal_refs_u)(hal_sint_t)SHMPTR(param->data_ptr);
    } else {
        // We have a pin, copy the data
        q->qtype    = HAL_QTYPE_PIN;
        q->name     = pin->name;
        q->pp.type  = pin->type;
        q->pp.dir   = pin->dir;
        q->pp.alias = pin->oldname ? ((hal_oldname_t *)SHMPTR(pin->oldname))->name : NULL;
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(pin->owner_ptr);
        q->pp.comp       = comp->name;
        q->pp.comp_id    = comp->comp_id;

        if(0 != pin->signal) {
            // The pin is connected, return the signal ref
            hal_sig_t *sig = (hal_sig_t *)SHMPTR(pin->signal);
            q->pp.ref      = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);
            q->pp.signal   = sig->name;
        } else {
            // Just the dummy value
            q->pp.ref    = (hal_refs_u)(hal_sint_t)&pin->dummysig;
            q->pp.signal = NULL;
        }
    }

    halpr_mutex_release();
    return 0;
}

//
// Iterate all pins/params with a callback
//
// Query inputs:
//   q->qtype   - Optional: HAL_QTYPE_PIN or HAL_QTYPE_PARAM to limit type
//
int hal_list_p(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_p: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !cb || (0 != q->qtype && !(HAL_QTYPE_PIN == q->qtype || HAL_QTYPE_PARAM == q->qtype))) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_p: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_qtype_t qtype = q->qtype; // Need to cache because field is reused

    // FIXME: this can be mostly merged once we merge pin/param structures
    if(!qtype || HAL_QTYPE_PIN == qtype) {
        rtapi_intptr_t pinref = hal_data->pin_list_ptr;
        while(pinref) {
            hal_pin_t *pin = (hal_pin_t *)SHMPTR(pinref);
            q->name     = pin->name;
            q->qtype    = HAL_QTYPE_PIN;
            q->pp.type  = pin->type;
            q->pp.dir   = pin->dir;
            q->pp.alias = pin->oldname ? ((hal_oldname_t *)SHMPTR(pin->oldname))->name : NULL;
            hal_comp_t *comp = (hal_comp_t *)SHMPTR(pin->owner_ptr);
            q->pp.comp       = comp->name;
            q->pp.comp_id    = comp->comp_id;
            if(pin->signal) {
                hal_sig_t *sig = (hal_sig_t *)SHMPTR(pin->signal);
                q->pp.ref = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);
                q->pp.signal = sig->name;
            } else {
                q->pp.ref = (hal_refs_u)(hal_sint_t)&pin->dummysig;
                q->pp.signal = NULL;
            }
            int rv = get_common(q->pp.type, &q->pp.value, q->pp.ref, 0);
            if(0 != rv) {
                // Non-zero return from get_common means inconsistent pin
                halpr_mutex_release();
                return rv;
            }
            if(0 != (rv = cb(q, arg))) {
                // Non-zero return from the callback breaks the loop and we're done
                halpr_mutex_release();
                return rv;
            }
            pinref = pin->next_ptr;
        }
    }
    if(!qtype || HAL_QTYPE_PARAM == qtype) {
        rtapi_intptr_t paramref = hal_data->param_list_ptr;
        while(paramref) {
            hal_param_t *param = (hal_param_t *)SHMPTR(paramref);
            q->name      = param->name;
            q->qtype     = HAL_QTYPE_PARAM;
            q->pp.type   = param->type;
            q->pp.dir    = param->dir;
            q->pp.alias  = param->oldname ? ((hal_oldname_t *)SHMPTR(param->oldname))->name : NULL;
            q->pp.signal = NULL;
            q->pp.ref    = (hal_refs_u)(hal_sint_t)SHMPTR(param->data_ptr);
            hal_comp_t *comp = (hal_comp_t *)SHMPTR(param->owner_ptr);
            q->pp.comp       = comp->name;
            q->pp.comp_id    = comp->comp_id;
            int rv = get_common(q->pp.type, &q->pp.value, q->pp.ref, 0);
            if(0 != rv) {
                // Non-zero return from get_common means inconsistent param
                halpr_mutex_release();
                return rv;
            }
            if(0 != (rv = cb(q, arg))) {
                // Non-zero return from the callback breaks the loop and we're done
                halpr_mutex_release();
                return rv;
            }
            paramref = param->next_ptr;
        }
    }

    halpr_mutex_release();
    return 0;
}

//
// Iterate all pins and find those connected to the signal specified
//
// Query inputs:
//   q->name   - Signal to search for
//
int hal_list_p_s(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_p_s: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !q->name || !cb) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_p_s: Invalid query arguments\n");
        return -EINVAL;
    }

    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    hal_sig_t *sig = halpr_find_sig_by_name(q->name);
    if(!sig) {
        halpr_mutex_release();
        return -ENOENT; // Signal not found
    }

    int rv = 0;
    hal_pin_t *pin = halpr_find_pin_by_sig(sig, NULL);  // Find first
    while(NULL != pin) {
        q->name      = pin->name;
        q->qtype     = HAL_QTYPE_PIN;
        q->pp.type   = pin->type;
        q->pp.dir    = pin->dir;
        q->pp.alias  = pin->oldname ? ((hal_oldname_t *)SHMPTR(pin->oldname))->name : NULL;
        q->pp.signal = sig->name;
        q->pp.ref    = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(pin->owner_ptr);
        q->pp.comp       = comp->name;
        q->pp.comp_id    = comp->comp_id;
 
        rv = get_common(q->pp.type, &q->pp.value, q->pp.ref, 0);
        if(0 != rv) {
            // Non-zero return from get_common means inconsistent pin
            break;
        }
        if(0 != (rv = cb(q, arg))) {
            // Callback requested exit
            break;
        }
        pin = halpr_find_pin_by_sig(sig, pin);  // Find next
    }

    halpr_mutex_release();
    return rv;
}

//
// Iterate all signals with a callback
//
// Query inputs: None
//
int hal_list_s(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_s: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !cb) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_s: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    int rv = 0;
    rtapi_intptr_t sigref = hal_data->sig_list_ptr;
    while(sigref) {
        hal_sig_t *sig = (hal_sig_t *)SHMPTR(sigref);
        q->name        = sig->name;
        q->qtype       = HAL_QTYPE_SIGNAL;
        q->sig.type    = sig->type;
        q->sig.writers = sig->writers;
        q->sig.readers = sig->readers;
        q->sig.bidirs  = sig->bidirs;
        // If someone wants to find the connected pins, then call
        // hal_list_p_s() to get the full list.
        q->sig.ref     = (hal_refs_u)(hal_sint_t)SHMPTR(sig->data_ptr);
        rv = get_common(q->sig.type, &q->sig.value, q->sig.ref, 1);
        if(0 != rv) {
            // Non-zero return from get_common means inconsistent signal
            break;
        }
        if(0 != (rv = cb(q, arg))) {
            // Non-zero return from the callback breaks the loop and we're done
            break;
        }
        sigref = sig->next_ptr;
    }

    halpr_mutex_release();
    return rv;
}

//
// Iterate all components with a callback
//
// Query inputs: None
//
int hal_list_comp(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_comp: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !cb) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_comp: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    int rv = 0;
    rtapi_intptr_t compref = hal_data->comp_list_ptr;
    while(compref) {
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(compref);
        q->name         = comp->name;
        q->qtype        = HAL_QTYPE_COMP;
        q->comp.type    = comp->type;
        q->comp.comp_id = comp->comp_id;
        q->comp.pid     = comp->pid;
        q->comp.ready   = !!comp->ready;
        q->comp.insmod  = comp->insmod_args ? (const char *)SHMPTR(comp->insmod_args) : NULL;
        if(0 != (rv = cb(q, arg))) {
            // Non-zero return from the callback breaks the loop and we're done
            break;
        }
        compref = comp->next_ptr;
    }

    halpr_mutex_release();
    return rv;
}

//
// Iterate all functions with a callback
//
// Query inputs: None
//
int hal_list_funct(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_funct: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !cb) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_funct: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    int rv = 0;
    rtapi_intptr_t functref = hal_data->funct_list_ptr;
    while(functref) {
        hal_funct_t *funct = (hal_funct_t *)SHMPTR(functref);
        q->name          = funct->name;
        q->qtype         = HAL_QTYPE_FUNCT;
        hal_comp_t *comp = (hal_comp_t *)SHMPTR(funct->owner_ptr);
        q->funct.comp_id = comp->comp_id;
        q->funct.comp    = comp->name;
        q->funct.users   = funct->users;
        q->funct.arg     = (rtapi_intptr_t)funct->arg;   // Purely informational
        q->funct.funct   = (rtapi_intptr_t)funct->funct; // Purely informational
        q->funct.reentrant = !!funct->reentrant;
        if(0 != (rv = cb(q, arg))) {
            // Non-zero return from the callback breaks the loop and we're done
            break;
        }
        functref = funct->next_ptr;
    }

    halpr_mutex_release();
    return rv;
}

//
// Iterate all threads with a callback
//
// Query inputs:
//   q->qtype - Optional: HAL_QTYPE_THREAD to only iterate threads or
//              HAL_QTYPE_THREAD_FUNCT to iterate threads *and* the functions
//              attached to the thread.
//
// Iteration of threads and its functions will call the callback with the
// thread's name first and the q->qtype set to HAL_QTYPE_THREAD. Then it will
// call the callback with q->qtype set to HAL_QTYPE_THREAD_FUNCT for each
// function attached to the thread.
//
int hal_list_thread(hal_query_t *q, hal_query_cb cb, void *arg)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_thread: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!q || !cb) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_list_thread: Invalid query arguments\n");
        return -EINVAL;
    }
    bool dofuncts = q->qtype == HAL_QTYPE_THREAD_FUNCT;
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();

    int rv = 0;
    rtapi_intptr_t threadref = hal_data->thread_list_ptr;
    while(threadref) {
        hal_thread_t *thread = (hal_thread_t *)SHMPTR(threadref);
        q->name            = thread->name;
        q->thread.comp_id  = thread->comp_id;
        hal_comp_t *comp = halpr_find_comp_by_id(thread->comp_id);
        const char *compname = comp ? comp->name : "?";
        q->thread.comp = compname;
        q->thread.priority = thread->priority;
        q->thread.period   = thread->period;
        q->thread.functidx = 0;
        q->thread.funct    = NULL;
        q->thread.is_init  = 0;
        q->qtype           = HAL_QTYPE_THREAD;
        // Callback on the thread
        if(0 != (rv = cb(q, arg))) {
            // Non-zero return from the callback breaks the loop and we're done
            break;
        }
        if(dofuncts) {
            // Callback on the thread's normal functions
            hal_funct_entry_t *froot = (hal_funct_entry_t *)&thread->funct_list;
            hal_funct_entry_t *entry = (hal_funct_entry_t *)SHMPTR(froot->links.next);
            for(int cnt = 0; entry != froot; cnt++) {
                hal_funct_t *funct = (hal_funct_t *)SHMPTR(entry->funct_ptr);
                // Make sure all data is set
                q->name            = thread->name;
                q->thread.comp_id  = thread->comp_id;
                q->thread.comp = compname;
                q->thread.priority = thread->priority;
                q->thread.period   = thread->period;
                // Function level data
                q->qtype           = HAL_QTYPE_THREAD_FUNCT;
                q->thread.functidx = cnt;
                q->thread.funct    = funct->name;
                q->thread.is_init  = 0;
                if(0 != (rv = cb(q, arg))) {
                    // Non-zero return from the callback quits and we're done
                    halpr_mutex_release();
                    return rv;
                }
                entry = (hal_funct_entry_t *)SHMPTR(entry->links.next);
            }
            // Callback on the thread's init functions
            froot = (hal_funct_entry_t *)&thread->init_funct_list;
            entry = (hal_funct_entry_t *)SHMPTR(froot->links.next);
            for(int cnt = 0; entry != froot; cnt++) {
                hal_funct_t *funct = (hal_funct_t *)SHMPTR(entry->funct_ptr);
                // Make sure all data is set
                q->name            = thread->name;
                q->thread.comp_id  = thread->comp_id;
                q->thread.comp = compname;
                q->thread.priority = thread->priority;
                q->thread.period   = thread->period;
                // Function level data
                q->qtype           = HAL_QTYPE_THREAD_FUNCT;
                q->thread.functidx = cnt;
                q->thread.funct    = funct->name;
                q->thread.is_init  = 1;
                if(0 != (rv = cb(q, arg))) {
                    // Non-zero return from the callback quits and we're done
                    halpr_mutex_release();
                    return rv;
                }
                entry = (hal_funct_entry_t *)SHMPTR(entry->links.next);
            }
        }
        threadref = thread->next_ptr;
    }

    halpr_mutex_release();
    return rv;
}

//
// Return HAL statistics
//
static int count_list(rtapi_intptr_t ptr)
{
    int n = 0;
    while(0 != ptr) {
        n++;
        // The first field in the structure is the 'next_ptr' field
        ptr = *((rtapi_intptr_t *)SHMPTR(ptr));
    }
    return n;
}

int hal_statistics(hal_statistics_t *sts)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_statistics: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!sts) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_statistics: Missing statistics structure pointer argument\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    // (list access and iteration may otherwise fail)
    halpr_mutex_acquire();

    sts->mem_total = HAL_SIZE;
    sts->mem_free  = hal_data->shmem_avail;

    sts->ncomps        = count_list(hal_data->comp_list_ptr);
    sts->ncomps_free   = count_list(hal_data->comp_free_ptr);
    sts->nsignals      = count_list(hal_data->sig_list_ptr);
    sts->nsignals_free = count_list(hal_data->sig_free_ptr);
    sts->nfuncts       = count_list(hal_data->funct_list_ptr);
    sts->nfuncts_free  = count_list(hal_data->funct_free_ptr);
    sts->nthreads      = count_list(hal_data->thread_list_ptr);
    sts->nthreads_free = count_list(hal_data->thread_free_ptr);

    // Count pins, params and aliases in use
    int nalias = 0;
    int npin = 0;
    int nparam = 0;
    rtapi_intptr_t ptr = hal_data->pin_list_ptr;
    while(0 != ptr) {
        npin++;
        hal_pin_t *pin = (hal_pin_t *)SHMPTR(ptr);
        if(0 != pin->oldname)
            nalias++;
        ptr = pin->next_ptr;
    }
    ptr = hal_data->param_list_ptr;
    while(0 != ptr) {
        nparam++;
        hal_param_t *param = (hal_param_t *)SHMPTR(ptr);
        if(0 != param->oldname)
            nalias++;
        ptr = param->next_ptr;
    }
    sts->naliases      = nalias;
    sts->naliases_free = count_list(hal_data->oldname_free_ptr);
    sts->npins         = npin;
    sts->npins_free    = count_list(hal_data->pin_free_ptr);
    sts->nparams       = nparam;
    sts->nparams_free  = count_list(hal_data->param_free_ptr);

    halpr_mutex_release();
    return 0;
}

//
// Return the status of a component by ID
//
int hal_comp_by_id(int comp_id, hal_query_t *q)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_comp_by_id: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(comp_id <= 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_comp_by_id: Invalid query arguments\n");
        return -EINVAL;
    }
    // Grab the mutex to keep the data stable
    halpr_mutex_acquire();
    hal_comp_t *comp = halpr_find_comp_by_id(comp_id);

    if(NULL != comp && NULL != q) {
        q->name         = comp->name;
        q->qtype        = HAL_QTYPE_COMP;
        q->comp.type    = comp->type;
        q->comp.comp_id = comp->comp_id;
        q->comp.pid     = comp->pid;
        q->comp.ready   = !!comp->ready;
        q->comp.insmod  = comp->insmod_args ? (const char *)SHMPTR(comp->insmod_args) : NULL;
    }
    halpr_mutex_release();
    return NULL != comp ? 0 : -ENOENT;
}

//
// Return the status of a named component
//
int hal_comp_by_name(const char *name, hal_query_t *q)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_comp_by_name: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!name) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_comp_by_name: Missing name argument\n");
        return -EINVAL;
    }
    halpr_mutex_acquire();
    hal_comp_t *comp = halpr_find_comp_by_name(name);

    if(NULL != comp && NULL != q) {
        q->name         = comp->name;
        q->qtype        = HAL_QTYPE_COMP;
        q->comp.type    = comp->type;
        q->comp.comp_id = comp->comp_id;
        q->comp.pid     = comp->pid;
        q->comp.ready   = !!comp->ready;
        q->comp.insmod  = comp->insmod_args ? (const char *)SHMPTR(comp->insmod_args) : NULL;
    }
    halpr_mutex_release();
    return NULL != comp ? 0 : -ENOENT;
}
