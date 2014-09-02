// initialize libwebsockets_protocols structs

#include "webtalk.hh"


struct libwebsocket_protocols *protocols;

#ifdef EXPERIMENTAL_ZWS_SUPPORT
#define NPROTOS 6
#else
#define NPROTOS 3
#endif

void init_protocols(void)
{
    // size must be number of protos + 1 - libwebsockets requires a zero delimiter struct
    struct libwebsocket_protocols *p = protocols = 
	(struct libwebsocket_protocols *) calloc(NPROTOS,
						 sizeof (struct libwebsocket_protocols));
    assert(p != 0);

    //  first protocol must always be HTTP handler
    p->name = "http";
    p->callback = callback_http;
    p->per_session_data_size = sizeof(zws_session_t);
    p->id = PROTO_FRAMING_NONE|PROTO_ENCODING_NONE;
    p++;

    // machinekit specific framing
    p->name = "machinekit1.0";
    p->callback = callback_http;
    p->per_session_data_size = sizeof(zws_session_t);
    p->id = PROTO_FRAMING_MACHINEKIT|PROTO_ENCODING_JSON|PROTO_VERSION(0);
    p++;


#ifdef EXPERIMENTAL_ZWS_SUPPORT
    // experimentally enable "ZWS1.0" as legit ws protocol
    // https://github.com/somdoron/rfc/blob/master/spec_39.txt
    p->name = "ZWS1.0";
    p->callback = callback_http;
    p->per_session_data_size = sizeof(zws_session_t);
    p->id = PROTO_FRAMING_ZWS|PROTO_ENCODING_PROTOBUF|PROTO_VERSION(0);
    p++;

    // mhaberler's protobuf-based framing method
    // assumes https://github.com/dcodeIO/ProtoBuf.js or manual decoding
    // client-side
    p->name = "ZWS1.0-proto";
    p->callback = callback_http;
    p->per_session_data_size = sizeof(zws_session_t);
    p->id = PROTO_FRAMING_ZWSPROTO|PROTO_ENCODING_PROTOBUF|PROTO_VERSION(0);
    p++;


    // wrapped in base64 - binary-safe version of above for text-only ws transports
    p->name = "ZWS1.0-protob-ase64";
    p->callback = callback_http;
    p->per_session_data_size = sizeof(zws_session_t);
    p->id = PROTO_WRAP_BASE64|PROTO_FRAMING_ZWSPROTO|PROTO_ENCODING_PROTOBUF|PROTO_VERSION(0);
    p++;
#endif

};
