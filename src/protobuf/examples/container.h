#ifndef  _CONTAINER_H
#define _CONTAINER_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef PB_DEBUG
#include <assert.h>
#endif

#include <libnml/nml/nml_type.hh> // NMLTYPE

// need to get at enum MsgType and some tags
#if defined(GOOGLE_PROTOBUF_VERSION)
#include <protobuf/generated/types.pb.h>

#define MSGTYPE          MsgType
#define EMCMOT_LOWER     MT_EMCMOT_LOWER
#define EMCMOT_UPPER     MT_EMCMOT_UPPER
#define EMC_NML_LOWER    MT_EMC_NML_LOWER
#define EMC_NML_UPPER    MT_EMC_NML_UPPER
#define CONTAINER        Container

#elif defined(PROTOBUF_C_MAJOR)
#include <protobuf/generated/types.pb-c.h>

#define MSGTYPE          _MsgType
#define EMCMOT_LOWER     MSG_TYPE__MT_EMCMOT_LOWER
#define EMCMOT_UPPER     MSG_TYPE__MT_EMCMOT_UPPER
#define EMC_NML_LOWER    MSG_TYPE__MT_EMC_NML_LOWER
#define EMC_NML_UPPER    MSG_TYPE__MT_EMC_NML_UPPER
#define CONTAINER        _Container

#elif defined(NANOPB_VERSION)
#include <protobuf/generated/types.npb.h>
#include <protobuf/generated/message.npb.h>
#define MSGTYPE          enum _MsgType
#define EMCMOT_LOWER     MsgType_MT_EMCMOT_LOWER
#define EMCMOT_UPPER     MsgType_MT_EMCMOT_UPPER
#define EMC_NML_LOWER    MsgType_MT_EMC_NML_LOWER
#define EMC_NML_UPPER    MsgType_MT_EMC_NML_UPPER
#define CONTAINER        struct _Container
#else
#error "include container.h after protobuf-specific headers"
#endif

static inline bool is_Motion_container(MSGTYPE msgtype)
{
    return ((msgtype > EMCMOT_LOWER) &&
	    (msgtype < EMCMOT_UPPER));
}

static inline bool is_NML_container(MSGTYPE msgtype)
{
    return ((msgtype > EMC_NML_LOWER) &&
	    (msgtype < EMC_NML_UPPER));
}

static inline int NML_to_Container(NMLTYPE nmltype)
{
    return (nmltype + EMC_NML_LOWER);
}

static inline int Motion_to_Container(NMLTYPE nmltype)
{
    return (nmltype + EMCMOT_LOWER);
}

// enums are encoded as varints. To determine the total message length
// to go into Container.length, we need to determine the length of
// the type tag.
static inline size_t varint_size(uint64_t value)
{
    size_t i = 0;

    if (value == 0) return 1;
    while (value)  {
        value >>= 7;
        i++;
    }
    return i;
}

#define CONTAINER_LENGTH(payload, type) \
    (payload + varint_size(type) + 5) // wire size of fixed32 = 5
static inline void finish_container_header(CONTAINER *container,
					   size_t    payload_size,
					   MSGTYPE   type)
{
#if defined(PB_DEBUG) && defined(GOOGLE_PROTOBUF_VERSION)
    assert(MsgType_IsValid(type));
#endif
#if defined(NANOPB_VERSION) || defined(PROTOBUF_C_MAJOR)
    container->type = type;
    container->length = CONTAINER_LENGTH(payload_size, type);
#endif
#if defined(GOOGLE_PROTOBUF_VERSION)
    container->set_type(type);
    container->set_length(CONTAINER_LENGTH(payload_size, MT_COMMAND));
#endif
}


#undef MSGTYPE
#undef EMCMOT_LOWER
#undef EMCMOT_UPPER
#undef EMC_NML_LOWER
#undef EMC_NML_UPPER

#ifdef __cplusplus
}
#endif
#endif // _CONTAINER_H
