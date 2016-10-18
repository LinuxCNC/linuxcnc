#ifndef _MSGINFO_H
#define _MSGINFO_H

#include <pb-machinekit.h>
#include <pb_decode.h>
#include <pb_encode.h>

#ifdef __cplusplus
#include <google/protobuf/message.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // the message type descriptor
    typedef struct {
	uint32_t msgid;
	int32_t encoded_size;  // -1 if variable size
	const char *name;
	size_t size;

	// the nanopb descriptor
	const pb_field_t *fields;

	// need to fudge this for RT - we cant include C++ headers in RT
	// in the RT case, just view those as void *
#ifdef __cplusplus
	const google::protobuf::Descriptor *descriptor;
#else
	void *descriptor;
#endif
	// user extensibility
	void     *user_ptr;
	unsigned user_flags;

    } pbmsginfo_t;

    // given a msgid, retrieve the pbmsginfo_t descriptor
    const pbmsginfo_t *pbmsgdesc_by_id(const unsigned msgid);

    // given a message name, retrieve the pbmsginfo_t descriptor
    const pbmsginfo_t *pbmsgdesc_by_name(const char *name);

    // compare function for qsort & bsearch
    int msginfo_cmp(const void *p1, const void *p2);

    extern unsigned pbmsginfo_count;

#ifdef __cplusplus
}
#endif
#endif // _MSGINFO_H
