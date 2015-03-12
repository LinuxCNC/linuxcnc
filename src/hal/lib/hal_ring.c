
#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"
#include "hal_ring.h"		/* HAL ringbuffer decls */

static hal_ring_t *alloc_ring_struct(void);
static void free_ring_struct(hal_ring_t * p);
static int next_ring_id(void);
static int hal_ring_newfv(int size, int sp_size, int flags,
			  const char *fmt, va_list ap);
static int hal_ring_deletefv(const char *fmt, va_list ap);
static int hal_ring_attachfv(ringbuffer_t *rb, unsigned *flags,
			     const char *fmt, va_list ap);
static int hal_ring_detachfv(ringbuffer_t *rb, const char *fmt, va_list ap);

/***********************************************************************
*                     Public HAL ring functions                        *
************************************************************************/
int hal_ring_newf(int size, int sp_size, int mode, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_ring_newfv(size, sp_size, mode, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_ring_new(const char *name, int size, int sp_size, int mode)
{
    hal_ring_t *rbdesc;
    int *prev, next, cmp, retval;
    int ring_id;
    ringheader_t *rhptr;

    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_LOCK(HAL_LOCK_LOAD);

    {
	hal_ring_t *ptr __attribute__((cleanup(halpr_autorelease_mutex)));

	rtapi_mutex_get(&(hal_data->mutex));

	// make sure no such ring name already exists
	if ((ptr = halpr_find_ring_by_name(name)) != 0) {
	    HALERR("ring '%s' already exists", name);
	    return -EEXIST;
	}
	// allocate a new ring id - needed since we dont track ring shm
	// segments in RTAPI
	if ((ring_id = next_ring_id()) < 0) {
	    HALERR("cant allocate new ring id for '%s'", name);
	    return -ENOMEM;
	}

	// allocate a new ring descriptor
	if ((rbdesc = alloc_ring_struct()) == 0)
	    NOMEM("ring '%s'", name);

	rbdesc->handle = rtapi_next_handle();
	rbdesc->flags = mode;
	rbdesc->ring_id = ring_id;

	// make total allocation fit ringheader, ringbuffer and scratchpad
	rbdesc->total_size = ring_memsize( rbdesc->flags, size, sp_size);

	if (rbdesc->flags & ALLOC_HALMEM) {
	    void *ringmem = shmalloc_up(rbdesc->total_size);
	    if (ringmem == NULL)
		NOMEM("ring '%s' size %d - insufficient HAL memory for ring",
		      name,rbdesc->total_size);

	    rbdesc->ring_offset = SHMOFF(ringmem);
	    rhptr = ringmem;
	} else {
	    // allocate shared memory segment for ring and init
	    rbdesc->ring_shmkey = OS_KEY((RTAPI_RING_SHM_KEY + ring_id), rtapi_instance);

	    int shmid;

	    // allocate an RTAPI shm segment owned by HAL_LIB_xxx
	    if ((shmid = rtapi_shmem_new(rbdesc->ring_shmkey, lib_module_id,
					 rbdesc->total_size)) < 0)
		NOMEM("rtapi_shmem_new(0x%8.8x,%d) failed: %d",
		       rbdesc->ring_shmkey, lib_module_id,
		       rbdesc->total_size);

	    // map the segment now so we can fill in the ringheader details
	    if ((retval = rtapi_shmem_getptr(shmid,
					     (void **)&rhptr, 0)) < 0)
		NOMEM("rtapi_shmem_getptr for %d failed %d",
		       shmid, retval);
	}

	HALDBG("created ring '%s' in %s, total_size=%d",
	       name, (rbdesc->flags & ALLOC_HALMEM) ? "halmem" : "shm",
	       rbdesc->total_size);

	ringheader_init(rhptr, rbdesc->flags, size, sp_size);
	rhptr->refcount = 0; // on hal_ring_attach: increase; on hal_ring_detach: decrease
	rtapi_snprintf(rbdesc->name, sizeof(rbdesc->name), "%s", name);
	rbdesc->next_ptr = 0;

	// search list for 'name' and insert new structure
	prev = &(hal_data->ring_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		/* reached end of list, insert here */
		rbdesc->next_ptr = next;
		*prev = SHMOFF(rbdesc);
		return 0;
	    }
	    ptr = SHMPTR(next);
	    cmp = strcmp(ptr->name, rbdesc->name);
	    if (cmp > 0) {
		/* found the right place for it, insert here */
		rbdesc->next_ptr = next;
		*prev = SHMOFF(rbdesc);
		return 0;
	    }
	    /* didn't find it yet, look at next one */
	    prev = &(ptr->next_ptr);
	    next = *prev;
	}
	// automatic unlock by scope exit
    }
}

int hal_ring_deletef(const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_ring_deletefv(fmt, ap);
    va_end(ap);
    return ret;
}

int hal_ring_delete(const char *name)
{
    int retval;

    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_LOCK(HAL_LOCK_LOAD);

    {
	hal_ring_t *hrptr __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	// ring must exist
	if ((hrptr = halpr_find_ring_by_name(name)) == NULL) {
	    HALERR("ring '%s' not found", name);
	    return -ENOENT;
	}

	ringheader_t *rhptr;
	int shmid = -1;

	if (hrptr->flags & ALLOC_HALMEM) {
	    // ring exists as HAL memory.
	    rhptr = SHMPTR(hrptr->ring_offset);
	} else {
	    // ring exists as shm segment. Retrieve shared memory address.
	    if ((shmid = rtapi_shmem_new_inst(hrptr->ring_shmkey,
					      rtapi_instance, lib_module_id,
					      0 )) < 0) {
		if (shmid != -EEXIST)  {
		    HALERR("ring '%s': rtapi_shmem_new_inst() failed %d",
			   name, shmid);
		    return shmid;
		}
	    }
	    if ((retval = rtapi_shmem_getptr(shmid, (void **)&rhptr, 0))) {
		HALERR("ring '%s': rtapi_shmem_getptr %d failed %d",
		       name, shmid, retval);
		return -ENOMEM;
	    }
	}
	// assure attach/detach balance is zero:
	if (rhptr->refcount) {
	    HALERR("ring '%s' still attached - refcount=%d",
		   name, rhptr->refcount);
	    return -EBUSY;
	}

	HALDBG("deleting ring '%s'", name);
	if (hrptr->flags & ALLOC_HALMEM) {
	    ; // if there were a HAL memory free function, call it here
	} else {
	    if ((retval = rtapi_shmem_delete(shmid, lib_module_id)) < 0)  {
		HALERR("ring '%s': rtapi_shmem_delete(%d,%d) failed: %d",
		       name, shmid, lib_module_id, retval);
		return retval;
	    }
	}
	// search for the ring (again..)
	int *prev = &(hal_data->ring_list_ptr);
	int next = *prev;
	while (next != 0) {
	    hrptr = SHMPTR(next);
	    if (strcmp(hrptr->name, name) == 0) {
		// this is the right ring
		// unlink from list
		*prev = hrptr->next_ptr;
		// and delete it, linking it on the free list
		free_ring_struct(hrptr);
		return 0;
	    }
	    // no match, try the next one
	    prev = &(hrptr->next_ptr);
	    next = *prev;
	}

	HALERR("BUG: deleting ring '%s'; not found in ring_list?",
	       name);
	return -ENOENT;
    }

}

int hal_ring_attachf(ringbuffer_t *rb, unsigned *flags, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_ring_attachfv(rb, flags, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_ring_attach(const char *name, ringbuffer_t *rbptr,unsigned *flags)
{
    hal_ring_t *rbdesc;
    ringheader_t *rhptr;

    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);

    // no mutex(es) held up to here
    {
	int retval  __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	if ((rbdesc = halpr_find_ring_by_name(name)) == NULL) {
	    HALERR("no such ring '%s'", name);
	    return -ENOENT;
	}

	// calling hal_ring_attach(name, NULL, NULL) is a way to determine
	// if a given ring exists.
	// hal_ring_attach(name, NULL, &flags) is a way to inspect the flags
	// of an existing ring without actually attaching it.
	if (rbptr == NULL) {
	    if (flags)
		*flags = rbdesc->flags;
	    return 0;
	}

	if (rbdesc->flags & ALLOC_HALMEM) {
	    rhptr = SHMPTR(rbdesc->ring_offset);
	} else {
	    int shmid;

	    // map in the shm segment - size 0 means 'must exist'
	    if ((retval = rtapi_shmem_new_inst(rbdesc->ring_shmkey,
					       rtapi_instance, lib_module_id,
					   0 )) < 0) {
		if (retval != -EEXIST)  {
		    HALERR("ring '%s': rtapi_shmem_new_inst() failed %d",
			   name, retval);
		    return retval;
		}
		// tried to map shm again. May happen in halcmd_commands:print_ring_info().
		// harmless.
	    }
	    shmid = retval;

	    // make it accessible
	    if ((retval = rtapi_shmem_getptr(shmid, (void **)&rhptr, 0))) {
		HALERR("ring '%s': rtapi_shmem_getptr %d failed %d",
		       name, shmid, retval);
		return -ENOMEM;
	    }
	}
	// record usage in ringheader
	rhptr->refcount++;
	// fill in ringbuffer_t
	ringbuffer_init(rhptr, rbptr);

	if (flags)
	    *flags = rbdesc->flags;
	// hal mutex unlock happens automatically on scope exit
    }
    return 0;
}

int hal_ring_detachf(ringbuffer_t *rb, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_ring_detachfv(rb, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_ring_detach(const char *name, ringbuffer_t *rbptr)
{

    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_LOCK(HAL_LOCK_CONFIG);

    if ((rbptr == NULL) || (rbptr->magic != RINGBUFFER_MAGIC)) {
	HALERR("ring '%s': invalid ringbuffer", name);
	return -EINVAL;
    }

    // no mutex(es) held up to here
    {
	int retval __attribute__((cleanup(halpr_autorelease_mutex)));
	rtapi_mutex_get(&(hal_data->mutex));

	ringheader_t *rhptr = rbptr->header;
	rhptr->refcount--;
	rbptr->magic = 0;  // invalidate FIXME

	// unlocking happens automatically on scope exit
    }
    return 0;
}

//NB: not HAL lock
hal_ring_t *halpr_find_ring_by_name(const char *name)
{
    int next;
    hal_ring_t *ring;

    /* search ring list for 'name' */
    next = hal_data->ring_list_ptr;
    while (next != 0) {
	ring = SHMPTR(next);
	if (strcmp(ring->name, name) == 0) {
	    /* found a match */
	    return ring;
	}
	/* didn't find it yet, look at next one */
	next = ring->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

/***********************************************************************
*                    Internal HAL ring support functions               *
************************************************************************/

// we manage ring shm segments through a bitmap visible in hal_data
// instead of going through separate (and rather useless) RTAPI methods
// this removes both reliance on a visible RTAPI data segment, which the
// userland flavors dont have, and simplifies RTAPI.
// the shared memory key of a given ring id and instance is defined as
// OS_KEY(RTAPI_RING_SHM_KEY+id, instance).
static int next_ring_id(void)
{
    int i;
    for (i = 0; i < HAL_MAX_RINGS; i++) {
	if (!RTAPI_BIT_TEST(hal_data->rings,i)) {  // unused
	    RTAPI_BIT_SET(hal_data->rings,i);      // allocate
	    return i;
	}
    }
    return -EBUSY; // no more slots available
}

static hal_ring_t *alloc_ring_struct(void)
{
    hal_ring_t *p;

    /* check the free list */
    if (hal_data->ring_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->ring_free_ptr);
	/* unlink it from the free list */
	hal_data->ring_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_ring_t));
    }
    return p;
}

static void free_ring_struct(hal_ring_t * p)
{
    /* add it to free list */
    p->next_ptr = hal_data->ring_free_ptr;
    hal_data->ring_free_ptr = SHMOFF(p);
}

// varargs helpers
static int hal_ring_newfv(int size, int sp_size, int flags,
			  const char *fmt, va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
	HALERR("length %d too long for name starting '%s'",
	       sz, name);
        return -ENOMEM;
    }
    return hal_ring_new(name, size, sp_size, flags);
}


static int hal_ring_deletefv(const char *fmt, va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
	HALERR("length %d too long for name starting '%s'",
	       sz, name);
        return -ENOMEM;
    }
    return hal_ring_delete(name);
}

static int hal_ring_attachfv(ringbuffer_t *rb, unsigned *flags,
			     const char *fmt, va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
	HALERR("length %d too long for name starting '%s'",
	       sz, name);
        return -ENOMEM;
    }
    return hal_ring_attach(name, rb, flags);
}

static int hal_ring_detachfv(ringbuffer_t *rb, const char *fmt, va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
	HALERR("length %d too long for name starting '%s'",
	       sz, name);
        return -ENOMEM;
    }
    return hal_ring_detach(name, rb);
}

#ifdef RTAPI

EXPORT_SYMBOL(hal_ring_new);
EXPORT_SYMBOL(hal_ring_newf);
EXPORT_SYMBOL(hal_ring_delete);
EXPORT_SYMBOL(hal_ring_deletef);
EXPORT_SYMBOL(hal_ring_attach);
EXPORT_SYMBOL(hal_ring_attachf);
EXPORT_SYMBOL(hal_ring_detach);
EXPORT_SYMBOL(hal_ring_detachf);

#endif
