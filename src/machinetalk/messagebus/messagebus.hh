// actor types

// unidirectional
#define ACTOR_PUBLISHER      1  // just emits messages, no receive
#define ACTOR_SUBSCRIBER     2  // just consumes messages, no transmit

// bidirectional
#define ACTOR_INJECTOR       4  // send, then receive
#define ACTOR_RESPONDER      8  // receive, then send

// rtproxy functions
#define ACTOR_ECHO          16  // a dummy responder which just replies with the request

// proxy behavior
#define TRACE_TO_RT         32  // log messages as sent to RT
#define TRACE_FROM_RT       64  // log messages as received from RT

// treating messages before passed to RT:
#define DESERIALIZE_TO_RT  128  // send as nanopb struct if decoded as such
#define SERIALIZE_FROM_RT  256  // encode from nanopb struct

extern int comp_id;
extern const char *progname;
