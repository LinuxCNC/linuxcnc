

#include "config.h"

#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_LOCAL1  // where all rtapi/ulapi logging goes
#endif

#include <mk-service.hh>
#include <mk-zeroconf.hh>
#include <inifile.h>
#include <syslog_async.h>


#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

typedef vector<string> argvec_t;

int mk_getnetopts(mk_netopts_t *n)
{
    const char *s, *inifile;
    const char *mkini = "MACHINEKIT_INI";
    char buffer[PATH_MAX];
    int retval = -1;

    assert(n != NULL);
    assert(n->rundir != NULL);

    if (gethostname(buffer, sizeof(buffer)) < 0) {
	syslog_async(LOG_ERR, "gethostname() failed ?! %s\n",
		     strerror(errno));
	goto DONE;
    }
    strtok(buffer, "."); // get rid of the domain name
    n->hostname = strdup(buffer);

    uuid_generate_time(n->proc_uuid);
    uuid_unparse(n->proc_uuid, buffer);
    n->process_uuid = strdup(buffer);

    if ((inifile = getenv(mkini)) == NULL) {
	syslog_async(LOG_ERR, "FATAL - '%s' missing in environment\n", mkini);
	goto DONE;
    }
    if (n->mkinifp)
	fclose(n->mkinifp);

    if ((n->mkinifp = fopen(inifile,"r")) == NULL) {
	syslog_async(LOG_ERR, "cant open inifile '%s'\n", inifile);
	return retval;
    }

    if (n->service_uuid == NULL) {
	n->service_uuid = (char *) iniFind(n->mkinifp, "MKUUID", "MACHINEKIT");
	if (n->service_uuid != NULL)
	    n->service_uuid = strdup(n->service_uuid);
    }
    if (n->service_uuid == NULL) {
	syslog_async(LOG_ERR, "no [MACHINEKIT]MKUUID found in %s\n", inifile);
	goto DONE;
    }

    if (uuid_parse(n->service_uuid, n->svc_uuid)) {
	syslog_async(LOG_ERR, "service UUID: syntax error: '%s'",
		     n->service_uuid);
	goto DONE;
    }


    iniFindInt(n->mkinifp, "REMOTE", "MACHINEKIT", &n->remote);

    // default behaviour: announce on both v4 and v6 addresses
    if (iniFindInt(n->mkinifp, "ANNOUNCE_IPV4", "MACHINEKIT", &n->announce_ipv4))
	n->announce_ipv4 = 1;
    if (iniFindInt(n->mkinifp, "ANNOUNCE_IPV6", "MACHINEKIT", &n->announce_ipv6))
	n->announce_ipv6 = 1;

    n->bind_ipv4 = NULL;
    if ((s = iniFind(n->mkinifp, "BIND_IPV4", "MACHINEKIT")) != NULL)
	n->bind_ipv4 = strdup(s);

    // default behaviour: bind * on an ipv6 enabled socket
    n->bind_ipv6 = "*";
    if ((s = iniFind(n->mkinifp, "BIND_IPV6", "MACHINEKIT")) != NULL)
	n->bind_ipv6 = strdup(s);

    retval = 0;
 DONE:
    return retval;
}

static int bind_ifs(mk_socket_t *s, const argvec_t &ifs)
{
    for (size_t i = 0; i < ifs.size(); i++) {
	string uri;
	if ((i == 0) && (s->port < 0)) {
	    // first bind on ephemeral port
	    // subsequent binds must be explicitly done
	    // on the port assigned
	    uri = "tcp://" + ifs[i] + ":*";
	} else {
	    uri = "tcp://" + ifs[i] + ":" + boost::lexical_cast<std::string>(s->port);
	}
	// use this port number for the rest of the ifs
	s->port = zsocket_bind(s->socket, uri.c_str());
	if (s->port < 0) {
	    syslog_async(LOG_ERR, "bind to '%s' failed: %s",
			 uri.c_str(), strerror(errno));
	    return -1;
	}
    }
    return 0;
}

int mk_bindsocket(mk_netopts_t *n, mk_socket_t *s)
{
    argvec_t ifs;
    int retval = 0;
    char buf[PATH_MAX];
    string delims("\t ");

    assert(n != NULL);
    assert(s != NULL);
    assert(s->socket != NULL);
    assert(s->tag != NULL);
    if (s->dnssd_type == NULL)
	s->dnssd_type = MACHINEKIT_DNSSD_SERVICE_TYPE;

    if (n->remote) {

	// first bind all V4 interfaces/addresses
	if (n->bind_ipv4) {
	    string input(n->bind_ipv4);
	    boost::split(ifs, input, boost::is_any_of(delims),
			 boost::algorithm::token_compress_on);
	    retval = bind_ifs(s, ifs);
	}

	if (retval)
	    return retval;

	if (n->bind_ipv6) {
	    string input(n->bind_ipv6);
	    boost::split(ifs, input, boost::is_any_of("\t "),
			 boost::algorithm::token_compress_on);

	    // if there are any V6 interfaces/addresses,
	    // enable V6 on socket now
	    if (ifs.size() > 0) {
		zsocket_set_ipv6 (s->socket, 1);
		assert (zsocket_ipv6 (s->socket) == 1);
	    }
	    // and bind them all
	    retval = bind_ifs(s, ifs);
	}
	// construct URI for mDNS announcement
	snprintf(buf, sizeof(buf), "dsn=tcp://%%s:%d", s->port);
	s->announced_uri = strdup(buf);
    } else {
	// use IPC sockets
	snprintf(buf, sizeof(buf), ZMQIPC_FORMAT,
		 n->rundir, n->rtapi_instance, s->tag, n->service_uuid);
	s->port = zsocket_bind(s->socket, buf);
	if (s->port < 0)
	    syslog_async(LOG_ERR, "bind(%s): %s\n", buf, strerror(errno));

	// construct URI for mDNS announcement
	char dsn[PATH_MAX];

	snprintf(dsn, sizeof(dsn), "dsn=%s", buf);
	s->announced_uri = strdup(dsn);
    }
    return retval;
}

int mk_announce(mk_netopts_t *n, mk_socket_t *s, const char *headline, const char *path)
{
    char name[PATH_MAX];
    int protocol = AVAHI_PROTO_UNSPEC; // default: both ipv4 and ipv6

    assert(n != NULL);

    // dont't announce if both ANNOUNCE_IPV4 and ANNOUNCE_IPV6 are zero
    if (!(n->announce_ipv4 || n->announce_ipv4))
	return 0;

    // determine avahi announcement mode
    if (!n->announce_ipv4)
	protocol = AVAHI_PROTO_INET6;

    if (!n->announce_ipv6)
	protocol = AVAHI_PROTO_INET;

    assert(s != NULL);
    assert(headline != NULL);

    snprintf(name, sizeof(name), "%s on %s.local pid %d", headline, n->hostname, getpid());
    s->publisher = zeroconf_service_announce(name,
					     s->dnssd_type,
					     s->dnssd_subtype,
					     s->port,
					     s->announced_uri,
					     n->service_uuid,
					     n->process_uuid,
					     s->tag,
					     path, // http only, otherwise NULL
					     protocol,
					     n->av_loop);
    if (s->publisher == NULL) {
	syslog_async(LOG_ERR, "failed to start zeroconf  publisher for '%s'\n",
		     name);
	return -1;
    }
    return 0;
}

int mk_withdraw(const mk_socket_t *s)
{
    if (s->publisher)
	zeroconf_service_withdraw(s->publisher);
    return 0;
}
