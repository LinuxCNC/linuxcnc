/*************************************************************************
* File:nml_srv.hh                                                        *
* Authors: Fred Proctor, Will Shackleford                                *
* Purpose: C++ Header file for RPC server that reads and writes          *
*          to a local NML buffer for remote processes.                   *
* Includes:                                                              *
*          1. class NML_SERVER                                           *
*************************************************************************/

#ifndef NML_SERVER_HH
#define NML_SERVER_HH

extern "C" {

#include <sys/types.h>
#include <unistd.h>		/* pid_t */
}
#include "cms_srv.hh"		/* class CMS_SERVER */
#include "nml.hh"		/* class NML */
#include "rem_msg.hh"
class NML_SERVER_LOCAL_PORT:public CMS_SERVER_LOCAL_PORT {
  protected:
    NML * nml;
    REMOTE_READ_REPLY *reader(REMOTE_READ_REQUEST * _req);
    REMOTE_READ_REPLY *blocking_read(REMOTE_READ_REQUEST * _req);
    REMOTE_WRITE_REPLY *writer(REMOTE_WRITE_REQUEST * _req);
    REMOTE_SET_DIAG_INFO_REPLY *set_diag_info(REMOTE_SET_DIAG_INFO_REQUEST *
	buf);
    REMOTE_GET_DIAG_INFO_REPLY *get_diag_info(REMOTE_GET_DIAG_INFO_REQUEST *
	buf);
    REMOTE_GET_MSG_COUNT_REPLY *get_msg_count(REMOTE_GET_DIAG_INFO_REQUEST *
	buf);
    void reset_diag_info();

    friend class NML_SUPER_SERVER;
    friend class NML_SERVER;
    int batch_list_id;

  public:
      NML_SERVER_LOCAL_PORT(NML * _nml);
      virtual ~ NML_SERVER_LOCAL_PORT();
};

class NML_SERVER:public CMS_SERVER {
  protected:
    int super_server_list_id;
    int being_deleted;

  public:
      NML_SERVER(NML * _nml, int set_to_master = 0);
      virtual ~ NML_SERVER();
    void delete_from_list();
    void add_to_nml_server_list();
    friend class NML_SUPER_SERVER;
};

class NML_SUPER_SERVER {
  public:
    LinkedList * servers;
    NML_SUPER_SERVER();
    ~NML_SUPER_SERVER();
    void add_to_list(NML *);
    void add_to_list(NML_SERVER *);
    void spawn_all_servers();
    void kill_all_servers();
    void delete_all_servers();
    int unspawned_servers;
};

extern NML_SUPER_SERVER *NML_Default_Super_Server;
extern void run_nml_servers();
extern void spawn_nml_servers();
extern void kill_nml_servers();

extern "C" {
    extern void nml_server_cleanup();
}
#endif				/* !NML_SERVER_HH */
