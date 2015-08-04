#include "config.h"
#include "halcmd_rtapiapp.h"

#include <czmq.h>
#include <string.h>
#include "ll-zeroconf.hh"
#include "mk-zeroconf.hh"
#include "mk-zeroconf-types.h"
#include "pbutil.hh"
#include <avahi-common/malloc.h>


#include <machinetalk/generated/message.pb.h>
#include <google/protobuf/text_format.h>

using namespace google::protobuf;

static pb::Container command, reply;

static zctx_t *z_context;
static void *z_command;
static int timeout = 5000;
static std::string errormsg;
int proto_debug;

int rtapi_rpc(void *socket, pb::Container &tx, pb::Container &rx)
{
    zframe_t *request = zframe_new (NULL, tx.ByteSize());
    assert(request);
    assert(tx.SerializeWithCachedSizesToArray(zframe_data (request)));
    if (proto_debug) {
	string buffer;
	if (TextFormat::PrintToString(tx, &buffer)) {
	    fprintf(stderr, "%s:%d:%s: request ---->\n%s%s\n",
		    __FILE__,__LINE__,__FUNCTION__,
		    buffer.c_str(),
		    std::string(20,'=').c_str());
	}
    }
    assert (zframe_send (&request, socket, 0) == 0);
    zframe_t *reply = zframe_recv (socket);
    if (reply == NULL) {
	errormsg =  "rtapi_rpc(): reply timeout";
	return -1;
    }
    int retval =  rx.ParseFromArray(zframe_data (reply),
				    zframe_size (reply)) ? 0 : -1;
   if (proto_debug) {
	string buffer;
	if (TextFormat::PrintToString(rx, &buffer)) {
	    fprintf(stderr, "%s:%d:%s: <---- reply\n%s%s\n",
		    __FILE__,__LINE__,__FUNCTION__,
		    buffer.c_str(),
		    std::string(20,'=').c_str());
	}
    }
    zframe_destroy(&reply);
    if (rx.note_size())
	errormsg = pbconcat(rx.note(), "\n");
    else
	errormsg = "";
    return retval;
}


int rtapi_callfunc(int instance,
		   const char *func,
		   const char **args)
{
    pb::RTAPICommand *cmd;
    command.Clear();
    command.set_type(pb::MT_RTAPI_APP_CALLFUNC);
    cmd = command.mutable_rtapicmd();
    cmd->set_func(func);
    cmd->set_instance(instance);

    int argc = 0;
    if (args)
	while(args[argc] && *args[argc]) {
	    cmd->add_argv(args[argc]);
	    argc++;
	}
    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();
}

int rtapi_newinst(int instance,
		  const char *comp,
		  const char *instname,
		  const char **args)
{
    pb::RTAPICommand *cmd;
    command.Clear();
    command.set_type(pb::MT_RTAPI_APP_NEWINST);
    cmd = command.mutable_rtapicmd();
    cmd->set_instance(instance);

    cmd->set_comp(comp);
    cmd->set_instname(instname);

    int argc = 0;
    if (args)
	while(args[argc] && *args[argc]) {
	    cmd->add_argv(args[argc]);
	    argc++;
	}
    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();
}

int rtapi_delinst(int instance,
		  const char *instname)
{
    pb::RTAPICommand *cmd;
    command.Clear();
    command.set_type(pb::MT_RTAPI_APP_DELINST);
    cmd = command.mutable_rtapicmd();
    cmd->set_instance(instance);
    cmd->set_instname(instname);
    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();

}

static int rtapi_loadop(pb::ContainerType type, int instance, const char *modname, const char **args)
{
    pb::RTAPICommand *cmd;
    command.Clear();
    command.set_type(type);
    cmd = command.mutable_rtapicmd();
    cmd->set_modname(modname);
    cmd->set_instance(instance);

    int argc = 0;
    if (args)
	while(args[argc] && *args[argc]) {
	    cmd->add_argv(args[argc]);
	    argc++;
	}
    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();
}

int rtapi_loadrt(int instance, const char *modname, const char **args)
{
    return rtapi_loadop(pb::MT_RTAPI_APP_LOADRT, instance, modname, args);
}

int rtapi_unloadrt(int instance, const char *modname)
{
    return rtapi_loadop(pb::MT_RTAPI_APP_UNLOADRT, instance, modname, NULL);
}

int rtapi_shutdown(int instance)
{
    pb::RTAPICommand *cmd;

    command.Clear();
    command.set_type(pb::MT_RTAPI_APP_EXIT);
    cmd = command.mutable_rtapicmd();
    cmd->set_instance(instance);

    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();
}


int rtapi_ping(int instance)
{
    pb::RTAPICommand *cmd;
    command.Clear();
    command.set_type(pb::MT_RTAPI_APP_PING);
    cmd = command.mutable_rtapicmd();
    cmd->set_instance(instance);

    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();
}

int rtapi_newthread(int instance, const char *name, int period, int cpu, int use_fp, int flags)
{
    pb::RTAPICommand *cmd;
    command.Clear();
    command.set_type(pb::MT_RTAPI_APP_NEWTHREAD);
    cmd = command.mutable_rtapicmd();
    cmd->set_instance(instance);
    cmd->set_threadname(name);
    cmd->set_threadperiod(period);
    cmd->set_cpu(cpu);
    cmd->set_use_fp(use_fp);
    cmd->set_flags(flags);

    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();
}

int rtapi_delthread(int instance, const char *name)
{
    pb::RTAPICommand *cmd;
    command.Clear();
    command.set_type(pb::MT_RTAPI_APP_DELTHREAD);
    cmd = command.mutable_rtapicmd();
    cmd->set_instance(instance);
    cmd->set_threadname(name);
    int retval = rtapi_rpc(z_command, command, reply);
    if (retval)
	return retval;
    return reply.retcode();
}

const char *rtapi_rpcerror(void)
{
    return errormsg.c_str();
}

int rtapi_connect(int instance, char *uri, const char *svc_uuid)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    char ipcuri[100];

    if (uri == NULL) {
	snprintf(ipcuri, sizeof(ipcuri),ZMQIPC_FORMAT,
		 RUNDIR, instance, "rtapi", svc_uuid);
	uri = ipcuri;
    }

#if 0
    // we're not doing remote RTAPI just yet
    if (uri == NULL) {
	char uuid[50];
	snprintf(uuid, sizeof(uuid),"uuid=%s", svc_uuid);

	zresolve_t res = {0};
	res.proto =	 AVAHI_PROTO_UNSPEC;
	res.interface = AVAHI_IF_UNSPEC;
	res.type =  (char *) RTAPI_DNSSD_SUBTYPE   MACHINEKIT_DNSSD_SERVICE_TYPE;
	res.match = uuid;
	res.domain = NULL;
	res.name = (char *)"";
	res.timeout_ms = 3000;
	res.result = SD_UNSET;

	resolve_context_t *p  = ll_zeroconf_resolve(&res);

	if (res.result == SD_OK) {
	    // fish out the dsn=<uri> TXT record

	    AvahiStringList *dsn = avahi_string_list_find(res.txt, "dsn");
	    char *key;
	    size_t vlen;

	    if (avahi_string_list_get_pair(dsn, &key, &uri, &vlen)) {
		fprintf(stderr,
			"halcmd: service discovery failed - no dsn= key\n");
		return -1;
	    }
	} else {
	    fprintf(stderr,
		    "halcmd: service discovery failed - cant retrieve rtapi_app command uri: %d\n",
		    res.result );
	    return -1;
	}
	ll_zeroconf_resolve_free(p);
    }
#endif

    z_context = zctx_new ();
    assert(z_context);
    z_command = zsocket_new (z_context, ZMQ_DEALER);
    assert(z_command);

    char z_ident[30];
    snprintf(z_ident, sizeof(z_ident), "halcmd%d",getpid());

    zsocket_set_identity(z_command, z_ident);
    zsocket_set_linger(z_command, 0);

    if (zsocket_connect(z_command, uri)) {
	perror("connect");
	return -EINVAL;
    }
    zsocket_set_rcvtimeo (z_command, timeout * ZMQ_POLL_MSEC);

    return rtapi_ping(instance);
}
