
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_ring.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("test component for HAL plugs");
MODULE_LICENSE("GPL");
RTAPI_TAG(HAL, HC_INSTANTIABLE);

static int comp_id;
static char *compname = "plug";

struct inst_data {
    hal_plug_t  *rplug, *wplug;
};

static char *wring = NULL;
RTAPI_IP_STRING(wring, "a ring this instance should plug to as a writer");

static char *rring = NULL;
RTAPI_IP_STRING(rring, "a ring this instance should plug to as a reader");

static int funct(void *arg, const hal_funct_args_t *fa)
{
    // use the instance pointer passed in halinst_export_funct()
    struct inst_data *ip = arg;

    return 0; // extended thread functs return a value
}

// init HAL objects
static int export_halobjs(struct inst_data *ip, int owner_id, const char *name)
{
    if (rring) {
	// if given, the ring MUST be a stream type
	plug_args_t rargs = {
	    .type = PLUG_READER,
	    .flags = RINGTYPE_STREAM,
	    .ring_name = rring,
	    .owner_name = name
	};
	ip->rplug = halg_plug_new(1, &rargs);
	if (ip->rplug == NULL)
	    return _halerrno;
    }
    if (wring) {
	// this plug accepts any ring type
	plug_args_t wargs = {
	    .type = PLUG_WRITER,
	    .flags = RINGTYPE_ANY,
	    .ring_name = wring,
	    .owner_name = name
	};
	ip->wplug = halg_plug_new(1, &wargs);
	if (ip->wplug == NULL)
	    return _halerrno;
    }

    // exporting '<instname>.funct' as an extended thread function
    // see lutn.c for a discussion of advantages
    hal_export_xfunct_args_t xfunct_args = {
        .type = FS_XTHREADFUNC,
        .funct.x = funct,
        .arg = ip,  // the instance's HAL memory blob
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = owner_id
    };
    return hal_export_xfunctf(&xfunct_args, "%s.funct", name);
}

// constructor - init all HAL pins, params, funct etc here
static int instantiate(const char *name, const int argc, const char**argv)
{
    struct inst_data *ip;

    // allocate a named instance, and some HAL memory for the instance data
    int inst_id = hal_inst_create(name, comp_id,
				  sizeof(struct inst_data),
				  (void **)&ip);
    if (inst_id < 0)
	return -1;

    // here ip is guaranteed to point to a blob of HAL memory
    // of size sizeof(struct inst_data).
    HALDBG("inst=%s argc=%d\n", name, argc);
    HALDBG("instance parms: rring=%s wring=%s", rring, wring);

    // these pins/params/functs will be owned by the instance,
    // and can be separately exited 'halcmd delinst <instancename>'
    int retval = export_halobjs(ip, inst_id, name);
    return retval;
}

// custom destructor
// pins, params, and functs are automatically deallocated by hal_exit(comp_id)
// regardless if a destructor is used or not (see below), so usually it is not
// needed
//
// however, some objects like vtables, rings, threads, signals are not owned by
// a component, hence cannot be automatically exited by hal_exit() even if desired
//
// interaction with such objects may require a custom destructor like below for
// cleanup actions

// NB: if a customer destructor is used, it is called
// - after the instance's functs have been removed from their respective threads
//   (so a thread funct call cannot interact with the destructor any more)
// - any pins,  params and plugs of this instance are still intact when the destructor is
//   called, and they are automatically destroyed by the HAL library once the
//   destructor returns
static int delete(const char *name, void *inst, const int inst_size)
{

    HALDBG("inst=%s size=%d %p\n", name, inst_size, inst);
    return 0;
}

int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate, delete, compname);
    if (comp_id < 0)
	return comp_id;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id); // calls delete() on all insts
}
