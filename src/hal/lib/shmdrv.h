/********************************************************************
 * Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ********************************************************************/

#ifndef SHMEM_H
#define SHMEM_H

#define DEVICE_NAME "shmdrv"

// mapping of non-standard VM regions, e.g. attaching the HAL
// segment of an outboard
// executiion of any outboard-specific code is triggered by a flag
// passed to IOC_SHM_CREATE - see shm_nonstandard_attach() in shmdrv.c
#define OUTBOARD1_CREATE  0x1
#define OUTBOARD2_CREATE  0x2

#define OUTBOARD_MASK (OUTBOARD1_CREATE|OUTBOARD2_CREATE)

struct shm_status {
    int driver_fd; // not inspected or set by kernel
    int  key;
    size_t size;
    size_t act_size;
    void *addr;
    int flags;
    int id;
    int n_kattach;
    int n_uattach;
    int creator;
};

#ifdef __KERNEL__

// in-kernel API
// semantically equivalent to the IOC_SHM_* userland ioctls below

extern int shmdrv_status(struct shm_status *shmstat);
extern int shmdrv_attach(struct shm_status *shmstat, void **shm);
extern int shmdrv_create(struct shm_status *shmstat);
extern int shmdrv_detach(struct shm_status *shmstat);

// 0 - errors and warning messages only
// 1 - informational
// 2 - debug

#define dbg(format, arg...)						\
    do {								\
	if (debug > 1) pr_info(DEVICE_NAME ": %s: " format  "\n", __FUNCTION__ , ## arg);	\
    } while (0)

#define info(format, arg...)						\
    do {								\
	if (debug > 0) pr_info(DEVICE_NAME ": %s: " format  "\n", __FUNCTION__ , ## arg);	\
    } while (0)

#define err(format, arg...)  pr_err(DEVICE_NAME ": " format "\n", ## arg)
#define warn(format, arg...) pr_warn(DEVICE_NAME ": " format "\n", ## arg)

#else // userland

#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern int shmdrv_loaded;

// an mmap() failure returns MAP_FAILED, not NULL, so test for both
#define MMAP_OK(x) (((x) != NULL) && ((x) != MAP_FAILED))
#define PAGESIZE_ALIGN(x)  ((x) + (-(x) & (page_size - 1)))

extern int shmdrv_available(void);
extern int shmdrv_available(void);
extern int shmdrv_driver_fd(void);
extern int shmdrv_status(struct shm_status *shmstat);
extern int shmdrv_create(struct shm_status *shmstat);
extern int shmdrv_attach(struct shm_status *shmstat, void **shm);
extern int shmdrv_detach(struct shm_status *shmstat, void *shm);
extern int shmdrv_gc(void);
extern void shmdrv_print_status(struct shm_status *sm, const char *tag);

extern int shm_common_init(void);
extern int shm_common_new(int key, int *size, int instance, void **shmptr, int create);
extern int shm_common_detach(int size, void *shmptr);
extern int shm_common_exists(int key);
extern int shm_common_unlink(int key);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !__KERNEL__

#define SHM_IOC_MAGIC    'r'

#define IOC_SHM_STATUS   _IOWR(SHM_IOC_MAGIC, 1, struct shm_status)
#define IOC_SHM_CREATE   _IOWR(SHM_IOC_MAGIC, 2, struct shm_status)
#define IOC_SHM_ATTACH   _IOWR(SHM_IOC_MAGIC, 3, struct shm_status)
#define IOC_SHM_GC       _IO(SHM_IOC_MAGIC, 4)

#endif // SHMEM_H
