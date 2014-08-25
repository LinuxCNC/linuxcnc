#ifndef _PB_LINUXCNC_H
#define _PB_LINUXCNC_H

// Nanopb global library options

// #define PB_FIELD_32BIT
// #define PB_BUFFER_ONLY   // custom streams not needed - memory buffers ok

#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)
#define VERSION EXPAND_AND_QUOTE(NANOPB_VERSION)

// take care when compiling as a kernel module:
#ifndef __KERNEL__
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#else
#include <linux/module.h>
#endif

#define _NO_PB_SYSTEM_HEADER
#include <machinetalk/nanopb/pb.h>

// linuxcnc extensions to nanopb

// callback to execute when this field is done decoding:
typedef struct pb_dest pb_dest_t;

typedef  bool (* process_cb_t)(pb_dest_t *);

// callback dispatch descriptors
struct pb_dest {
    void  *dest;
    size_t size;
    // encode: fetch field value
    // decode: field decode complete
    process_cb_t callback;
    void  *userdata;
};

/* extern bool pb_encode_dest(pb_ostream_t *stream, const pb_field_t *field, void * const *arg); */
/* extern bool pb_decode_dest(pb_istream_t *stream, const pb_field_t *field, void *arg); */


// use like so:
#if 0
bool process_legacy_motcmd(pb_dest_t *d)
{
    emcmot_command_t  *mc = d->dest;
    const char *u = d->userdata;
    ....
}
static pb_dest_t cmd_dest     = {
    &localCommand,
    sizeof(emcmot_command_t),
    process_legacy_motcmd,
    "opaque user data"
};

static pb_dest_t status_dest  = {
    &localStatus,
    sizeof(emcmot_status_t),
    NULL,
    NULL
};

// see nanopb.c for pb_encode_dest() / pb_decode_dest():
container = (Container) {
    .legacy_motcmd.funcs.decode = pb_decode_dest,
    .legacy_motcmd.arg = &cmd_dest,
};
pb_decode(&cstream, Container_fields, &container);
pb_encode(&cstream, Container_fields, &container);
#endif

#endif
