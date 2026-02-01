/********************************************************************
 * Description: hal_pin_reader.c
 *   Simple HAL pin reader for userspace kinematics parameter refresh.
 *   Attaches to HAL shared memory and reads pin values by name.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "rtapi.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_pin_reader.h"

/* Static state for HAL attachment */
static struct {
    int attached;
    int rtapi_module_id;
    int rtapi_shmem_id;
    char *hal_shmem_base;
    hal_data_t *hal_data;
} hal_reader_ctx;

/*
 * Attach to HAL shared memory
 */
static int hal_reader_attach(void)
{
    void *mem;
    int retval;

    if (hal_reader_ctx.attached) {
        return 0;
    }

    hal_reader_ctx.rtapi_module_id = rtapi_init("hal_pin_reader");
    if (hal_reader_ctx.rtapi_module_id < 0) {
        return -1;
    }

    hal_reader_ctx.rtapi_shmem_id = rtapi_shmem_new(HAL_KEY,
                                                     hal_reader_ctx.rtapi_module_id,
                                                     HAL_SIZE);
    if (hal_reader_ctx.rtapi_shmem_id < 0) {
        rtapi_exit(hal_reader_ctx.rtapi_module_id);
        return -1;
    }

    retval = rtapi_shmem_getptr(hal_reader_ctx.rtapi_shmem_id, &mem);
    if (retval < 0) {
        rtapi_shmem_delete(hal_reader_ctx.rtapi_shmem_id,
                           hal_reader_ctx.rtapi_module_id);
        rtapi_exit(hal_reader_ctx.rtapi_module_id);
        return -1;
    }

    hal_reader_ctx.hal_shmem_base = (char *)mem;
    hal_reader_ctx.hal_data = (hal_data_t *)mem;

    if (hal_reader_ctx.hal_data->version != HAL_VER) {
        rtapi_shmem_delete(hal_reader_ctx.rtapi_shmem_id,
                           hal_reader_ctx.rtapi_module_id);
        rtapi_exit(hal_reader_ctx.rtapi_module_id);
        hal_reader_ctx.hal_shmem_base = NULL;
        hal_reader_ctx.hal_data = NULL;
        return -1;
    }

    hal_reader_ctx.attached = 1;
    return 0;
}

/*
 * Find a pin by name
 */
static hal_pin_t *find_pin_by_name(const char *name)
{
    int next;
    hal_pin_t *pin;
    hal_oldname_t *oldname;
    hal_data_t *hd = hal_reader_ctx.hal_data;
    char *base = hal_reader_ctx.hal_shmem_base;

    if (!hd || !base) return NULL;

    next = hd->pin_list_ptr;
    while (next != 0) {
        pin = (hal_pin_t *)(base + next);

        if (strcmp(pin->name, name) == 0) {
            return pin;
        }

        if (pin->oldname != 0) {
            oldname = (hal_oldname_t *)(base + pin->oldname);
            if (strcmp(oldname->name, name) == 0) {
                return pin;
            }
        }

        next = pin->next_ptr;
    }

    return NULL;
}

/*
 * Get pointer to pin data
 */
static void *get_pin_data_ptr(hal_pin_t *pin)
{
    hal_sig_t *sig;
    hal_data_u *data;
    char *base = hal_reader_ctx.hal_shmem_base;

    if (pin->signal != 0) {
        sig = (hal_sig_t *)(base + pin->signal);
        data = (hal_data_u *)(base + sig->data_ptr);
    } else {
        data = &pin->dummysig;
    }

    switch (pin->type) {
        case HAL_BIT:   return (void *)&data->b;
        case HAL_FLOAT: return (void *)&data->f;
        case HAL_S32:   return (void *)&data->s;
        case HAL_U32:   return (void *)&data->u;
        case HAL_S64:   return (void *)&data->ls;
        case HAL_U64:   return (void *)&data->lu;
        default:        return (void *)data;
    }
}

/*
 * Public API: Read float pin
 */
int hal_pin_reader_read_float(const char *name, double *value)
{
    hal_pin_t *pin;
    void *data_ptr;

    if (!name || !value) return -1;
    if (hal_reader_attach() != 0) return -1;

    pin = find_pin_by_name(name);
    if (!pin || pin->type != HAL_FLOAT) return -1;

    data_ptr = get_pin_data_ptr(pin);
    *value = *((volatile hal_float_t *)data_ptr);
    return 0;
}

/*
 * Public API: Read bit pin
 */
int hal_pin_reader_read_bit(const char *name, int *value)
{
    hal_pin_t *pin;
    void *data_ptr;

    if (!name || !value) return -1;
    if (hal_reader_attach() != 0) return -1;

    pin = find_pin_by_name(name);
    if (!pin || pin->type != HAL_BIT) return -1;

    data_ptr = get_pin_data_ptr(pin);
    *value = *((volatile hal_bit_t *)data_ptr) ? 1 : 0;
    return 0;
}

/*
 * Public API: Read s32 pin
 */
int hal_pin_reader_read_s32(const char *name, int *value)
{
    hal_pin_t *pin;
    void *data_ptr;

    if (!name || !value) return -1;
    if (hal_reader_attach() != 0) return -1;

    pin = find_pin_by_name(name);
    if (!pin || pin->type != HAL_S32) return -1;

    data_ptr = get_pin_data_ptr(pin);
    *value = *((volatile hal_s32_t *)data_ptr);
    return 0;
}
