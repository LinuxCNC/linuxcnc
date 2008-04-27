/********************************************************************
* Description: cms_srv.hh
*   C++ Header file for server that reads and writes to a local CMS
*   buffer for remote processes.
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
/*************************************************************************
* File:cms_srv.hh
* Authors: Fred Proctor, Will Shackleford
* Purpose: 
* Includes:
*          1. class CMS_SERVER
*          2. class CMS_SERVER_LOCAL_PORT
*************************************************************************/

#ifndef CMS_SERVER_HH
#define CMS_SERVER_HH

#include "cms_user.hh"		/* class CMS, CMS_STATUS */
#include "cms_cfg.hh"		/* CMS_CONFIG_LINELEN */
#include "rem_msg.hh"		/* struct REMOTE_READ_REQUEST, */
				/* struct REMOTE_WRITE_REQUEST, */
extern int cms_server_count;
extern void wait_for_servers(int);

class LinkedList;
extern LinkedList *cms_server_list;

class CMS_SERVER;
class CMS_DIAG_PROC_INFO;

class CMS_SERVER_LOCAL_PORT:public virtual CMS_USER {
  protected:
    long buffer_number;
    int list_id;
    friend class CMS_SERVER;
    CMS_DIAG_PROC_INFO *orig_info;

    /* virtual functions for accessing local buffer. */
    virtual REMOTE_READ_REPLY *reader(REMOTE_READ_REQUEST * req);
    virtual REMOTE_READ_REPLY *blocking_read(REMOTE_READ_REQUEST * req);
    virtual REMOTE_WRITE_REPLY *writer(REMOTE_WRITE_REQUEST * buf);
    virtual REMOTE_SET_DIAG_INFO_REPLY
	* set_diag_info(REMOTE_SET_DIAG_INFO_REQUEST * buf);
    virtual REMOTE_GET_DIAG_INFO_REPLY
	* get_diag_info(REMOTE_GET_DIAG_INFO_REQUEST * buf);
    virtual REMOTE_GET_MSG_COUNT_REPLY
	* get_msg_count(REMOTE_GET_DIAG_INFO_REQUEST * buf);
    virtual void reset_diag_info();
    REMOTE_READ_REPLY read_reply;
    REMOTE_WRITE_REPLY write_reply;
    REMOTE_GET_BUF_NAME_REPLY namereply;
    REMOTE_GET_DIAG_INFO_REPLY get_diag_info_reply;

  public:
      CMS_SERVER_LOCAL_PORT(CMS * _cms);
      virtual ~ CMS_SERVER_LOCAL_PORT();
    int local_channel_reused;

};

class CMS_USER_INFO;

struct CMS_USER_CONNECT_STRUCT {
    CMS_USER_CONNECT_STRUCT();
    CMS_USER_INFO *user_info;
    int fd;
};

class CMS_SERVER_REMOTE_PORT {
  public:
    CMS_SERVER_REMOTE_PORT(CMS_SERVER *);
    virtual ~ CMS_SERVER_REMOTE_PORT();
    virtual void run() = 0;
    virtual void register_port();
    virtual void unregister_port();
    virtual int accept_local_port_cms(CMS * cms);
    int port_registered;
    CMS_USER_INFO *current_user_info;
    CMS_USER_INFO *get_connected_user(int);
    void add_connected_user(int);

  protected:
      LinkedList * connected_users;
    CMS_USER_CONNECT_STRUCT *current_connected_user_struct;
    CMS_SERVER *cms_server_parent;
    static CMS_SERVER *find_server(long _pid, long _tid = 0);
    static void print_servers();
    friend class CMS_SERVER;
    double min_compatible_version;
    int confirm_write;
  public:
    int running;
    int max_total_subdivisions;
    int port_num;
    int max_clients;
    int current_clients;
};

class CMS_SERVER {
  public:
    REMOTE_CMS_REQUEST *request;
    int server_spawned;
    int server_registered;
    int list_id;
    LinkedList *cms_local_ports;

    CMS_SERVER_LOCAL_PORT *find_local_port(long _buffer_num);
    REMOTE_CHECK_IF_READ_REPLY cir_reply;
    REMOTE_GET_MSG_COUNT_REPLY gmc_reply;
    REMOTE_GET_QUEUE_LENGTH_REPLY gql_reply;
    REMOTE_GET_SPACE_AVAILABLE_REPLY gsa_reply;
    REMOTE_CLEAR_REPLY clear_reply_struct;
    int using_passwd_file;
    int get_access_type();
    long get_message_type();
    const char *get_buffer_name(int _buf_num);
    int requests_processed;
//    void read_passwd_file();

  public:
    int get_total_subdivisions(long _buffer_num);
    CMS_SERVER_REMOTE_PORT *remote_port;
    void gen_random_key(char key[], int len);
    int security_check(CMS_USER_INFO * user_info, int _buf_num);
    int is_using_passwd_file();
    CMS_USER_INFO *get_user_info(const char *name, const char *passwd);
//    int get_user_keys(const char *name, char *key1, char *key2);

    static void clean(int);

    long current_pid;
    long current_tid;
    long creator_pid;
    long creator_tid;
    long spawner_pid;
    long spawner_tid;
    long server_pid;
    long server_tid;

    long maximum_cms_size;
    REMOTE_CMS_REPLY *process_request(REMOTE_CMS_REQUEST *);
    void register_server(int setup_CC_signal_handler = 1);
    void unregister_server();
    void run(int setup_CC_signal_handler = 1);
    int spawn();
    void kill_server();
      CMS_SERVER();
    void add_local_port(CMS_SERVER_LOCAL_PORT *);
    void delete_all_local_ports();
    virtual void delete_from_list();
      virtual ~ CMS_SERVER();
    virtual void initialize_write_request_space();
    virtual int accept_local_port_cms(CMS *);
    REMOTE_READ_REQUEST read_req;
    REMOTE_WRITE_REQUEST write_req;
    REMOTE_GET_KEYS_REQUEST get_keys_req;
    REMOTE_LOGIN_REQUEST login_req;
    REMOTE_SET_SUBSCRIPTION_REQUEST set_subscription_req;
    REMOTE_CHECK_IF_READ_REQUEST check_if_read_req;
    REMOTE_GET_MSG_COUNT_REQUEST get_msg_count_req;
    REMOTE_GET_QUEUE_LENGTH_REQUEST get_queue_length_req;
    REMOTE_GET_SPACE_AVAILABLE_REQUEST get_space_available_req;
    REMOTE_CLEAR_REQUEST clear_req;
    REMOTE_SET_DIAG_INFO_REQUEST set_diag_info_req;
    REMOTE_GET_DIAG_INFO_REQUEST get_diag_info_req;
    REMOTE_READ_REPLY *read_reply;
    REMOTE_WRITE_REPLY *write_reply;
    REMOTE_GET_KEYS_REPLY *get_keys_reply;
    REMOTE_GET_KEYS_REPLY perm_get_keys_reply;
    REMOTE_LOGIN_REPLY *login_reply;
    REMOTE_LOGIN_REPLY perm_login_reply;
    REMOTE_SET_SUBSCRIPTION_REPLY *set_subscription_reply;
    REMOTE_SET_SUBSCRIPTION_REPLY perm_set_subscription_reply;
    REMOTE_CHECK_IF_READ_REPLY *check_if_read_reply;
    REMOTE_GET_MSG_COUNT_REPLY *get_msg_count_reply;
    REMOTE_GET_QUEUE_LENGTH_REPLY *get_queue_length_reply;
    REMOTE_GET_SPACE_AVAILABLE_REPLY *get_space_available_reply;
    REMOTE_CLEAR_REPLY *clear_reply;
    REMOTE_SET_DIAG_INFO_REPLY *set_diag_info_reply;
    REMOTE_GET_DIAG_INFO_REPLY *get_diag_info_reply;
    CMS_SERVER_LOCAL_PORT *last_local_port_used;
    int diag_enabled;
    char set_diag_info_buf[0x400];
    int max_total_subdivisions;
    virtual void set_diag_info(REMOTE_SET_DIAG_INFO_REQUEST * _diag_info);
    virtual void reset_diag_info(int buffer_number);

  private:
    double time_of_last_key_request;
    LinkedList *known_users;
    char passwd_file[256];
    CMS_USER_INFO *find_user(const char *);
    int guest_can_read;
    int guest_can_write;

};

extern int (*detailed_security_check) (const char *user_name,
    const char *buffer_name, long msg_type, int access_type);

#endif /* !CMS_SERVER_HH */
