/********************************************************************
* Description: tcp_srv.hh
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

#ifndef TCP_SRV_HH
#define TCP_SRV_HH

#include "cms_srv.hh"		/* class CMS_SERVER_REMOTE_PORT */
#include "linklist.hh"		/* class LinkedList */
#include "rem_msg.hh"

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>		/* memset(), strerror() */
#include <netinet/in.h>
#include <errno.h>		/* errno */
#include <signal.h>		// SIGPIPE, signal()
#include <sys/time.h>           /* struct timeval */

#ifdef __cplusplus
}
#endif

#ifndef NO_THREADS
#define NO_THREADS

#ifdef POSIX_THREADS
#include <pthread.h>
#endif
#endif

#define MAX_TCP_BUFFER_SIZE 16
class CLIENT_TCP_PORT;

class CMS_SERVER_REMOTE_TCP_PORT:public CMS_SERVER_REMOTE_PORT {
  public:
    CMS_SERVER_REMOTE_TCP_PORT(CMS_SERVER * _cms_server);
    virtual ~ CMS_SERVER_REMOTE_TCP_PORT();
    int accept_local_port_cms(CMS *);
    void run();
    void register_port();
    void unregister_port();
    double dtimeout;
  protected:
      fd_set read_fd_set, write_fd_set;
    void handle_request(CLIENT_TCP_PORT *);
    int maxfdpl;
    LinkedList *client_ports;
    LinkedList *subscription_buffers;
    int connection_socket;
    long connection_port;
    struct sockaddr_in server_socket_address;
    REMOTE_CMS_REQUEST *request;
    char temp_buffer[0x2000];
    int current_poll_interval_millis;
    int polling_enabled;
    struct timeval select_timeout;
    void update_subscriptions();
    void add_subscription_client(int buffer_number, int subscription_type,
	int poll_interval_millis, CLIENT_TCP_PORT * clnt);
    void remove_subscription_client(CLIENT_TCP_PORT * clnt,
	int buffer_number);
    void recalculate_polling_interval();
    void switch_function(CLIENT_TCP_PORT *
	_client_tcp_port,
	CMS_SERVER * server, long request_type, long buffer_number, long
	received_serial_number);
};

class TCP_BUFFER_SUBSCRIPTION_INFO {
  public:
    TCP_BUFFER_SUBSCRIPTION_INFO();
    ~TCP_BUFFER_SUBSCRIPTION_INFO();
    int buffer_number;
    int min_last_id;
    int list_id;
    LinkedList *sub_clnt_info;
};

class TCP_CLIENT_SUBSCRIPTION_INFO {
  public:
    TCP_CLIENT_SUBSCRIPTION_INFO();
    ~TCP_CLIENT_SUBSCRIPTION_INFO();
    int subscription_type;
    int poll_interval_millis;
    double last_sub_sent_time;
    int subscription_list_id;
    int buffer_number;
    int subscription_paused;
    int last_id_read;
    TCP_BUFFER_SUBSCRIPTION_INFO *sub_buf_info;
    CLIENT_TCP_PORT *clnt_port;
};

class TCPSVR_BLOCKING_READ_REQUEST;

class CLIENT_TCP_PORT {
  public:
    CLIENT_TCP_PORT();
    ~CLIENT_TCP_PORT();
    long serial_number;
    int errors, max_errors;
    struct sockaddr_in address;
    int socket_fd;
    LinkedList *subscriptions;
    pid_t tid;
    pid_t pid;
    int blocking;
#ifdef POSIX_THREADS
    pthread_t threadId;
#else
    int threadId;
#endif
    TCPSVR_BLOCKING_READ_REQUEST *blocking_read_req;
    REMOTE_SET_DIAG_INFO_REQUEST *diag_info;

};

class TCPSVR_BLOCKING_READ_REQUEST:public REMOTE_BLOCKING_READ_REQUEST {
  public:
    TCPSVR_BLOCKING_READ_REQUEST();
    ~TCPSVR_BLOCKING_READ_REQUEST();
    CLIENT_TCP_PORT *_client_tcp_port;
    CMS_SERVER_REMOTE_TCP_PORT *remport;
    CMS_SERVER *server;
    REMOTE_BLOCKING_READ_REPLY *read_reply;
};

#endif /* TCP_SRV_HH */
