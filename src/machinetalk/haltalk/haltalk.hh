/*
 * Copyright (C) 2013-2014 Michael Haberler <license@mah.priv.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_LOCAL1  // where all rtapi/ulapi logging goes
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <uuid/uuid.h>
#include <czmq.h>

#include <string>
#include <unordered_map>

#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

#include <rtapi.h>
#include <hal.h>
#include <hal_priv.h>
#include <hal_group.h>
#include <hal_rcomp.h>
#include <inifile.h>
#include <syslog_async.h>

#include "halitem.h"
#include "mk-zeroconf.hh"
#include "select_interface.h"

#include <machinetalk/generated/message.pb.h>
namespace gpb = google::protobuf;

// announced protocol versions
#define HAL_HALGROUP_STATUS_VERSION 2
#define HAL_HALRCOMP_STATUS_VERSION 2
#define HAL_RCOMMAND_VERSION     2

#if JSON_TIMING
#include <machinetalk/json2pb/json2pb.h>
#include <jansson.h>
#endif

typedef struct htself htself_t;

typedef struct {
    hal_compiled_group_t *cg;
    int serial; // must be unique per active group
    unsigned flags;
    htself_t *self;
    int timer_id; // > -1: scan timer active - subscribers present
    int msec;
} group_t;

typedef struct {
    hal_compiled_comp_t *cc;
    int serial; // must be unique per active comp
    unsigned flags;
    htself_t *self;
    int timer_id;
    int msec;
} rcomp_t;

typedef struct htbridge {
    int state;
    void *z_bridge_status;
    void *z_bridge_cmd;
    int timer_id;
} htbridge_t;

// groups indexed by group name
typedef std::unordered_map<std::string, group_t *> groupmap_t;
typedef groupmap_t::iterator groupmap_iterator;

// remote components indexed by component name
typedef std::unordered_map<std::string, rcomp_t *> compmap_t;
typedef compmap_t::iterator compmap_iterator;

// HAL items indexed by handle
typedef std::unordered_map<int, halitem_t *> itemmap_t;
typedef itemmap_t::iterator itemmap_iterator;

typedef struct htconf {
    const char *progname;
    const char *inifile;
    const char *section;
    const char *modname;
    const char *interface;
    const char *ipaddr;
    const char *halgroup;
    const char *halrcomp;
    const char *command;
    const char *interfaces;
    const char *bridgecomp;
    const char *bridgecomp_cmduri;
    const char *bridgecomp_updateuri;
    int bridge_target_instance;

    int paranoid; // extensive runtime checks - may be costly
    int debug;
    int default_group_timer; // msec
    int default_rcomp_timer; // msec
    int keepalive_timer; // msec; disabled if zero
    unsigned ifIndex;
    char *service_uuid;
    int remote;
    bool trap_signals;
} htconf_t;

typedef struct htself {
    htconf_t *cfg;
    uuid_t    process_uuid;      // server instance (this process)
    uuid_t    svc_uuid;  // service instance (set of running server processes)
    int       comp_id;
    int       signal_fd;
    bool      interrupted;
    pid_t     pid;

    pb::Container rx; // any ParseFrom.. function does a Clear() first
    pb::Container tx; // tx must be Clear()'d after or before use

    AvahiCzmqPoll *av_loop;
    register_context_t *halgroup_publisher;
    register_context_t *halrcomp_publisher;
    register_context_t *halrcmd_publisher;

    zctx_t   *z_context;
    zloop_t  *z_loop;

    void       *z_halgroup;
    int        z_group_port;
    const char *z_halgroup_dsn;
    groupmap_t groups;

    void       *z_halrcomp;
    const char *z_halrcomp_dsn;
    int        z_rcomp_port;
    compmap_t  rcomps;

    void       *z_halrcmd;
    const char *z_halrcmd_dsn;
    int        z_halrcmd_port;
    itemmap_t  items;

    htbridge_t *bridge;
} htself_t;


// haltalk_group.cc:
int scan_groups(htself_t *self);
int release_groups(htself_t *self);
int handle_group_timer(zloop_t *loop, int timer_id, void *arg);
int handle_group_input(zloop_t *loop, zmq_pollitem_t *poller, void *arg);
int ping_groups(htself_t *self);

// haltalk_rcomp.cc:
int scan_comps(htself_t *self);
int release_comps(htself_t *self);
int handle_rcomp_input(zloop_t *loop, zmq_pollitem_t *poller, void *arg);
int handle_rcomp_timer(zloop_t *loop, int timer_id, void *arg);
int ping_comps(htself_t *self);

// haltalk_zeroconf.cc:
int ht_zeroconf_announce(htself_t *self);
int ht_zeroconf_withdraw(htself_t *self);

// haltalk_command.cc:
int handle_command_input(zloop_t *loop, zmq_pollitem_t *poller, void *arg);

// haltalk_introspect.cc:
int process_describe(htself_t *self, const std::string &from,  void *socket);
int describe_group(htself_t *self, const char *group, const std::string &from,  void *socket);
int describe_comp(htself_t *self, const char *comp, const std::string &from,  void *socket);
int describe_parameters(htself_t *self);

// haltalk_bridge.cc:
int bridge_init(htself_t *self);
