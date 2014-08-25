
typedef enum {
    IDLE,
    WAIT_FOR_COMMAND,     // from bus - cmd-out
    WAIT_FOR_TO_RT_SPACE, // currently blocked
    WRITING_TO_RT,
    WAIT_FOR_RT_MSG,      // from RTcomp - not decided yet whether it'll be
                          // a subcommand or reply (dual injector/responder comps)
    WAIT_FOR_RESPONSE,    // from bus - response-out (injector only)
    WAIT_FOR_RT_RESPONSE, // from RTcomp - command sent to RTcomp, waiting for RTcomp to reply (responder only)
} rtproxystate_t;

typedef struct {
    const char *name;

    unsigned flags;
    rtproxystate_t state;
    void *pipe;
    void *proxy_response;
    void *proxy_cmd;
    ringbuffer_t to_rt_ring;       // incoming ringbuffer
    msgbuffer_t to_rt_mframe;      // multiframe layer on incoming ringbuffer

    ringbuffer_t from_rt_ring;     // outgoing ringbuffer
    msgbuffer_t from_rt_mframe;    // multiframe layer on outgoing ringbuffer

    // for from-rt deserialisation
    void *buffer;
#define FROMRT_SIZE 4096          // fail miserably if larger

    const char *to_rt_name;
    const char *from_rt_name;
    bool decode_out, encode_in;  // pb_encode()/pb_decode() before/after RT I/O
    int min_delay, max_delay, current_delay; // poll delay with exponential backoff

    // stats:
    unsigned decode_fail;  // wanted DESERIALIZE_TO_RT but payload didnt decode
    unsigned mftx, ftx;    // frame + multiframe send counts
    unsigned mfrx, frx;    // frame + multiframe receive counts
    unsigned rb_txfail, rb_rxfail; // ringbuffer ops failures

} rtproxy_t;


void rtproxy_thread(void *arg, zctx_t *ctx, void *pipe);
