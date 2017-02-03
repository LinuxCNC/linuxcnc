#ifndef _PBMSGS_H
#define _PBMSGS_H

#include <machinetalk/include/pb-linuxcnc.h>
#include <machinetalk/nanopb/pb_decode.h>
#include <machinetalk/nanopb/pb_encode.h>

// the message type descriptor
typedef struct {
    uint32_t msgid;
    int32_t encoded_size;
    const char *name;
    size_t size;
    const pb_field_t *fields;
    void *user;
} msginfo_t;

#define PB_MSG(id, size, name) {				\
	id,							\
	    size,						\
	    # name,						\
	    sizeof(PB_MSG_ ## id),				\
	    name ## _fields,					\
	    NULL,						\
	    },

#define PB_MSGINFO_DELIMITER {0, -1, NULL, 0, NULL, NULL}
#define PB_MSGID

#include <machinetalk/protobuf/message.npb.h>
#include <machinetalk/protobuf/value.npb.h>
#include <machinetalk/protobuf/test.npb.h>
#include <machinetalk/protobuf/jplan.npb.h>
#include <machinetalk/protobuf/ros.npb.h>
 

#endif // _PBMSGS_H
