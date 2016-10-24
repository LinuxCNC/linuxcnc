
#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"
#include "hal_ring.h"		/* HAL ringbuffer decls */

static int next_ring_id(void);
static int free_ring_id(const int id);
static char *ringtypes[] = {
    "record",
    "multi",
    "stream",
    "any",
};
/***********************************************************************
*                     Public HAL ring functions                        *
************************************************************************/
hal_ring_t *halg_ring_newfv(const int use_hal_mutex,
			    const int size,
			    const int sp_size,
			    const int mode,
			    const char *fmt,
			    va_list ap)
{
    PCHECK_HALDATA();
    PCHECK_LOCK(HAL_LOCK_LOAD);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_ring_t *rptr = NULL;
	ringheader_t *rhptr;
	int retval, ring_id;
	char *name = NULL;
	char buf[HAL_MAX_NAME_LEN + 1];

	// allocate a new ring id - needed since we dont track ring shm
	// segments in RTAPI
	// do this early so we can make up a name if none given
	// NB: need to undo with free_ring_id() on failure
	if ((ring_id = next_ring_id()) < 0) {
	    return NULL; // _halerrno, errmsg set in next_ring_id
	}

	// if no name given, make one up now based on ring id and type:
	if (fmt == NULL) {
	    char *newfmt;
	    switch (mode & RINGTYPE_MASK) {
	    case RINGTYPE_RECORD:
		newfmt = "record-%d";
		break;
	    case RINGTYPE_MULTIPART:
		newfmt = "multi-%d";
		break;
	    case RINGTYPE_STREAM:
		newfmt = "stream-%d";
		break;
	    default:
		HALFAIL(EINVAL, "invalid ring type: 0x%x", mode & RINGTYPE_MASK);
		goto FAIL;
	    }
	    name =  fmt_args(buf, sizeof(buf), newfmt, ring_id);
	} else {
	    name = fmt_ap(buf, sizeof(buf), fmt, ap);
	    if (name == NULL) {
		goto FAIL;
	    }
	}
	HAL_ASSERT(name != NULL);

	// make sure no such ring name already exists
	rptr = halg_find_object_by_name(0, HAL_RING, name).ring;
	if (rptr != NULL) {
	    HALFAIL(EEXIST, "ring '%s' already exists", name);
	    goto FAIL;
	}

	// allocate ring descriptor
	if ((rptr = halg_create_objectf(0, sizeof(hal_ring_t),
					HAL_RING, 0, name)) == NULL) {
	    goto FAIL; // _halerrno set in halg_create_object
	}

	rptr->flags = mode;
	rptr->ring_id = ring_id;

	// make total allocation fit ringheader, ringbuffer and scratchpad
	rptr->total_size = ring_memsize( rptr->flags, size, sp_size);

	if (rptr->flags & ALLOC_HALMEM) {
	    void *ringmem = shmalloc_desc_aligned(rptr->total_size,
						  RTAPI_CACHELINE);
	    if (ringmem == NULL) {
		HALFAIL(ENOMEM, "ring '%s' size %d - insufficient HAL memory for ring",
		       name,rptr->total_size);
		goto FAIL;
	    }
	    rptr->ring_offset = SHMOFF(ringmem);
	    rhptr = ringmem;
	} else {
	    // allocate shared memory segment for ring and init
	    rptr->ring_shmkey = OS_KEY((RTAPI_RING_SHM_KEY + ring_id), rtapi_instance);

	    int shmid;

	    // allocate an RTAPI shm segment owned by HAL_LIB_xxx
	    if ((shmid = rtapi_shmem_new(rptr->ring_shmkey, lib_module_id,
					 rptr->total_size)) < 0) {
		HALFAIL(shmid, "rtapi_shmem_new(0x%8.8x,%d) failed: %d",
		       rptr->ring_shmkey, lib_module_id,
		       rptr->total_size);
		goto FAIL;
	    }
	    // map the segment now so we can fill in the ringheader details
	    if ((retval = rtapi_shmem_getptr(shmid,
					     (void **)&rhptr, 0)) < 0) {
		HALFAIL(retval, "rtapi_shmem_getptr for %d failed %d",
		       shmid, retval);
		goto FAIL;
	    }
	    HAL_ASSERT(is_aligned(rhptr, RTAPI_CACHELINE));

	}

	HALDBG("created ring '%s' in %s, total_size=%d",
	       name, (rptr->flags & ALLOC_HALMEM) ? "halmem" : "shm",
	       rptr->total_size);

	ringheader_init(rhptr, rptr->flags, size, sp_size);
	rhptr->refcount = 0; // on hal_ring_attach: increase; on hal_ring_detach: decrease

	// make it visible
	halg_add_object(false, (hal_object_ptr)rptr);
	return rptr;

    FAIL:
	free_ring_id(ring_id);
	if (rptr) {
	    hal_object_ptr o = {.any = rptr};
	    halg_free_object(0, o);
	}
	return NULL;
    } // automatic unlock by scope exit
}


int free_ring_struct(hal_ring_t *hrptr)
{
    ringheader_t *rhptr;
    int shmid = -1, ring_id;
    int retval;

    if (hrptr->flags & ALLOC_HALMEM) {
	// ring exists as HAL memory.
	rhptr = SHMPTR(hrptr->ring_offset);
    } else {
	// ring exists as shm segment. Retrieve shared memory address.
	if ((shmid = rtapi_shmem_new_inst(hrptr->ring_shmkey,
					  rtapi_instance, lib_module_id,
					  0 )) < 0) {
	    if (shmid != -EEXIST)  {
		HALFAIL_RC(shmid, "ring '%s': rtapi_shmem_new_inst() failed %d",
		       ho_name(hrptr), shmid);
	    }
	}
	if ((retval = rtapi_shmem_getptr(shmid, (void **)&rhptr, 0))) {
	    HALFAIL_RC(ENOMEM, "ring '%s': rtapi_shmem_getptr %d failed %d",
		   ho_name(hrptr), shmid, retval);
	}
    }
    // assure attach/detach balance is zero:
    if (rhptr->refcount) {
	HALFAIL_RC(EBUSY, "ring '%s' still attached - refcount=%d",
	       ho_name(hrptr), rhptr->refcount);
    }

    HALDBG("deleting ring '%s'", ho_name(hrptr));

    ring_id = hrptr->ring_id; // record for freeing ring id later
    if (hrptr->flags & ALLOC_HALMEM) {
	shmfree_desc(rhptr);
    } else {
	if ((retval = rtapi_shmem_delete(shmid, lib_module_id)) < 0)  {
	    HALFAIL_RC(retval, "ring '%s': rtapi_shmem_delete(%d,%d) failed: %d",
		   ho_name(hrptr), shmid, lib_module_id, retval);
	}
    }
    // free descriptor. May return -EBUSY if ring referenced.
    retval = halg_free_object(false, (hal_object_ptr)hrptr);
    if (retval == 0)
	retval = free_ring_id(ring_id);  // reuse
    return retval;
}

int halg_ring_deletefv(const int use_hal_mutex,
		       const char *fmt,
		       va_list ap)
{

    CHECK_HALDATA();
    CHECK_STR(fmt);
    char *name, buf[HAL_MAX_NAME_LEN + 1];
    name = fmt_ap(buf, sizeof(buf), fmt, ap);
    if (name == NULL)
	return _halerrno;

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	// ring must exist
	hal_ring_t *rptr = halg_find_object_by_name(0, HAL_RING, name).ring;
	if (rptr  == NULL) {
	    HALFAIL_RC(ENOENT, "ring '%s' not found", name);
	}
	free_ring_struct(rptr);
    }
    return 0;
}

int halg_ring_attachfv(const int use_hal_mutex,
		       ringbuffer_t *rbptr,
		       unsigned *flags,
		       const char *fmt,
		       va_list ap)
{
    CHECK_HALDATA();
    CHECK_STR(fmt);

    char *name, buf[HAL_MAX_NAME_LEN + 1];
    name = fmt_ap(buf, sizeof(buf), fmt, ap);
    if (name == NULL)
	return _halerrno;
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);
	int retval;
	ringheader_t *rhptr;

	hal_ring_t *rptr = halg_find_object_by_name(0, HAL_RING, name).ring;
	if (rptr == NULL) {
	    HALFAIL_RC(ENOENT, "no such ring '%s'", name);
	}

	// calling hal_ring_attach(name, NULL, NULL) is a way to determine
	// if a given ring exists.
	// hal_ring_attach(name, NULL, &flags) is a way to inspect the flags
	// of an existing ring without actually attaching it.
	if (rbptr == NULL) {
	    if (flags)
		*flags = rptr->flags;
	    return 0;
	}

	if (rptr->flags & ALLOC_HALMEM) {
	    rhptr = SHMPTR(rptr->ring_offset);
	} else {
	    int shmid;

	    // map in the shm segment - size 0 means 'must exist'
	    if ((retval = rtapi_shmem_new_inst(rptr->ring_shmkey,
					       rtapi_instance, lib_module_id,
					   0 )) < 0) {
		if (retval != -EEXIST)  {
		    HALFAIL_RC(retval, "ring '%s': rtapi_shmem_new_inst() failed %d",
			   name, retval);
		}
		// tried to map shm again. May happen in halcmd_commands:print_ring_info().
		// harmless.
	    }
	    shmid = retval;

	    // make it accessible
	    if ((retval = rtapi_shmem_getptr(shmid, (void **)&rhptr, 0))) {
		HALFAIL_RC(ENOMEM, "ring '%s': rtapi_shmem_getptr %d failed %d",
		       name, shmid, retval);
	    }
	}
	// record usage in ringheader
	rhptr->refcount++;
	// fill in ringbuffer_t
	ringbuffer_init(rhptr, rbptr);

	if (flags)
	    *flags = rptr->flags;
	// hal mutex unlock happens automatically on scope exit
    }
    return 0;
}


int halg_ring_detach(const int use_hal_mutex,
		     ringbuffer_t *rbptr)
{

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);

    if ((rbptr == NULL) || (rbptr->magic != RINGBUFFER_MAGIC)) {
	HALFAIL_RC(EINVAL, "invalid ringbuffer at %p", rbptr);
    }
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	ringheader_t *rhptr = rbptr->header;
	rhptr->refcount--;
	rbptr->magic = 0;
    }
    return 0;
}
/***********************************************************************
*                     Public HAL plug functions                        *
************************************************************************/

hal_plug_t *halg_plug_new(const int use_hal_mutex,
			  plug_args_t *args)

{
    PCHECK_HALDATA();
    PCHECK_NULL(args);
    PCHECK_LOCK(HAL_LOCK_LOAD);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	// make sure the owner exists, and obtain descriptor
	hal_object_ptr owner;
	if (args->owner_name)
	    // NB: finds any object type
	    owner = halg_find_object_by_name(0, 0, args->owner_name);
	else
	    owner = halg_find_object_by_id(0, 0, args->owner_id);
	if (owner.any == NULL) {
	    if (args->owner_name)
		HALFAIL_NULL(EINVAL,
			     "object '%s' does not exist",
			     args->owner_name);
	    else
		HALFAIL_NULL(EINVAL,
			     "object with id=%d does not exist",
			     args->owner_id);
	}

	// make sure the ring exists, and obtain descriptor
	hal_ring_t *ring;
	if (args->ring_name)
	    ring = halg_find_object_by_name(0, HAL_RING, args->ring_name).ring;
	else
	    ring = halg_find_object_by_id(0, HAL_RING, args->ring_id).ring;

	// construct plug name as '<ring>.<plug owner>.[read|write]'
	char *tag = (args->type == PLUG_WRITER) ? "write" : "read";
	char buf[HAL_MAX_NAME_LEN];
	char *plugname = fmt_args(buf, sizeof(buf), "%s.%s.%s",
				  hh_get_name(&ring->hdr),
				  hh_get_name(owner.hdr),
				  tag);
	if (plugname == NULL) {
	    HALFAIL(EINVAL, "name too long");
	    goto FAIL;
	}

	// make sure no such plug name already exists
	hal_plug_t *e = halg_find_object_by_name(0, HAL_PLUG,
						 plugname).plug;
	if (e != NULL) {
	    HALFAIL(EEXIST, "plug '%s' already exists", plugname);
	    goto FAIL;
	}

	// at this point the ring descriptor must be valid
	if (ring == NULL)
		goto FAIL;

	// check ring type compatibility
	unsigned wanted = args->flags & RINGTYPE_MASK;
	unsigned ring_is = ring->flags & RINGTYPE_MASK;
	if ((wanted != RINGTYPE_ANY) &&
	    (wanted != ring_is)) {
	    HALFAIL(ENOENT, "ring types incompatible: plug wants '%s', ring is '%s'",
		   ringtypes[wanted], ringtypes[ring_is]);
	    goto FAIL;
	}

	hal_plug_t *plug = NULL;
	// allocate plug descriptor
	if ((plug = halg_create_objectf(0,
					sizeof(hal_plug_t),
					HAL_PLUG,
					hh_get_id(owner.hdr),
					plugname)) == NULL) {
	    goto FAIL;
	}

	// we have the object.
	plug->ring_id = ho_id(ring);
	plug->flags = args->flags;
	plug->role = args->type;

	// Attach the ring.
	unsigned flags = 0;
	int retval = halg_ring_attachf(0, &plug->rb, &flags, ho_name(ring));
	if (retval) {
	    // TBD: undo damage?
	    // check flags again? ah.
	    goto FAIL;
	}
	// mark usage in ringheder
	if (args->type == PLUG_WRITER)
	    plug->rb.header->writer = ho_id(plug);
	else
	    plug->rb.header->reader = ho_id(plug);

	// If multi, init the multiframe accessor struct
	if (plug->rb.header->type == RINGTYPE_MULTIPART) {
	    msgbuffer_init(&plug->mb, &plug->rb);
	}

	// off to the races:
	halg_add_object(false, (hal_object_ptr)plug);

	HALDBG("created plug '%s' type %s ",
	       hh_get_name(&plug->hdr), ringtypes[plug->rb.header->type]);

	return plug;

    FAIL:
	if (plug)
	    halg_free_object(0, (hal_object_ptr) plug);
	return NULL;
    }
}

int halg_plug_delete(const int use_hal_mutex,
		     hal_plug_t *plug)

{
    CHECK_HALDATA();
    CHECK_NULL(plug);
    CHECK_LOCK(HAL_LOCK_LOAD);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	// make sure it's a valid HAL object:
	if (!(hh_is_valid(&plug->hdr))) {
	    HALFAIL_RC(EINVAL, "object at %p not valid", plug);
	}
	// and it actually is a plug:
	if (hh_get_object_type(&plug->hdr) != HAL_PLUG) {
	    HALFAIL_RC(EINVAL, "object at %p not a plug but a %s", plug,
		   hal_object_typestr(hh_get_object_type(&plug->hdr)));
	}
	// do not fail if called after a half-done init:
	if (plug->rb.header != NULL) {
	    // looks valid.
	    // unmark ourselves in ringheder
	    int self = hh_get_id(&plug->hdr);

	    if (plug->rb.header->writer == self)
		plug->rb.header->writer = 0;
	    if (plug->rb.header->reader == self)
		plug->rb.header->reader = 0;

	    // and detach from the ring.
	    halg_ring_detach(0, &plug->rb);
	}

	HALDBG("deleting plug '%s'", hh_get_name(&plug->hdr));

	// delete the plug object. Invalidates *plug.
	return halg_free_object(false, (hal_object_ptr)plug);
    }
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
    HALFAIL_RC(EINVAL, "out of ring id's, HAL_MAX_RINGS = %d",HAL_MAX_RINGS);
}

static int free_ring_id(const int id)
{
    if ((id < 0) || (id >HAL_MAX_RINGS)) {
	HALFAIL_RC(EINVAL, "invalid ring id: %d", id);
    }
    if (!RTAPI_BIT_TEST(hal_data->rings,id)) {
	HALFAIL_RC(EINVAL, "unused ring id: %d", id);
    }
    RTAPI_BIT_CLEAR(hal_data->rings, id);
    return 0;
}


#ifdef RTAPI

EXPORT_SYMBOL(halg_ring_newfv);
EXPORT_SYMBOL(halg_ring_deletefv);
EXPORT_SYMBOL(halg_ring_attachfv);
EXPORT_SYMBOL(halg_ring_detach);
EXPORT_SYMBOL(halg_plug_new);

#endif
