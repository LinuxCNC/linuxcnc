//
// HAL non-RT support API
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
#define __HAL_LIBRARY_INTERNAL_ONLY 1
#include "hal_priv.h"

//
// Allow user-land to forcefully release the HAL mutex.
//
// WARNING: This may crash the rest of the LCNC application(s).
//
int hal_mutex_force_release(void)
{
    if(!hal_data) {
        // The HAL shared memory segment was not mapped, do so now
        int comp_id = rtapi_init("hal_unlocker");
        if(comp_id < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "error: hal_mutex_force_release: rtapi_init failed\n");
            return comp_id;
        }
        int mem_id = rtapi_shmem_new(HAL_KEY, comp_id, HAL_SIZE);
        if(mem_id < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "error: hal_mutex_force_release: could not open shared memory\n");
            rtapi_exit(comp_id);
            return mem_id;
        }
        int rv = rtapi_shmem_getptr(mem_id, (void **)&hal_shmem_base);
        if (rv < 0) {
            hal_shmem_base = NULL;
            rtapi_print_msg(RTAPI_MSG_ERR, "error: hal_mutex_force_release: could not access shared memory\n");
            rtapi_shmem_delete(mem_id, comp_id);
            rtapi_exit(comp_id);
            return rv;
        }
        hal_data = (hal_data_t *)hal_shmem_base;

        halpr_mutex_force_release();

        // We're done, cleanup our mapping
        rtapi_shmem_delete(mem_id, comp_id);
        rtapi_exit(comp_id);
        hal_data = NULL;
        hal_shmem_base = NULL;
    } else {
        // We already have a mapped segment, use it
        halpr_mutex_force_release();
    }
    return 0;
}

//
// HAL will pretend that the exact base period requested is possible.
// This mode is not suitable for running real hardware.
// Returns zero (0) on success or a negative -EACCES error if already set.
//
int hal_enforce_exact_base_period(void)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_DBG, "hal_enforce_exact_base_period: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    halpr_mutex_acquire();
    if(0 != hal_data->exact_base_period) {
        halpr_mutex_release();
        return -EACCES;
    }
    hal_data->exact_base_period = 1;
    halpr_mutex_release();
    return 0;
}

//
// Set the insmod arguments in a named component
// This call is used when loading is (nearly) done
//
int hal_comp_insmod_args(const char *compname, const char *args)
{
    if(NULL == hal_data) {
        rtapi_print_msg(RTAPI_MSG_DBG, "hal_comp_insmod_args: HAL shared memory not mapped\n");
        return -EFAULT;
    }
    if(!compname || !args) {
        rtapi_print_msg(RTAPI_MSG_DBG, "hal_comp_insmod_args: Invalid arguments\n");
        return -EINVAL;
    }
    if(!SHMCHK(args)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hal_comp_insmod_args: 'args' not in HAL memory\n");
        return -EINVAL;
    }

    halpr_mutex_acquire();
    hal_comp_t *comp = halpr_find_comp_by_name(compname);
    if(!comp) {
        halpr_mutex_release();
        rtapi_print_msg(RTAPI_MSG_DBG, "hal_comp_insmod_args: Component '%s' not found\n", compname);
        return -ENOENT;
    }
    comp->insmod_args = SHMOFF(args);
    halpr_mutex_release();
    return 0;
}
