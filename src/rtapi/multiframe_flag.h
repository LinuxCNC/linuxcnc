#ifndef _MULTIFRAME_FLAG_H
#define _MULTIFRAME_FLAG_H


// possible wireformat encodings of a machinetalk message
typedef enum {
    MF_NPB_CSTRUCT    =  0,    // payload is in nanopb C struct format
    MF_PROTOBUF       =  1,    // payload is in protobuf wire format
    MF_BLOB           =  2,    // payload is an opaque blob
    MF_STRING         =  3,    // payload is a printable string
    MF_LEGACY_MOTCMD  =  4,    // motion command C structs, with cmd_code_t type tag
    MF_LEGACY_MOTSTAT =  5,    // motion status C structs, with cmd_status_t type tag
    MF_JSON           =  6,    // payload is a JSON object (string)
    MF_GPB_TEXTFORMAT =  7,    // payload is google::protobuf::TextFormat (string)
    MF_XML            =  8,    // payload is XML format (string)

    MF_ROSv1          =  9,
    MF_ROSv2          =  10,

    // add here as needed and change the #define to point to the last encoding
    // used in the base code
#define MF_LAST  MF_ROSv2

    MF_RESERVED1      =  11,
    MF_RESERVED2      =  12,
    MF_USER1          =  13,
    MF_USER2          =  14,
    MF_USER3          =  15,
} mt_encoding_t;

// mt_encoding_set_t - indicates which type(s) of encoding an entity is
// willing to accept
//
// used to identify plug capabilities and drive automatic transcoding
// announced/set by readers, inspected by writer
//
//            a bitmap of mf_encoding_t flags
typedef enum {
    	RE_NPB_CSTRUCT  =  RTAPI_BIT(MF_NPB_CSTRUCT), // supports nanopb C struct format
	RE_PROTOBUF     =  RTAPI_BIT(MF_PROTOBUF),    // payload is in protobuf wire format
        RE_BLOB         =  RTAPI_BIT(MF_BLOB),        // supports blob
	RE_STRING       =  RTAPI_BIT(MF_STRING),      // etc etc
	RE_LEGACY_MOTCMD   =  RTAPI_BIT(MF_LEGACY_MOTCMD),
        RE_LEGACY_MOTSTAT  =  RTAPI_BIT(MF_LEGACY_MOTSTAT),
	RE_JSON         =  RTAPI_BIT(MF_JSON),
	RE_GPB_TEXTFORMAT  =  RTAPI_BIT(MF_GPB_TEXTFORMAT),
	RE_XML          =  RTAPI_BIT(MF_XML),
	RE_ROSv1        =  RTAPI_BIT(MF_ROSv1),
	RE_ROSv2        =  RTAPI_BIT(MF_ROSv2),

	// RE_RESERVED1,2   intentionally undefined

	RE_USER1          =  RTAPI_BIT(MF_USER1),
	RE_USER2          =  RTAPI_BIT(MF_USER2),
	RE_USER3          =  RTAPI_BIT(MF_USER3),
} mt_encoding_set_t;
#define RE_MAX 16

// disposition of the __u32 flags value
typedef struct {
    __u32 msgid     : 12; // must hold all proto msgid values (!)
    __u32 format    : 4;  // an mt_encoding_t - how to interpret the message
    __u32 more      : 1;  // zeroMQ marker for multiframe messages
    __u32 eor       : 1;  // zeroMQ marker end-of-route, next frame is payload
                          // zeroMQ route tags are marked by msgid == MSGID_HOP
    __u32 unused    : 14; // spare
} mfields_t;


typedef union {
    __u32     u;
    mfields_t f;
} mflag_t;


#endif // _MULTIFRAME_FLAG_H
