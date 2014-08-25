#ifndef _MULTIFRAME_FLAG_H
#define _MULTIFRAME_FLAG_H

typedef enum {
    MF_TRANSPARENT    = 0,    // rtproxy passed through unmodified
    MF_PROTOBUF       = 1,    // payload in protobuf wire format
    MF_NPB_CSTRUCT    = 2,    // payload is in nanopb C struct format
} mframetype_t;


// if MF_PROTOBUF or MF_NPB_CSTRUCT,
// pbmsgtype denotes the protobuf message type contained
typedef enum {
    NPB_UNSPECIFIED   = 0,
    NPB_CONTAINER     = 1,
    NPB_RT_CONTAINER  = 2,
} npbtype_t;

// disposition of the __u32 flags value
typedef struct {
    __u32 frametype : 8;
    __u32 npbtype   : 8;
    __u32 __unused  : 16;
} mfields_t;


typedef union {
    __u32     u;
    mfields_t f;
} mflag_t;


#endif // _MULTIFRAME_FLAG_H
