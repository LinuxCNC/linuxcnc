/* zeroconf announce/withdraw tailored for machinekit purposes
 */

#ifndef _MK_SERVICE_HH
#define _MK_SERVICE_HH

#include <uuid/uuid.h>
#include <czmq.h>

#include "czmq-watch.h"
#include "ll-zeroconf.hh"
#include "mk-zeroconf-types.h"

// per-daemon:
typedef struct {
    // these must be set by caller:
    const char    *rundir;          // for IPC sockets
    int            rtapi_instance;  // defaults to 0
    zctx_t        *z_context;
    zloop_t       *z_loop;
    AvahiCzmqPoll *av_loop;         // Avahi CZMQ event loop adapter

    // caller should zero before first use, the file remains open
    // for later use (eg retrieving specific service port numbers)
    FILE     *mkinifp;

    // filled in by mk_getnetopts()
    char     *service_uuid;     // service instance (set of running server processes)
    char     *process_uuid;     // server instance (this process)
    uuid_t   svc_uuid;          // binary forms of the above
    uuid_t   proc_uuid;
    const char *hostname;       // without domain suffix

    // filled in by mk_getnetopts() by inspecting $MACHINEKIT_INI:
    int remote;                 // false - use IPC; true - use TCP

    // list of zmq interface/address strings, see zmq_tcp(7)
    const char *bind_ipv4;
    const char *bind_ipv6;

    // handling of mDNS announcements:
    int announce_ipv4;
    int announce_ipv6;
} mk_netopts_t;

// per-czmq-socket of a daemon:
typedef struct {
    int   port;                // otpionally may be set by caller

    void *socket;              // must be set by caller
    const char *dnssd_subtype; // must be set by caller
    const char *tag;           // must be set by caller

    const char *dnssd_type;    // may be set by caller, else defaults to
                               // MACHINEKIT_DNSSD_SERVICE_TYPE
    char *announced_uri;       // set in mk_bindsocket()
    register_context_t *publisher; // set in mk_announce()
} mk_socket_t;

// fill in mk_netopts from $MACHINEKIT_INI
int mk_getnetopts(mk_netopts_t *n);

int mk_bindsocket(mk_netopts_t *n, mk_socket_t *s);

int mk_announce(mk_netopts_t *n, mk_socket_t *s, const char *headline, const char *path);

int mk_withdraw(const mk_socket_t *s);


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif
