/********************************************************************
* Description: nmldiag.cc
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

#include "nml.hh"		// NML_MAIN_Channel_List
#include "nmldiag.hh"
#include "rcs_print.hh"
#include "linklist.hh"
#include <time.h>

static char access_name[9][40] = {
    "ZERO",
    "READ",
    "CHECK_IF_READ",
    "PEEK",
    "WRITE",
    "WRITE_IF_READ",
    "CLEAR",
    "GET_MSG_COUNT",
    "GET_DIAG_INFO"
};

void NML_DIAGNOSTICS_INFO::print()
{
    if (NULL != last_writer_dpi) {
	rcs_print("Last writer = %ld (%s)\n", last_writer,
	    last_writer_dpi->name);
    }
    if (NULL != last_reader_dpi) {
	rcs_print("Last reader = %ld (%s)\n", last_reader,
	    last_reader_dpi->name);
    }
    if (NULL == dpis) {
	return;
    }
    CMS_DIAG_PROC_INFO *dpi = (CMS_DIAG_PROC_INFO *) dpis->get_head();
    while (NULL != dpi) {
	rcs_print("\n");
	rcs_print("Info for process %s:\n", dpi->name);
	rcs_print("\t Host and system info: %s\n", dpi->host_sysinfo);
	rcs_print("\t Process Id: %ld\n", dpi->pid);
	rcs_print("\t RCS Library Version: %f\n", dpi->rcslib_ver);
	if (dpi->access_type >= 0 && dpi->access_type <= 9) {
	    rcs_print("\t Last operation:  %d (%s)\n", dpi->access_type,
		access_name[dpi->access_type]);
	}
	rcs_print("\t msg_id: %ld\n", dpi->msg_id);
	rcs_print("\t msg_size: %ld\n", dpi->msg_size);
	rcs_print("\t msg_type: %ld\n", dpi->msg_type);
	rcs_print("\t number_of_accesses: %ld\n", dpi->number_of_accesses);
	rcs_print("\t number_of_new_messages: %ld\n",
	    dpi->number_of_new_messages);
	rcs_print("\t bytes_moved: %f\n", dpi->bytes_moved);
	time_t t = 0;
	const char *ctime_ret = "";
	if (dpi->first_access_time > 0.0) {
	    t = (time_t) dpi->first_access_time;
	    ctime_ret = ctime(&t);
	    if (NULL == ctime_ret) {
		ctime_ret = "";
	    }
	}
	rcs_print("\t first_access_time: %f :  %s\n", dpi->first_access_time,
	    ctime_ret);
	ctime_ret = "";
	if (dpi->last_access_time > 0.0) {
	    t = (time_t) dpi->last_access_time;
	    ctime_ret = ctime(&t);
	    if (NULL == ctime_ret) {
		ctime_ret = "";
	    }
	}
	rcs_print("\t last_access_time: %f  : %s\n", dpi->last_access_time,
	    ctime_ret);

	if (dpi->max_difference >= dpi->min_difference) {
	    rcs_print("\t Maximum time between accesses: %f\n",
		dpi->max_difference);
	    rcs_print("\t Minumum time between accesses: %f\n",
		dpi->min_difference);
	}
	double total_time = dpi->last_access_time - dpi->first_access_time;
	if (total_time > 0) {
	    int h, m, s;
	    h = ((int) total_time) / 3600;
	    m = ((int) total_time - h * 60) / 60;
	    s = ((int) total_time - h * 3600 - m * 60);
	    rcs_print
		("\t Time between first and last access: %f -- %02d:%02d:%02d\n",
		total_time, h, m, s);
	    if (dpi->number_of_accesses > 0) {
		rcs_print("\t Average time between accesses: %f\n",
		    (total_time) / dpi->number_of_accesses);
	    }
	    if (dpi->number_of_new_messages > 0) {
		rcs_print("\t Average time between new messages: %f\n",
		    (total_time) / dpi->number_of_new_messages);
	    }
	    if (dpi->bytes_moved > 0) {
		rcs_print("\t Average bytes moved per second: %f\n",
		    dpi->bytes_moved / (total_time));
	    }
	}
	if (dpi->bytes_moved > 0 && dpi->number_of_new_messages > 0) {
	    rcs_print("\t Average bytes moved per message: %f\n",
		dpi->bytes_moved / (dpi->number_of_new_messages));
	}
	dpi = (CMS_DIAG_PROC_INFO *) dpis->get_next();
    }
}

int nml_print_diag_list()
{
    if (NULL != NML_Main_Channel_List) {
	NML *nml = (NML *) NML_Main_Channel_List->get_head();
	while (NULL != nml) {
	    if (NULL != nml->cms) {
		if (!nml->cms->enable_diagnostics) {
		    nml = (NML *) NML_Main_Channel_List->get_next();
		    continue;
		}
		rcs_print
		    ("\n*********************************************\n");
		if (NULL != nml->cms->BufferName) {
		    rcs_print("* Buffer Name: %s\n", nml->cms->BufferName);
		}
		NML_DIAGNOSTICS_INFO *ndi = nml->get_diagnostics_info();
		if (NULL != ndi) {
		    ndi->print();
		}
		rcs_print
		    ("*********************************************\n\n");
	    }
	    nml = (NML *) NML_Main_Channel_List->get_next();
	}
    }
    return (0);
}
