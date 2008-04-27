/********************************************************************
* Description: cmssvrp.cc
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/

extern "C" void cms_print_servers();

void cms_print_servers()
{
    CMS_SERVER *cms_server;

    rcs_print("cms_server_count=%d\n", cms_server_count);
    if (NULL == cms_server_list) {
	rcs_print("cms_server_list is NULL.\n");
	return;
    }

    cms_server = (CMS_SERVER *) cms_server_list->get_head();
    rcs_print("CMS Server Tasks:\n");
    rcs_print
	("\t server_pid, \tnum_buffers, \tport,\t max_clients,\t cur_clients,\t requests_processed\n");
    while (NULL != cms_server) {
	int num_buffers = 0;
	if (cms_server->cms_local_ports != NULL) {
	    num_buffers = cms_server->cms_local_ports->list_size;
	}
	int port_num = 0;
	int max_clients = 0;
	int current_clients = 0;
	int requests_processed = cms_server->requests_processed;
	if (cms_server->remote_port != NULL) {
	    port_num = cms_server->remote_port->port_num;
	    max_clients = cms_server->remote_port->max_clients;
	    current_clients = cms_server->remote_port->current_clients;
	}
	rcs_print(" \t%d (0x%X),\t %d,\t %d,\t %d,\t %d,\t %d\n",
	    cms_server->server_pid, cms_server->server_pid,
	    num_buffers, port_num,
	    max_clients, current_clients, requests_processed);
	cms_server = (CMS_SERVER *) cms_server_list->get_next();
    }
}
