/*************************************************************************
* File: nml.cc                                                           *
* Authors: Fred Proctor, Will Shackleford                                *
* Purpose: C++ file for the  Neutral Manufacturing Language (NML).       *
*          Includes:                                                     *
*                    1. Member functions for class NML.                  *
*************************************************************************/

/* Include Files */
#include "rcsversion.h"		/* lib version */

extern "C" {

#include <string.h>		/* memcpy() */
#include <stdlib.h>		/* atexit() */
#include <sys/param.h>		/* MAXHOSTNAMELEN */

}
#include "nml.hh"		/* class NML */
#include "nmlmsg.hh"		/* class NMLmsg */
#include "cms.hh"		/* class CMS */
#include "timer.hh"		/* esleep() */
#include "nml_srv.hh"		/* NML_Default_Super_Server */
#include "cms_cfg.hh"		/* cms_config(), cms_copy() */
#include "linklist.hh"		/* class LinkedList */
#include "rcs_print.hh"		/* rcs_print_error() */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif
#include "nmldiag.hh"		/* NML_DIAGNOSTICS_INFO */
/* Pointer to a global list of NML channels. */
    LinkedList *NML_Main_Channel_List = (LinkedList *) NULL;
LinkedList *Dynamically_Allocated_NML_Objects = (LinkedList *) NULL;

int nml_print_hostname_on_error = 0;

char NML_ERROR_TYPE_STRINGS[8][80] = {
    "NML_NO_ERROR",
    "NML_BUFFER_NOT_READ",
    "NML_TIMED_OUT",
    "NML_INVALID_CONFIGURATION",
    "NML_FORMAT_ERROR",
    "NML_INTERNAL_CMS_ERROR",
    "NML_NO_MASTER_ERROR",
    "NML_INVALID_MESSAGE_ERROR",
};

static char *default_nml_config_file = NULL;
int nml_reset_errors_printed = 1;

void set_default_nml_config_file(const char *cfg_file)
{
    if (cfg_file == NULL) {
	default_nml_config_file = NULL;
    }
    default_nml_config_file = (char *) malloc(strlen(cfg_file) + 1);
    strcpy(default_nml_config_file, cfg_file);
}

const char *get_default_nml_config_file()
{
    return default_nml_config_file;
}

/*
* NML Member Functions
*/

/* Special new operator to allow the nml_cleanup function to
distinguish between dynamically and statically allocated NML
objects. */
void *NML::operator                          new(size_t size)
{
    if (size < sizeof(NML)) {
	rcs_print_error
	    ("void *NML::operator new() called with size (%d) < sizeof(NML) (%d) the code calling NML was probably not compiled with the correct header file version.\n",
	    size, sizeof(NML));
	size = sizeof(NML);
    }
    void *nml_space = NULL;
    char *cptr = (char *) NULL;
    int dynamic_list_id = 0;
    nml_space = malloc(size + sizeof(int) * 2);
    if (NULL != nml_space) {
	memset(nml_space, 0, size);
    }
    if (NULL == Dynamically_Allocated_NML_Objects) {
	Dynamically_Allocated_NML_Objects = new LinkedList();
    }
    if (NULL != Dynamically_Allocated_NML_Objects) {
	dynamic_list_id =
	    Dynamically_Allocated_NML_Objects->storeAtTail(nml_space,
	    sizeof(NML), 0);
	cptr = ((char *) nml_space) + sizeof(NML);
	// guarantee alignment
	cptr += sizeof(int) - (((int) cptr) % sizeof(int));
	*((int *) cptr) = dynamic_list_id;
    }
    rcs_print_debug(PRINT_NML_CONSTRUCTORS, "%X = NML::operater new(%d)\n",
	nml_space, size);
    return nml_space;
}

void NML::operator                          delete(void *nml_space)
{
    int dynamic_list_id = 0;
    char *cptr = (char *) NULL;

    rcs_print_debug(PRINT_NML_DESTRUCTORS, "NML::operater delete(%X)\n",
	nml_space);

    if (NULL == nml_space) {
	return;
    }

    if (NULL != Dynamically_Allocated_NML_Objects) {
	cptr = ((char *) nml_space) + sizeof(NML);
	cptr += sizeof(int) - (((int) cptr) % sizeof(int));
	dynamic_list_id = *((int *) cptr);
	Dynamically_Allocated_NML_Objects->deleteNode(dynamic_list_id);
	if (Dynamically_Allocated_NML_Objects->listSize == 0) {
	    delete Dynamically_Allocated_NML_Objects;
	    Dynamically_Allocated_NML_Objects = (LinkedList *) NULL;
	}
    }
    free(nml_space);
}

/******************************************************************
* Constructor for NML:
* Parameters:
* NML_FORMAT_PTR f_ptr - address of the function to be used to format messages.
* char *buf - Name of the buffer to connect to as written in the config file.
* char *proc - Name of the calling process as expected in the config file.
* char *file - Name of the configuration file.
* int set_to_server - If 1 this NML will consider its calling process to
*   be an NML_SERVER, which effects how and when messages are encoded and
*   decoded.
* int set_to_master - Passed to the CMS constructor - how this is used
*   depends on the type of CMS buffer. In general the master is responsible
*   for creating/initializing the buffer. If set_to_master == 1 then this
*   process will be considered the master, if set_to_master == 0 then
*   the the configuration file determines if this is the master, and it
*   set_to_master == -1 then this will not be the master.
* NOTES:
*  1. Borland C++(for DOS and Windows) does not allow default
*   parameters to be specified both here and in the header file.
*  2. All pointers are first initialized to NULL so that it can be determined
*  later if the constructor returned before creating the objects
*  the pointers are intended to point at.
******************************************************************/
NML::NML(NML_FORMAT_PTR f_ptr,
    char *buf, char *proc, char *file, int set_to_server, int set_to_master)
{
    cms_for_msg_string_conversions = 0;
    info_printed = 0;
    blocking_read_poll_interval = -1.0;
    forced_type = 0;
    strncpy(bufname, buf, 40);
    strncpy(procname, proc, 40);
    if (NULL == file) {
	file = default_nml_config_file;
    }
    strncpy(cfgfilename, file, 160);

    if (rcs_errors_printed >= max_rcs_errors_to_print
	&& max_rcs_errors_to_print > 0 && nml_reset_errors_printed) {
	rcs_errors_printed = 0;
	rcs_print
	    ("\nResetting rcs_errors_printed because a new NML channel is being created.\n");
    }

    already_deleted = 0;
    channel_type = NML_GENERIC_CHANNEL_TYPE;
    sizeof_message_header = sizeof(NMLmsg);

    reconstruct(f_ptr, buf, proc, file, set_to_server, set_to_master);

    if (NULL != cms) {
	cms->sizeof_message_header = sizeof_message_header;
	char *forced_type_eq = strstr(cms->buflineupper, "FORCE_TYPE=");
	if (forced_type_eq != NULL) {
	    long temp = strtol(forced_type_eq + 11, NULL, 0);
	    if (temp > 0) {
		forced_type = temp;
		fast_mode = 0;
	    }
	}
    }
}

int create_NML(NML ** nml, NML_FORMAT_PTR f_ptr,
    char *buf, char *proc, char *file)
{
    *nml = new NML(f_ptr, buf, proc, file);
    if (NULL == nml) {
	return -1;
    }
    if (!(*nml)->valid()) {
	return -1;
    }
    if (NULL != (*nml)->cms) {
	(*nml)->cms->sizeof_message_header = (*nml)->sizeof_message_header;
	char *forced_type_eq =
	    strstr((*nml)->cms->buflineupper, "FORCE_TYPE=");
	if (forced_type_eq != NULL) {
	    long temp = strtol(forced_type_eq + 11, NULL, 0);
	    if (temp > 0) {

		(*nml)->forced_type = temp;
	    }
	}
    }
    return 0;
}

void free_NML(NML * nml)
{
    if (NULL != nml) {
	delete nml;
    }
}

void NML::reconstruct(NML_FORMAT_PTR f_ptr, char *buf, char *proc,
    char *file, int set_to_server, int set_to_master)
{

    cms = (CMS *) NULL;
    format_chain = (LinkedList *) NULL;
    phantom_read = (NMLTYPE(*)())NULL;
    phantom_peek = (NMLTYPE(*)())NULL;
    phantom_write = (int (*)(NMLmsg *)) NULL;
    phantom_write_if_read = (int (*)(NMLmsg *)) NULL;
    phantom_check_if_read = (int (*)()) NULL;
    phantom_clear = (int (*)()) NULL;
    channel_list_id = 0;
    error_type = NML_NO_ERROR;
    fast_mode = 0;
    ignore_format_chain = 0;
    info_printed = 0;

    format_chain = new LinkedList;
    if (NULL != format_chain) {
	prefix_format_chain(f_ptr);
    }

    if (NULL == f_ptr) {
	rcs_print_error("NML:(Format Function Pointer) f_ptr == NULL.\n");
    }

    if (-1 == cms_config(&cms, buf, proc, file, set_to_server, set_to_master)) {
	set_error();
	if (!info_printed) {
	    print_info(buf, proc, file);
	}
	if (NULL != cms) {
	    rcs_print_debug(PRINT_NML_DESTRUCTORS, " delete (CMS *) %X;\n",
		cms);
	    delete cms;
	    cms = (CMS *) NULL;
	}
	return;
    }

    if (NULL == cms) {
	if (!info_printed) {
	    print_info(buf, proc, file);
	}
	error_type = NML_INVALID_CONFIGURATION;
	return;
    }

    if (cms->status < 0) {
	error_type = NML_INVALID_CONFIGURATION;
	if (!info_printed) {
	    print_info(buf, proc, file);
	}
	rcs_print_debug(PRINT_NML_DESTRUCTORS, " delete (CMS *) %X;\n", cms);
	delete cms;
	cms = (CMS *) NULL;
	return;
    }
    if (!set_to_server) {
	register_with_server();
    }
    add_to_channel_list();
    /* FAST MODE is a combination of options which allow certian checks
       during a read or write operation to be avoided and therefore reduce
       the NML/CMS overhead. */
    if (!cms->is_phantom &&
	cms->ProcessType == CMS_LOCAL_TYPE &&
	!cms->neutral && !cms->isserver && !cms->enable_diagnostics) {
	fast_mode = 1;
    }

    cms_status = (int *) &(cms->status);
    cms_inbuffer_header_size = &(cms->header.in_buffer_size);
    char *forced_type_eq = strstr(cms->buflineupper, "FORCE_TYPE=");
    if (forced_type_eq != NULL) {
	long temp = strtol(forced_type_eq + 11, NULL, 0);
	if (temp > 0) {
	    forced_type = temp;
	    fast_mode = 0;
	}
    }
    char *brpi_eq = strstr(cms->buflineupper, "BRPI=");
    if (brpi_eq != NULL) {
	blocking_read_poll_interval = strtod(brpi_eq + 5, NULL);
    }

}

/******************************************************************
* Constructor for NML:
* Parameters:
* char *buf - Name of the buffer to connect to as written in the config file.
* char *proc - Name of the calling process as expected in the config file.
* char *file - Name of the configuration file.
* int set_to_server - If 1 this NML will consider its calling process to
*   be an NML_SERVER, which effects how and when messages are encoded and
*   decoded.
* int set_to_master - Passed to the CMS constructor - how this is used
*   depends on the type of CMS buffer. In general the master is responsible
*   for creating/initializing the buffer. If set_to_master == 1 then this
*   process will be considered the master, if set_to_master == 0 then
*   the the configuration file determines if this is the master, and it
*   set_to_master == -1 then this will not be the master.
* NOTES:
*  1. Borland C++(for DOS and Windows) does not allow default
*   parameters to be specified both here and in the header file.
*  2. All pointers are first initialized to NULL so that it can be determined
*  later if the constructor returned before creating the objects
*  the pointers are intended to point at.
*  3. This constructor does not register itself with the default server.
*  4. The NML object created by this constructor can not be used until
* the format_chain is constructed. (This may be done by
* derived classes. )
******************************************************************/
NML::NML(char *buf, char *proc, char *file,
    int set_to_server, int set_to_master)
{
    if (NULL == file) {
	file = default_nml_config_file;
    }
    cms_for_msg_string_conversions = 0;
    strncpy(bufname, buf, 40);
    strncpy(procname, proc, 40);
    strncpy(cfgfilename, file, 160);
    blocking_read_poll_interval = -1.0;
    info_printed = 0;
    forced_type = 0;
    already_deleted = 0;
    cms = (CMS *) NULL;
    format_chain = (LinkedList *) NULL;
    phantom_read = (NMLTYPE(*)())NULL;
    phantom_peek = (NMLTYPE(*)())NULL;
    phantom_write = (int (*)(NMLmsg *)) NULL;
    phantom_write_if_read = (int (*)(NMLmsg *)) NULL;
    phantom_check_if_read = (int (*)()) NULL;
    phantom_clear = (int (*)()) NULL;
    channel_list_id = 0;
    error_type = NML_NO_ERROR;
    ignore_format_chain = 0;
    fast_mode = 0;

    channel_type = NML_GENERIC_CHANNEL_TYPE;
    sizeof_message_header = sizeof(NMLmsg);

    if (-1 == cms_config(&cms, buf, proc, file, set_to_server, set_to_master)) {
	if (verbose_nml_error_messages) {
	    rcs_print_error("NML: cms_config returned -1.\n");
	}
	if (!info_printed) {
	    print_info(buf, proc, file);
	}
	if (NULL != cms) {
	    rcs_print_debug(PRINT_NML_DESTRUCTORS, " delete (CMS *) %X;\n",
		cms);
	    delete cms;
	    cms = (CMS *) NULL;
	}
	error_type = NML_INVALID_CONFIGURATION;
	return;
    }

    if (NULL == cms) {
	error_type = NML_INVALID_CONFIGURATION;
	if (!info_printed) {
	    print_info(buf, proc, file);
	}
	return;
    }

    if (cms->status < 0) {
	error_type = NML_INVALID_CONFIGURATION;
	if (!info_printed) {
	    print_info(buf, proc, file);
	}
	rcs_print_debug(PRINT_NML_DESTRUCTORS, " delete (CMS *) %X;\n", cms);
	delete cms;
	cms = (CMS *) NULL;
	return;
    }

    add_to_channel_list();
    if (!cms->is_phantom &&
	cms->ProcessType == CMS_LOCAL_TYPE && !cms->neutral && !cms->isserver)
    {
	fast_mode = 1;
    }
    cms_status = (int *) &(cms->status);
    cms_inbuffer_header_size = &(cms->header.in_buffer_size);

    if (NULL != cms) {
	cms->sizeof_message_header = sizeof_message_header;
	char *forced_type_eq = strstr(cms->buflineupper, "FORCE_TYPE=");
	if (forced_type_eq != NULL) {
	    long temp = strtol(forced_type_eq + 11, NULL, 0);
	    if (temp > 0) {
		forced_type = temp;
		fast_mode = 0;
	    }
	}
	char *brpi_eq = strstr(cms->buflineupper, "BRPI=");
	if (brpi_eq != NULL) {
	    blocking_read_poll_interval = strtod(brpi_eq + 5, NULL);
	}
    }
}

/******************************************************************
* Constructor for NML:
* Parameters:
* char *buffer_line - Buffer line  as written in the config file.
* char *proc_line - Process Line as expected in the config file.
* char *file - Name of the configuration file.
* int set_to_server - If 1 this NML will consider its calling process to
*   be an NML_SERVER, which effects how and when messages are encoded and
*   decoded.
* int set_to_master - Passed to the CMS constructor - how this is used
*   depends on the type of CMS buffer. In general the master is responsible
*   for creating/initializing the buffer. If set_to_master == 1 then this
*   process will be considered the master, if set_to_master == 0 then
*   the the configuration file determines if this is the master, and it
*   set_to_master == -1 then this will not be the master.
* NOTES:
*  1. Borland C++(for DOS and Windows) does not allow default
*   parameters to be specified both here and in the header file.
*  2. All pointers are first initialized to NULL so that it can be determined
*  later if the constructor returned before creating the objects
*  the pointers are intended to point at.
*  3. This constructor does not register itself with the default server.
*  4. The NML object created by this constructor can not be used until
* the format_chain is constructed. (This may be done by
* derived classes. )
******************************************************************/
NML::NML(char *buffer_line, char *proc_line)
{
    cms_for_msg_string_conversions = 0;
    cms = (CMS *) NULL;
    blocking_read_poll_interval = -1.0;
    forced_type = 0;
    info_printed = 0;
    already_deleted = 0;
    format_chain = (LinkedList *) NULL;
    phantom_read = (NMLTYPE(*)())NULL;
    phantom_peek = (NMLTYPE(*)())NULL;
    phantom_write = (int (*)(NMLmsg *)) NULL;
    phantom_write_if_read = (int (*)(NMLmsg *)) NULL;
    phantom_check_if_read = (int (*)()) NULL;
    phantom_clear = (int (*)()) NULL;
    channel_list_id = 0;
    error_type = NML_NO_ERROR;
    ignore_format_chain = 0;
    fast_mode = 0;

    channel_type = NML_GENERIC_CHANNEL_TYPE;
    sizeof_message_header = sizeof(NMLmsg);

    if (-1 == cms_create_from_lines(&cms, buffer_line, proc_line)) {
	if (verbose_nml_error_messages) {
	    rcs_print_error("NML: cms_create_from_lines returned -1.\n");
	}
	if (!info_printed) {
	    print_info();
	}
	if (NULL != cms) {
	    rcs_print_debug(PRINT_NML_DESTRUCTORS, " delete (CMS *) %X;\n",
		cms);
	    delete cms;
	    cms = (CMS *) NULL;
	}
	error_type = NML_INVALID_CONFIGURATION;
	return;
    }
    if (NULL == cms) {
	error_type = NML_INVALID_CONFIGURATION;
	return;
    }
    if (cms->status < 0) {
	error_type = NML_INVALID_CONFIGURATION;
	if (verbose_nml_error_messages) {
	    rcs_print_error("NML: cms->status = %d.\n", cms->status);
	}
	if (!info_printed) {
	    print_info();
	}
	rcs_print_debug(PRINT_NML_DESTRUCTORS, " delete (CMS *) %X;\n", cms);
	delete cms;
	cms = (CMS *) NULL;
	return;
    }
    add_to_channel_list();
    if (!cms->is_phantom &&
	cms->ProcessType == CMS_LOCAL_TYPE && !cms->neutral && !cms->isserver)
    {
	fast_mode = 1;
    }
    cms_status = (int *) &(cms->status);
    cms_inbuffer_header_size = &(cms->header.in_buffer_size);
    if (NULL != cms) {
	cms->sizeof_message_header = sizeof_message_header;
	char *forced_type_eq = strstr(cms->buflineupper, "FORCE_TYPE=");
	if (forced_type_eq != NULL) {
	    long temp = strtol(forced_type_eq + 11, NULL, 0);
	    if (temp > 0) {
		forced_type = temp;
		fast_mode = 0;
	    }
	}
	char *brpi_eq = strstr(cms->buflineupper, "BRPI=");
	if (brpi_eq != NULL) {
	    blocking_read_poll_interval = strtod(brpi_eq + 5, NULL);
	}
    }
}

/***************************************************************
* NML Member Function: add_to_channel_list()
* Purpose:
*  Adds a pointer to this NML object to the main NML list.
* The list is used by nml_cleanup to delete all NML objects owned by
* a process.
* Under VxWorks the list must be shared by all processes so we must
* remember which process (also known as a task) added it to the list.
* (This is a great reason for tossing VxWorks in the circular file!)
*****************************************************************/
void
  NML::add_to_channel_list()
{
    if (NULL == NML_Main_Channel_List) {
	NML_Main_Channel_List = new LinkedList;
    }
    if (NULL != NML_Main_Channel_List) {
	channel_list_id =
	    NML_Main_Channel_List->storeAtTail(this, sizeof(NML), 0);
    }
}

/********************************************************************
* NML Member Function: register_with_server()
* Purpose:
*  The NML_Default_Super_Server keeps a list of all of the NML objects
* that were specified as servers in the config file.
* When nml_start is called servers will be spawned for these buffers.
* The NML_Default_Super_Server also tries to reduce the number of processes
* spawned by grouping buffers with the same buffer number.
********************************************************************/
void NML::register_with_server()
{
    if (NULL != cms) {
	if (cms->spawn_server) {
	    if (NULL == NML_Default_Super_Server) {
		NML_Default_Super_Server = new NML_SUPER_SERVER;
	    }
	    NML_Default_Super_Server->add_to_list(this);
	}
    }
}

/**************************************************************
* NML Constructor:
* Parameters:
* NML *nml; - pointer to NML object to duplicate.
* int set_to_server - If 1 this NML will consider its calling process to
*   be an NML_SERVER, which effects how and when messages are encoded and
*   decoded.
* int set_to_master - Passed to the CMS constructor - how this is used
*   depends on the type of CMS buffer. In general the master is responsible
*   for creating/initializing the buffer. If set_to_master == 1 then this
*   process will be considered the master, if set_to_master == 0 then
*   the the configuration file determines if this is the master, and it
*   set_to_master == -1 then this will not be the master.
* NOTES:
*  1. Borland C++(for DOS and Windows) does not allow default
*   parameters to be specified both here and in the header file.
*  2. All pointers are first initialized to NULL so that it can be determined
*  later if the constructor returned before creating the objects
*  the pointers are intended to point at.
*************************************************************/
NML::NML(NML * nml_ptr, int set_to_server, int set_to_master)
{
    cms_for_msg_string_conversions = 0;
    already_deleted = 0;
    forced_type = 0;
    cms = (CMS *) NULL;
    format_chain = (LinkedList *) NULL;
    error_type = NML_NO_ERROR;
    ignore_format_chain = 0;
    channel_list_id = 0;
    fast_mode = 0;
    info_printed = 0;
    blocking_read_poll_interval = -1.0;

    channel_type = NML_GENERIC_CHANNEL_TYPE;
    sizeof_message_header = sizeof(NMLmsg);

    if (NULL != nml_ptr) {
	strncpy(bufname, nml_ptr->bufname, 40);
	strncpy(procname, nml_ptr->procname, 40);
	strncpy(cfgfilename, nml_ptr->cfgfilename, 160);
	if (NULL != nml_ptr->cms) {
	    /* Create a CMS channel identitical to the one from the argument
	       NML channel accept that the channel may be set_to_server or
	       set_to_master differently. */
	    cms_copy(&cms, nml_ptr->cms, set_to_server, set_to_master);
	    if (NULL != cms) {
		cms->current_subdivision = nml_ptr->cms->current_subdivision;
	    }
	}
    }
    if (!ignore_format_chain) {
	format_chain = new LinkedList;
	if ((NULL != nml_ptr->format_chain) && (NULL != format_chain)) {
	    LinkedList *from, *to;
	    NML_FORMAT_PTR format_func_ptr;
	    from = nml_ptr->format_chain;
	    to = format_chain;
	    format_func_ptr = (NML_FORMAT_PTR) from->getHead();
	    while (NULL != format_func_ptr) {
		to->storeAtTail((void *) format_func_ptr, 0, 0);
		format_func_ptr = (NML_FORMAT_PTR) from->getNext();
	    }
	}
    }
    if (NULL == cms) {
	return;
    }
    add_to_channel_list();
    if (!cms->is_phantom &&
	cms->ProcessType == CMS_LOCAL_TYPE && !cms->neutral && !cms->isserver)
    {
	fast_mode = 1;
    }
    cms_status = (int *) &(cms->status);
    cms_inbuffer_header_size = &(cms->header.in_buffer_size);
    char *forced_type_eq = strstr(cms->buflineupper, "FORCE_TYPE=");
    if (forced_type_eq != NULL) {
	long temp = strtol(forced_type_eq + 11, NULL, 0);
	if (temp > 0) {
	    forced_type = temp;
	    fast_mode = 0;
	}
    }
    char *brpi_eq = strstr(cms->buflineupper, "BRPI=");
    if (brpi_eq != NULL) {
	blocking_read_poll_interval = strtod(brpi_eq + 5, NULL);
    }
    if (NULL != nml_ptr->cms->dpi) {
	CMS_DIAG_PROC_INFO *dpi = cms->get_diag_proc_info();
	*dpi = *(nml_ptr->cms->get_diag_proc_info());
	cms->set_diag_proc_info(dpi);
    }
    cms->first_diag_store = nml_ptr->cms->first_diag_store;
    if (NULL != cms->handle_to_global_data &&
	NULL != nml_ptr->cms->handle_to_global_data) {
	cms->handle_to_global_data->total_bytes_moved =
	    nml_ptr->cms->handle_to_global_data->total_bytes_moved;
    }
}

/**************************************************************
* NML Reset Function
* Can be used instead of deleting and recreating an NML channel.
*************************************************************/
int
  NML::reset()
{
    int cms_copy_ret = 0;
    if (valid()) {
	return 1;
    }
    if (NULL != cms) {
	// Recreate a CMS channel identitical to the old one, do not
	// re-read the config file.
	CMS *oldcms = cms;
	cms = NULL;
	cms_copy_ret = cms_copy(&cms, oldcms, 0, 0);
	if (cms_copy_ret < 0) {
	    if (cms != NULL && cms != oldcms) {
		delete oldcms;
	    }
	    return 0;
	}
	register_with_server();
	add_to_channel_list();
	/* FAST MODE is a combination of options which allow certian checks
	   during a read or write operation to be avoided and therefore
	   reduce the NML/CMS overhead. */
	if (!cms->is_phantom &&
	    cms->ProcessType == CMS_LOCAL_TYPE &&
	    !cms->neutral && !cms->isserver && !cms->enable_diagnostics) {
	    fast_mode = 1;
	}

	cms_status = (int *) &(cms->status);
	cms_inbuffer_header_size = &(cms->header.in_buffer_size);
	char *forced_type_eq = strstr(cms->buflineupper, "FORCE_TYPE=");
	if (forced_type_eq != NULL) {
	    long temp = strtol(forced_type_eq + 11, NULL, 0);
	    if (temp > 0) {
		forced_type = temp;
		fast_mode = 0;
	    }
	}
	char *brpi_eq = strstr(cms->buflineupper, "BRPI=");
	if (brpi_eq != NULL) {
	    blocking_read_poll_interval = strtod(brpi_eq + 5, NULL);
	}

	delete oldcms;
    } else {
	// Re-read the config file before creating a new CMS object.
	if (cms_config(&cms, bufname, procname, cfgfilename, 0, 0) < 0) {
	    return 0;
	}
    }
    return valid();
}

/*********************************************************
* NML Destructor:
* Notes:
*  1. Use if(NULL != ???) to avoid deleting objects that were
* never constructed.
*  2. The delete channel function was added because an error occured in
* running a server unded WIN32. An exception would occur as
* the last NML channel was being deleted from within nml_cleanup.
* After two days of being unable to debug this problem I
* replaced the delete nml with nml->delete_channel() as a workaround.
* The cause of this exception is still not understood.
*********************************************************/
NML::~NML()
{
    if (already_deleted) {
	if (verbose_nml_error_messages) {
	    rcs_print_error("NML channel being deleted more than once.\n");
	}
    }
    already_deleted = 1;
    delete_channel();
}

void NML::delete_channel()
{
    rcs_print_debug(PRINT_NML_DESTRUCTORS, "deleting NML (%d)\n",
	channel_list_id);
    if (NULL != cms_for_msg_string_conversions
	&& cms != cms_for_msg_string_conversions) {
	delete cms_for_msg_string_conversions;
	cms_for_msg_string_conversions = 0;
    }
    if (NULL != cms) {
	rcs_print_debug(PRINT_NML_DESTRUCTORS, " delete (CMS *) %X;\n", cms);
	delete cms;
	cms = (CMS *) NULL;
    }
    if (NULL != format_chain) {
	delete format_chain;
	format_chain = (LinkedList *) NULL;
    }
    if (NULL != NML_Main_Channel_List && (0 != channel_list_id)) {
	NML_Main_Channel_List->deleteNode(channel_list_id);
    }
    rcs_print_debug(PRINT_NML_DESTRUCTORS, "Leaving ~NML()\n");
}

/*************************************************************
* NML Member Function: get_address()
* Purpose:
*  Returns the address of the local copy of the buffer.
*  Use this function instead of nml->cms->subdiv_data directly to prevent
*   users from pointing nml->cms->subdiv_data at something else. Or getting
*   a segmentation fault if cms was not properly constructed.
***************************************************************/
NMLmsg *NML::get_address()
{
    if (NULL == cms) {
	if (NULL != cms_for_msg_string_conversions) {
	    return ((NMLmsg *) cms_for_msg_string_conversions->subdiv_data);
	}
	error_type = NML_INVALID_CONFIGURATION;
	return ((NMLmsg *) NULL);
    } else {
	return ((NMLmsg *) cms->subdiv_data);
    }
}

/*************************************************************
* NML Member Function: valid()
* Purpose:
* Provides a check which can be used after an NML object is
* constructed to determine if everthing is in order.
* Returns: 1 if everything is O.K. 0 otherwise.
*************************************************************/
int NML::valid()
{
    if (NULL == cms) {
	error_type = NML_INVALID_CONFIGURATION;
	return (0);
    }

    if (cms->is_phantom) {
	error_type = NML_NO_ERROR;
	return (1);
    }

    if (CMS_MISC_ERROR == cms->status) {
	error_type = NML_INTERNAL_CMS_ERROR;
	return (0);
    }

    if (CMS_NO_MASTER_ERROR == cms->status) {
	error_type = NML_NO_MASTER_ERROR;
	return (0);
    }

    if (NULL == cms->data) {
	error_type = NML_INVALID_CONFIGURATION;
	return (0);
    }

    if (cms->neutral && (NULL == cms->encoded_data) && !cms->isserver) {
	error_type = NML_INVALID_CONFIGURATION;
	return (0);
    }

    if (!ignore_format_chain) {
	if (NULL == format_chain) {
	    error_type = NML_INVALID_CONFIGURATION;
	    return (0);
	}
    }

    error_type = NML_NO_ERROR;
    return (1);
}

/***********************************************************
* NML Member Function: read()
* Purpose: Reads an NMLmsg from a CMS buffer.
* Returns:
*  0 The read was successful but the data was not updated since the last read.
*  -1 The buffer could not be read.
*  o.w. The type of the new NMLmsg is returned.
* Notes:
*   1. Users need to call NML::get_address in order to access the
* messages that were read.
*  2. Some buffers can be identified as PHANTOM in the config file.
* this will cause cms->is_phantom to be set. This will cause this
* function to call the function pointed to by phantom_read
* if it is not NULL rather than using CMS.
*  3. If an error occurs, users can check error_type before
* making any other NML calls
***********************************************************/
NMLTYPE NML::read()
{
    error_type = NML_NO_ERROR;
    if (fast_mode) {
	cms->read();
	switch (cms->status) {
	case CMS_READ_OLD:
	    return (0);
	case CMS_READ_OK:
	    if (((NMLmsg *) cms->subdiv_data)->type <= 0 && !cms->isserver) {
		rcs_print_error
		    ("NML: New data recieved but type of %d is invalid.\n",
		    ((NMLmsg *) cms->subdiv_data)->type);
		return -1;
	    }
	    return (((NMLmsg *) cms->subdiv_data)->type);

	default:
	    set_error();
	    return -1;
	}
    }
    /* Check pointers. */
    if (NULL == cms) {
	if (error_type != NML_INVALID_CONFIGURATION) {
	    error_type = NML_INVALID_CONFIGURATION;
	    rcs_print_error("NML::read: CMS not configured.\n");
	}
	return (-1);
    }

    /* Handle PHANTOMs */
    if (cms->is_phantom) {
	if (NULL != phantom_read) {
	    return ((*phantom_read) ());
	} else {
	    return (0);
	}
    }

    /* Read using CMS */
    if (!cms->force_raw) {
	cms->set_mode(CMS_READ);
    }
    cms->read();

    if (!cms->force_raw) {
	if (cms->status == CMS_READ_OK) {
	    if (-1 == format_output()) {
		error_type = NML_FORMAT_ERROR;
		return (-1);
	    }
	}
    }

    /* Choose the return value. */
    switch (cms->status) {
    case CMS_READ_OLD:
	error_type = NML_NO_ERROR;
	return (0);
    case CMS_READ_OK:
	error_type = NML_NO_ERROR;
	if (((NMLmsg *) cms->subdiv_data)->type <= 0 && !cms->isserver) {
	    rcs_print_error
		("NML: New data recieved but type of %d is invalid.\n",
		((NMLmsg *) cms->subdiv_data)->type);
	    return -1;
	}
	return (((NMLmsg *) cms->subdiv_data)->type);

    default:
	set_error();
	return -1;
    }
}

/***********************************************************
* NML Member Function: blocking_read(double timeout)
* Purpose: Reads an NMLmsg from a CMS buffer after waiting up to timeout seconds for new data.
* Returns:
*  0 The read was successful but the data was not updated since the last read.
*  -1 The buffer could not be read.
*  o.w. The type of the new NMLmsg is returned.
* Notes:
*   1. Users need to call NML::get_address in order to access the
* messages that were read.
*  2. Some buffers can be identified as PHANTOM in the config file.
* this will cause cms->is_phantom to be set. This will cause this
* function to call the function pointed to by phantom_read
* if it is not NULL rather than using CMS.
*  3. If an error occurs, users can check error_type before
* making any other NML calls
***********************************************************/
NMLTYPE NML::blocking_read(double blocking_timeout)
{
    error_type = NML_NO_ERROR;
    if (fast_mode) {
	cms->blocking_read(blocking_timeout);
	switch (cms->status) {
	case CMS_READ_OLD:
	    return (0);
	case CMS_READ_OK:
	    if (((NMLmsg *) cms->subdiv_data)->type <= 0 && !cms->isserver) {
		rcs_print_error
		    ("NML: New data recieved but type of %d is invalid.\n",
		    ((NMLmsg *) cms->subdiv_data)->type);
		return -1;
	    }
	    return (((NMLmsg *) cms->subdiv_data)->type);
	case CMS_TIMED_OUT:
	    error_type = NML_NO_ERROR;
	    return 0;

	default:
	    set_error();
	    return (-1);
	}
    }
    /* Check pointers. */
    if (NULL == cms) {
	if (error_type != NML_INVALID_CONFIGURATION) {
	    error_type = NML_INVALID_CONFIGURATION;
	    rcs_print_error("NML::blocking_read: CMS not configured.\n");
	}
	return (-1);
    }

    /* Handle PHANTOMs */
    if (cms->is_phantom) {
	if (NULL != phantom_read) {
	    return ((*phantom_read) ());
	} else {
	    return (0);
	}
    }

    /* Read using CMS */
    if (!cms->force_raw) {
	cms->set_mode(CMS_READ);
    }
    if (cms->BufferType == CMS_SHMEM_TYPE) {
	cms->blocking_read(blocking_timeout);
    } else {
	double time_elapsed = 0.0;
	double start_time = 0.0;
	if (blocking_timeout > 0.0) {
	    start_time = etime();
	}
	double current_brpi = blocking_read_poll_interval;
	cms->status = CMS_READ_OLD;
	if (current_brpi < 2e-2) {
	    current_brpi = 2e-2;
	}
	if (current_brpi > blocking_timeout / 2.0 && blocking_timeout > 1e-6) {
	    current_brpi = blocking_timeout / 2.0;
	}
	while (cms->status == CMS_READ_OLD &&
	    (time_elapsed < blocking_timeout || blocking_timeout < 0.0)) {

	    esleep(current_brpi);
	    cms->read();
	    if (blocking_timeout > 0.0 && cms->status == CMS_READ_OLD) {
		time_elapsed = etime() - start_time;
	    }
	    if (time_elapsed < 0.0) {
		break;
	    }
	}
    }

    if (!cms->force_raw) {
	if (cms->status == CMS_READ_OK) {
	    if (-1 == format_output()) {
		error_type = NML_FORMAT_ERROR;
		return (-1);
	    }
	}
    }

    /* Choose the return value. */
    switch (cms->status) {
    case CMS_READ_OLD:
	return (0);
    case CMS_READ_OK:
	if (((NMLmsg *) cms->subdiv_data)->type <= 0 && !cms->isserver) {
	    rcs_print_error
		("NML: New data recieved but type of %d is invalid.\n",
		((NMLmsg *) cms->subdiv_data)->type);
	    return -1;
	}
	return (((NMLmsg *) cms->subdiv_data)->type);
    case CMS_TIMED_OUT:
	error_type = NML_NO_ERROR;
	return 0;

    default:
	set_error();
	return (-1);
    }

}

/* Same as the read with no arguments except that the data is
 stored in a user supplied location . */
NMLTYPE NML::read(void *temp_data, long temp_size)
{
    NMLTYPE return_value = 0;
    void *original_data;
    long original_size = cms->size;
    long original_max_message_size = cms->max_message_size;
    original_data = cms->data;
    cms->data = temp_data;
    cms->size = temp_size;
    if (cms->max_message_size > ((long) temp_size)) {
	cms->max_message_size = temp_size;
    }
    return_value = peek();
    cms->data = original_data;
    cms->size = original_size;
    cms->max_message_size = original_max_message_size;
    return return_value;
}

/* Same as the peek with no arguments except that the data is
 stored in a user supplied location . */
NMLTYPE NML::peek(void *temp_data, long temp_size)
{
    NMLTYPE return_value = 0;
    void *original_data;
    long original_size = cms->size;
    long original_max_message_size = cms->max_message_size;
    original_data = cms->data;
    cms->data = temp_data;
    cms->size = temp_size;
    if (cms->max_message_size > ((long) temp_size)) {
	cms->max_message_size = temp_size;
    }
    return_value = peek();
    cms->data = original_data;
    cms->size = original_size;
    cms->max_message_size = original_max_message_size;
    return return_value;
}

/***********************************************************
* NML Member Function: peek()
* Purpose: Reads an NMLmsg from a CMS buffer without setting the
* was_read flag. The was_read flag is used by check_if_read and
* write_if_read.
* Returns:
*  0 The read was successful but the data was not updated since the last read.
*  -1 The buffer could not be read.
*  o.w. The type of the new NMLmsg is returned.
* Notes:
*   1. Users need to call NML::get_address in order to access the
* messages that were read.
*  2. Some buffers can be identified as PHANTOM in the config file.
* this will cause cms->is_phantom to be set. This will cause this
* function to call the function pointed to by phantom_read
* if it is not NULL rather than using CMS.
*  3. If an error occurs, users can check error_type before
* making any other NML calls
***********************************************************/
NMLTYPE NML::peek()
{
    error_type = NML_NO_ERROR;
    if (fast_mode) {
	cms->peek();
	switch (cms->status) {
	case CMS_READ_OLD:
	    return (0);
	case CMS_READ_OK:
	    if (((NMLmsg *) cms->subdiv_data)->type <= 0 && !cms->isserver) {
		rcs_print_error
		    ("NML: New data recieved but type of %d is invalid.\n",
		    ((NMLmsg *) cms->subdiv_data)->type);
		return -1;
	    }
	    return (((NMLmsg *) cms->subdiv_data)->type);

	default:
	    set_error();
	    return -1;
	}
    }
    if (NULL == cms) {
	if (error_type != NML_INVALID_CONFIGURATION) {
	    error_type = NML_INVALID_CONFIGURATION;
	    rcs_print_error("NML::peek: CMS not configured.\n");
	}
	return (-1);
    }

    if (cms->is_phantom) {
	if (NULL != phantom_peek) {
	    return ((*phantom_peek) ());
	} else {
	    return (0);
	}
    }
    if (!cms->force_raw) {
	cms->set_mode(CMS_READ);
    }

    cms->peek();
    if (!cms->force_raw) {
	if (cms->status == CMS_READ_OK) {
	    if (-1 == format_output()) {
		error_type = NML_FORMAT_ERROR;
		return (-1);
	    }
	}
    }

    switch (cms->status) {
    case CMS_READ_OLD:
	return (0);
    case CMS_READ_OK:
	if (((NMLmsg *) cms->subdiv_data)->type <= 0 && !cms->isserver) {
	    rcs_print_error
		("NML: New data recieved but type of %d is invalid.\n",
		((NMLmsg *) cms->subdiv_data)->type);
	    return -1;
	}
	return (((NMLmsg *) cms->subdiv_data)->type);

    default:
	set_error();
	return -1;
    }

}

/***********************************************************
* NML Member Function: format_output()
* Purpose: Formats the data read from a CMS buffer as required
* by the process that created this NML. The formatting converts
* messages from some platform indepent format to a platform
* specific format or vice versa. (Performing byte-swapping etc.)
* Returns:
*  0  The format was successful.
*  -1 An error occured.
* Notes:
*  1. There are 3 conditions under which format_output may be
* called.
*     i. The data is being read out as is. (cms->mode == CMS_RAW_OUT).
*    ii. A user needs the data in the local platform-specific or raw format
*        but the buffer has been encoded in a platform-independant or
*         neutral format.   (cms->mode == CMS_DECODE).
*   iii. An NML_SERVER needs the data encoded in a platform-independant
*        or neutral format to send it out over the network but the buffer
*        is in a local platform-specific or raw format.
*         (cms->mode == CMS_ENCODE)
*  2. This function uses a list of format functions supplied
* by the user and stored int the format_chain.
* Returns:
* 0 = Success.
* -1 = Error.
***********************************************************/
int NML::format_output()
{
    NMLTYPE new_type;
    long new_size;

    /* Check pointers */
    if (NULL == cms) {
	rcs_print_error("NML: cms is NULL.\n");
	return (-1);
    }

    if (cms->force_raw) {
	return 0;
    }

    if (forced_type > 0) {
	new_type = forced_type;
    }

    switch (cms->mode) {
    case CMS_RAW_OUT:
	break;
    case CMS_DECODE:
	/* Check the status of CMS. */
	if (cms->status == CMS_READ_OK) {
	    /* Handle the generic part of the message. */
	    cms->format_low_ptr = cms->format_high_ptr = (char *) NULL;
	    cms->rewind();	/* Move to the start of encoded buffer. */
	    cms->update(new_type);	/* Get the message type from encoded
					   buffer. */
	    cms->update(new_size);	/* Get the message size from encoded
					   buffer. */
	    if (forced_type > 0) {
		new_type = forced_type;
	    }
	    ((NMLmsg *) cms->subdiv_data)->type = new_type;	/* Store type 
								   in
								   message. */
	    ((NMLmsg *) cms->subdiv_data)->size = new_size;	/* Store size 
								   in
								   message. */

	    if (new_size > cms->max_message_size) {
		rcs_print_error("NML: Message %ld of size  %ld \n", new_type,
		    new_size);
		rcs_print_error
		    ("     too large for local buffer of %s of size %d.\n",
		    cms->BufferName, cms->max_message_size);
		if (verbose_nml_error_messages) {
		    rcs_print_error
			("Check that all processes agree on buffer size.\n");
		}
		cms->status = CMS_INSUFFICIENT_SPACE_ERROR;
		return (-1);
	    }

	    /* Check the list of format functions. */
	    if (!ignore_format_chain) {
		cms->format_low_ptr = (char *) cms->subdiv_data;
		cms->format_high_ptr = cms->format_low_ptr + cms->size;
		if (NULL == format_chain) {
		    rcs_print_error("NML::read: Format chain is NULL.\n");
		    return (-1);
		}
		/* Run through the list of format functions. */
		if (-1 == run_format_chain(new_type, cms->subdiv_data)) {
		    rcs_print_error("NMLread: NMLformat error\n");
		    if (verbose_nml_error_messages) {
			rcs_print_error("   (Buffer = %s, Process = %s)\n",
			    cms->BufferName, cms->ProcessName);
		    }
		    return (-1);
		}
	    }
	}
	break;
    case CMS_ENCODE:
	/* Check the status of CMS. */
	if (cms->status != CMS_MISC_ERROR) {
	    cms->format_low_ptr = cms->format_high_ptr = (char *) NULL;
	    cms->rewind();	/* Move to the start of the encoded buffer. */

	    /* Get the type and size from the message. */
	    new_type = ((NMLmsg *) cms->subdiv_data)->type;
	    new_size = ((NMLmsg *) cms->subdiv_data)->size;

	    if (forced_type > 0) {
		new_type = forced_type;
		((NMLmsg *) cms->subdiv_data)->type = forced_type;
	    }

	    /* Store the type and size in the encoded buffer. */
	    cms->update(new_type);
	    cms->update(new_size);

	    if (new_size > cms->max_message_size) {
		rcs_print_error("NML: Message %ld of size  %ld\n", new_type,
		    new_size);
		rcs_print_error
		    ("     too large for local buffer of %s of size %d.\n",
		    cms->BufferName, cms->max_message_size);
		if (verbose_nml_error_messages) {
		    rcs_print_error
			("Check that all processes agree on buffer size.\n");
		}
		cms->status = CMS_INSUFFICIENT_SPACE_ERROR;
		return (-1);
	    }

	    /* Check the list of format functions. */
	    if (!ignore_format_chain) {
		cms->format_low_ptr = (char *) cms->subdiv_data;
		cms->format_high_ptr = cms->format_low_ptr + cms->size;
		if (NULL == format_chain) {
		    rcs_print_error("NML::read: Format chain is NULL.\n");
		    return (-1);
		}

		/* Run through the list of format functions. */
		if (-1 == run_format_chain(new_type, cms->subdiv_data)) {
		    rcs_print_error("NMLread: NMLformat error\n");
		    if (verbose_nml_error_messages) {
			rcs_print_error("   (Buffer = %s, Process = %s)\n",
			    cms->BufferName, cms->ProcessName);
		    }
		    return (-1);
		}

		/* Get the new size of the message now that it's been
		   encoded. */
		cms->get_encoded_msg_size();
	    }
	}
	break;
    default:
	rcs_print_error("NML::format_output: invalid format mode. (%d)\n",
	    cms->mode);
	return (-1);
    }
    if (forced_type > 0) {
	new_type = forced_type;
	((NMLmsg *) cms->subdiv_data)->type = forced_type;
    }

    return (((int) cms->status < 0) ? -1 : 0);
}

/*************************************************************
* NML Member Function: write()
* Purpose: This write function provides users with an alternative
* style of writing a message.
* Parameters:
* NMLmsg &nml_msg - Reference to the message to be written.
* Returns:
*  0 - The message was successfully written.
*  -1 - An error occured. (Timeouts are considered errors.)
*************************************************************/
int NML::write(NMLmsg & nml_msg)
{
    return (write(&nml_msg));	/* Call the other NML::write() */
}

/*************************************************************
* NML Member Function: write()
* Purpose: Writes a message to the global buffer.
* Parameters:
* NMLmsg *nml_msg - Address of the message to be written.
* Returns:
*  0 - The message was successfully written.
*  -1 - An error occured. (Timeouts are considered errors.)
*************************************************************/
int NML::write(NMLmsg * nml_msg)
{
    error_type = NML_NO_ERROR;
    if (fast_mode) {
	*cms_inbuffer_header_size = nml_msg->size;
	cms->write(nml_msg);
	if (*cms_status == CMS_WRITE_OK) {
	    return (0);
	}
	set_error();
	return (-1);
    }
    /* Check pointers. */
    if (NULL == cms) {
	if (error_type != NML_INVALID_CONFIGURATION) {
	    error_type = NML_INVALID_CONFIGURATION;
	    rcs_print_error("NML::write: CMS not configured.\n");
	}
	return (-1);
    }

    if (NULL == nml_msg) {
	error_type = NML_INVALID_MESSAGE_ERROR;
	rcs_print_error("NML::write: Message is NULL.\n");
	return (-1);
    }

    if ((nml_msg->size == 0 || nml_msg->type == 0) && !cms->isserver) {
	error_type = NML_INVALID_MESSAGE_ERROR;
	rcs_print_error("NML::write: Message size or type is zero.\n");
	rcs_print_error
	    ("NML: Check that the message was properly constructed.\n");
    }

    /* Handle Phantom Buffers. */
    if (cms->is_phantom) {
	if (NULL != phantom_write) {
	    return ((*phantom_write) (nml_msg));
	} else {
	    return (0);
	}
    }

    /* Set CMS to a write mode. */
    cms->set_mode(CMS_WRITE);

    /* Format the message if neccessary. */
    if (-1 == format_input(nml_msg)) {
	error_type = NML_FORMAT_ERROR;
	return -1;
    }

    if (CMS_RAW_IN == cms->mode) {
	cms->write(nml_msg);	/* Write the unformatted message.  */
    } else {
	cms->write(cms->subdiv_data);	/* Write the formatted message.  */
    }

    if (CMS_WRITE_OK == cms->status) {
	error_type = NML_NO_ERROR;
	return (0);
    }

    return set_error();
}

/*************************************************************
* NML Member Function: set_error
* Purpose: This write function provides users with an alternative
* style of writing a message.
* Parameters:
* NMLmsg &nml_msg - Reference to the message to be written.
* Returns:
*  0 - The message was successfully written.
*  -1 - An error occured. (Timeouts are considered errors.)
* Check error_type for more info.
*************************************************************/
int NML::set_error()
{
    if (error_type != NML_NO_ERROR) {
	return -1;
    }

    if (NULL == cms) {
	error_type = NML_INVALID_CONFIGURATION;
	return 0;
    }

    /* Choose return value. */
    switch (cms->status) {
    case CMS_TIMED_OUT:
	error_type = NML_TIMED_OUT;
	return (-1);

    case CMS_QUEUE_FULL:
	error_type = NML_QUEUE_FULL_ERROR;
	break;

    case CMS_NO_MASTER_ERROR:
	error_type = NML_NO_MASTER_ERROR;
	break;

    case CMS_WRITE_WAS_BLOCKED:
	error_type = NML_BUFFER_NOT_READ;
	break;

    case CMS_STATUS_NOT_SET:	/* The status variable has not been set yet. */
    case CMS_READ_OLD:		/* Read successful, but data is old. */
    case CMS_READ_OK:		/* Read successful so far. */
    case CMS_WRITE_OK:		/* Write successful so far. */
    case CMS_CLEAR_OK:		/* A clear operation was successful.  */
	error_type = NML_NO_ERROR;
	break;

    case CMS_RESOURCE_CONFLICT_ERROR:
    case CMS_CREATE_ERROR:
    case CMS_CONFIG_ERROR:
	error_type = NML_INVALID_CONFIGURATION;
	break;

    case CMS_MISC_ERROR:
    default:
	error_type = NML_INTERNAL_CMS_ERROR;
	break;

    }

    if (error_type == NML_NO_ERROR) {
	return 0;
    }
    if (!info_printed) {
	print_info();
    }

    return -1;
}

/*************************************************************
* NML Member Function: write_if_read()
* Purpose: This write function provides users with an alternative
* style of writing a message.
* Parameters:
* NMLmsg &nml_msg - Reference to the message to be written.
* Returns:
*  0 - The message was successfully written.
*  -1 - An error occured. (Timeouts are considered errors.)
* Check error_type for more info.
*************************************************************/
int NML::write_if_read(NMLmsg & nml_msg)
{
    return (write_if_read(&nml_msg));
}

/***********************************************************
* NML Member Function: write_if_read()
* Purpose: Write a message to the global buffer, but do not
*  over-write another message if that message is unread.
* Parameters:
*  NMLmsg *nml_msg - Address of the message to be written.
* Returns:
*  0 - The message was successfully written.
*  -1 - An error occured.
* (Timeouts, and unread buffers  are considered errors.)
* Check error_type for more info.
************************************************************/
int NML::write_if_read(NMLmsg * nml_msg)
{
    error_type = NML_NO_ERROR;
    if (fast_mode) {
	cms->header.in_buffer_size = nml_msg->size;
	cms->write(nml_msg);
	if (cms->status == CMS_WRITE_OK) {
	    return (0);
	}
	set_error();
	return (-1);
    }
    if (NULL == cms) {
	if (error_type != NML_INVALID_CONFIGURATION) {
	    error_type = NML_INVALID_CONFIGURATION;
	    rcs_print_error("NML::write_if_read: CMS not configured.\n");
	}
	return (-1);
    }

    if (NULL == nml_msg) {
	error_type = NML_INVALID_MESSAGE_ERROR;
	rcs_print_error("NML::write_if_read: Message is NULL.\n");
	return (-1);
    }

    if ((nml_msg->size == 0 || nml_msg->type == 0) && !cms->isserver) {
	error_type = NML_INVALID_MESSAGE_ERROR;
	rcs_print_error
	    ("NML::write_if_read: Message size or type is zero.\n");
	if (verbose_nml_error_messages) {
	    rcs_print_error
		("NML: Check that the message was properly constructed.\n");
	}
    }

    if (cms->is_phantom) {
	if (NULL != phantom_write_if_read) {
	    return ((*phantom_write_if_read) (nml_msg));
	} else {
	    return (0);
	}
    }

    cms->set_mode(CMS_WRITE);
    if (-1 == format_input(nml_msg)) {
	error_type = NML_FORMAT_ERROR;
	return -1;
    }

    if (CMS_RAW_IN == cms->mode) {
	cms->write_if_read(nml_msg);
    } else {
	cms->write_if_read(cms->subdiv_data);
    }

    return (set_error());
}

/*******************************************************************
* NML Member Function: format_input()
* Purpose: Formats the in an NML message to be writen to a CMS buffer
*  as required by the configuration file. The formatting converts
*  messages from some platform indepent format to a platform
*  specific format or vice versa. (Performing byte-swapping etc.)
* Parameters:
* NMLmsg *nml_msg - The address of the NML message.
* Returns:
* 0 = Success.
* -1 = Error.
* Notes:
*  1. There are 3 conditions that format_input may be called under.
*    i. The message will be written to the buffer without any formatting.
*       (cms->mode == CMS_RAW_IN)
*    ii. The message is in a native or raw format and needs to be encoded
*  in a neutral format before being sent over the network or into a
*  neutral buffer.
*        (cms->mode == CMS_ENCODE)
*   iii. The process calling this is a server which recieved a neutrally
* encoded buffer over the network which must be converted to native
* format before being written into a raw buffer.
*        (cms->mode == CMS_DECODE)
*  2. This function is for internal NML use only.
******************************************************************/
int NML::format_input(NMLmsg * nml_msg)
{
    NMLTYPE new_type;
    long new_size;
    if (NULL == cms) {
	return -1;
    }

    if (cms->force_raw) {
	cms->mode = CMS_RAW_IN;
    }

    switch (cms->mode) {
    case CMS_RAW_IN:
	/* Make sure the message size is not larger than the buffer size. */
	if (nml_msg->size > cms->max_message_size) {
	    rcs_print_error("NML: Message size(%d) too large for"
		" CMS buffer size of %d.\n",
		nml_msg->size, cms->max_message_size);
	    cms->status = CMS_INSUFFICIENT_SPACE_ERROR;
	    return (-1);
	}
	cms->header.in_buffer_size = nml_msg->size;
	break;
    case CMS_ENCODE:
	/* Make sure the message size is not larger than the buffer size. */
	if (nml_msg->size > cms->max_message_size) {
	    rcs_print_error("NML: Message size(%d) too large for"
		" CMS buffer size of %d.\n",
		nml_msg->size, cms->max_message_size);
	    cms->status = CMS_INSUFFICIENT_SPACE_ERROR;
	    return (-1);
	}

	cms->format_low_ptr = (char *) nml_msg;
	cms->format_high_ptr = cms->format_low_ptr + nml_msg->size;
	/* Handle the generic part of the message. */
	cms->rewind();		/* Move to the start of the encoded buffer. */
	cms->update(nml_msg->type);	/* Store message type in encoded
					   buffer. */
	cms->update(nml_msg->size);	/* Store message size in encoded
					   buffer. */

	/* Check list of format functions. */
	if (!ignore_format_chain) {
	    if (NULL == format_chain) {
		rcs_print_error("NML::read: Format chain is NULL.\n");
		return (-1);
	    }

	    /* Run through list of format functions. */
	    if (-1 == run_format_chain(nml_msg->type, nml_msg)) {
		rcs_print_error("NMLwrite: format error\n");
		if (verbose_nml_error_messages) {
		    rcs_print_error("   (Buffer = %s, Process = %s)\n",
			cms->BufferName, cms->ProcessName);
		}
		return (-1);
	    }
	}
	/* Determine the new size of the message now that its encoded. */
	cms->header.in_buffer_size = cms->get_encoded_msg_size();
	break;
    case CMS_DECODE:
	cms->format_low_ptr = cms->format_high_ptr = (char *) NULL;
	cms->rewind();		/* Move to the start of the encoded buffer. */
	cms->update(new_type);	/* Get message type from encoded buffer. */
	cms->update(new_size);	/* Get message size from encoded buffer. */

	/* Make sure the message size is not larger than the buffer size. */
	if (new_size > cms->max_message_size) {
	    rcs_print_error("NMLwrite: Message size(%d) too large for"
		" CMS buffer size of %d.\n", new_size, cms->max_message_size);
	    cms->status = CMS_INSUFFICIENT_SPACE_ERROR;
	    return (-1);
	}
	cms->format_low_ptr = (char *) cms->subdiv_data;
	cms->format_high_ptr = cms->format_low_ptr + cms->size;

	/* Store the new type and size in the raw message. */
	((NMLmsg *) cms->subdiv_data)->type = new_type;
	((NMLmsg *) cms->subdiv_data)->size = new_size;

	/* Check the list of format functions. */
	if (!ignore_format_chain) {
	    if (NULL == format_chain) {
		rcs_print_error("NML::read: Format chain is NULL.\n");
		return (-1);
	    }

	    /* Run through the list of format functions. */
	    if (-1 == run_format_chain(new_type, cms->subdiv_data)) {
		rcs_print_error("NMLwrite: format error\n");
		rcs_print_error("   (Buffer = %s, Process = %s)\n",
		    cms->BufferName, cms->ProcessName);
		return (-1);
	    }
	}
	/* Choose a size that will ensure the entire message will be read
	   out. */
	if (cms->format_size < ((long) sizeof(NMLmsg))) {
	    cms->format_size = sizeof(NMLmsg);
	}
	if (cms->format_size > new_size) {
	    ((NMLmsg *) cms->subdiv_data)->size = (long) cms->format_size;
	}
	cms->header.in_buffer_size = ((NMLmsg *) cms->subdiv_data)->size;
	break;
    default:
	rcs_print_error("NML::format_input: invalid mode (%d).\n", cms->mode);
	return (-1);
    }

    return (((int) cms->status < 0) ? -1 : 0);
}

int NML::run_format_chain(NMLTYPE type, void *buf)
{
    NML_FORMAT_PTR format_function;

    format_function = (NML_FORMAT_PTR) format_chain->getHead();
    while (NULL != format_function) {
	switch ((*format_function) (type, buf, cms)) {
	case -1:
	    return (-1);
	case 0:
	    break;
	case 1:
	    return (0);
	}
	format_function = (NML_FORMAT_PTR) format_chain->getNext();
    }
    return (0);
}

int NML::prefix_format_chain(NML_FORMAT_PTR f_ptr)
{
    if (NULL == format_chain) {
	format_chain = new LinkedList;
    }
    if (NULL != format_chain) {
	format_chain->storeAtHead((void *) f_ptr, 0, 0);
    }
    return (0);
}

/**************************************************************************
* NML member function: msg2str
* Parameter: NMLmsg &msg -- Reference to message to be converted into a string.
* Returns: Returns a pointer to the cms->encoded_data buffer if successful
* since this should contain the string or NULL if there was an error.
***************************************************************************/
const char *NML::msg2str(NMLmsg & msg)
{
    return msg2str(&msg);
}

/**************************************************************************
* NML member function: msg2str
* Parameter: NMLmsg *msg -- Pointer to message to be converted into a string.
* Returns: Returns a pointer to the cms->encoded_data buffer if successful
* since this should contain the string or NULL if there was an error.
***************************************************************************/
const char *NML::msg2str(NMLmsg * nml_msg)
{
    CMS *orig_cms = cms;
    char *str = NULL;
    if (NULL == nml_msg) {
	return NULL;
    }
    if (NULL == cms) {
	int msg_length = nml_msg->size;
	if (NULL != cms_for_msg_string_conversions) {
	    if ((cms_for_msg_string_conversions->size > 16 * msg_length &&
		    cms_for_msg_string_conversions->size > 2048) ||
		cms_for_msg_string_conversions->size < 4 * msg_length) {
		delete cms_for_msg_string_conversions;
		cms_for_msg_string_conversions = 0;
	    }
	}
	if (NULL == cms_for_msg_string_conversions) {
	    cms_for_msg_string_conversions =
		new CMS(nml_msg->size * 4 + 16 + (16 - (nml_msg->size % 16)));
	}
	cms = cms_for_msg_string_conversions;
    }
    cms->set_temp_updater(CMS_DISPLAY_ASCII_ENCODING);
    cms->set_mode(CMS_ENCODE);
    if (-1 == format_input(nml_msg)) {
	cms->restore_normal_updater();
	error_type = NML_FORMAT_ERROR;
	cms = orig_cms;
	return ((char *) NULL);
    }
    cms->restore_normal_updater();
    str = (char *) cms->encoded_data;
    cms = orig_cms;
    return (const char *) str;
}

static int info_message_printed = 0;
char cwd_buf[256];
char host_name_buf[MAXHOSTNAMELEN];
char last_bufname[10];
char last_procname[10];
char last_cfg_file[40];

/**************************************************************************
* NML member function: print_info()
* Prints the buffer, process names and configuration file information.
***************************************************************************/
void NML::print_info(char *bufname, char *procname, char *cfg_file)
{
    info_printed = 1;
    if (!verbose_nml_error_messages) {
	return;
    }
    if (NULL == cms || error_type != NML_NO_ERROR) {
	if (max_rcs_errors_to_print <= rcs_errors_printed &&
	    max_rcs_errors_to_print >= 0) {
	    return;
	}
    }
    if (error_type == NML_QUEUE_FULL_ERROR && !cms_print_queue_full_messages) {
	return;
    }
    if (NULL != cms) {
	if (cms->status < 0) {
	    if (max_rcs_errors_to_print <= rcs_errors_printed &&
		max_rcs_errors_to_print >= 0) {
		return;
	    }
	}
    }
    if (NULL != bufname && NULL != procname && NULL != cfg_file) {
	if (!strncmp(bufname, last_bufname, 10)
	    && !strncmp(procname, last_procname, 10)
	    && !strncmp(cfg_file, last_cfg_file, 40)) {
	    return;
	}
	strncpy(last_bufname, bufname, 10);
	strncpy(last_procname, procname, 10);
	strncpy(last_cfg_file, cfg_file, 40);
    }
    if (!info_message_printed) {
	rcs_print
	    ("\n**********************************************************\n");
	rcs_print("* Current Directory = %s\n", getcwd(cwd_buf, 256));
	if (nml_print_hostname_on_error) {
#if 0
	    dl_gethostname(host_name_buf, MAXHOSTNAMELEN);
	    if (host_name_buf[0] != 0) {
		rcs_print("* Host = %s\n", host_name_buf);
	    }
#endif
	}
	rcs_print("* ");
	info_message_printed = 1;
    }
    rcs_print
	("\n**********************************************************\n");
    if (NULL != cms) {
	rcs_print("* BufferName = %s\n", cms->BufferName);
	rcs_print("* BufferType = %d\n", cms->BufferType);
	rcs_print("* ProcessName = %s\n", cms->ProcessName);
	rcs_print("* Configuration File = %s\n", cfgfilename);
	rcs_print("* CMS Status = %d (%s)\n", cms->status,
	    cms->status_string(cms->status));
	rcs_print("* Recent errors repeated:\n");
	rcs_print("%s\n", last_error_bufs[0]);
	rcs_print("%s\n", last_error_bufs[1]);
	rcs_print("%s\n", last_error_bufs[2]);
	rcs_print("%s\n", last_error_bufs[3]);
	memset(last_error_bufs[0], 0, 100);
	memset(last_error_bufs[1], 0, 100);
	memset(last_error_bufs[2], 0, 100);
	memset(last_error_bufs[3], 0, 100);
	if (NULL == strstr(cms->BufferLine, "\n")) {
	    rcs_print("* BufferLine: %s\n", cms->BufferLine);

	} else {
	    rcs_print("* BufferLine: %s", cms->BufferLine);
	}
	if (NULL == strstr(cms->ProcessLine, "\n")) {
	    rcs_print("* ProcessLine: %s\n", cms->ProcessLine);
	} else {
	    rcs_print("* ProcessLine: %s", cms->ProcessLine);
	}
    } else {
	if (NULL != bufname) {
	    rcs_print("* BufferName = %s\n", bufname);
	}
	if (NULL != procname) {
	    rcs_print("* ProcessName = %s\n", procname);
	}
    }
    if (NULL != cfg_file) {
	rcs_print("* Config File = %s\n", cfg_file);
    }
    rcs_print("* error_type = %d (%s)\n", error_type,
	NML_ERROR_TYPE_STRINGS[error_type]);
    rcs_print
	("************************************************************\n\n");
}

void nml_start()
{

    spawn_nml_servers();
}

void nml_cleanup()
{
    NML *nml;
    nml_server_cleanup();
    if (NULL != NML_Main_Channel_List) {
	rcs_print_debug(PRINT_NML_DESTRUCTORS,
	    "Deleting %d channels from the NML_Main_Channel_List.\n",
	    NML_Main_Channel_List->listSize);
	nml = (NML *) NML_Main_Channel_List->getHead();
	while (NULL != nml) {
	    if (nml->cms != NULL) {
		rcs_print_debug(PRINT_NML_DESTRUCTORS,
		    "Deleting %s NML channel from NML_Main_Channel_List.\n",
		    nml->cms->BufferName);
	    }
	    nml->delete_channel();

	    rcs_print_debug(PRINT_NML_DESTRUCTORS,
		"NML channel deleted from NML_Main_Channel_List\n");
	    if (NULL == NML_Main_Channel_List) {
		return;
	    }
	    NML_Main_Channel_List->deleteCurrentNode();
	    nml = (NML *) NML_Main_Channel_List->getNext();
	}
	if (NULL != NML_Main_Channel_List) {
	    delete NML_Main_Channel_List;
	    NML_Main_Channel_List = (LinkedList *) NULL;
	}
    }
    if (NULL != Dynamically_Allocated_NML_Objects) {
	nml = (NML *) Dynamically_Allocated_NML_Objects->getHead();
	while (NULL != nml) {
	    if (nml->cms != NULL) {
		rcs_print_debug(PRINT_NML_DESTRUCTORS,
		    "Deleting %s NML channel from Dynamically_Allocated_NML_Objects.\n",
		    nml->cms->BufferName);
	    }
	    delete nml;

	    rcs_print_debug(PRINT_NML_DESTRUCTORS,
		"NML channel deleted from Dynamically_Allocated_NML_Objects\n");
	    if (NULL == Dynamically_Allocated_NML_Objects) {
		return;
	    }
	    Dynamically_Allocated_NML_Objects->deleteCurrentNode();
	    nml = (NML *) Dynamically_Allocated_NML_Objects->getNext();
	}
	if (NULL != Dynamically_Allocated_NML_Objects) {
	    delete Dynamically_Allocated_NML_Objects;
	    Dynamically_Allocated_NML_Objects = (LinkedList *) NULL;
	}
    }
    nmlClearHostAliases();
}

void nml_wipeout_lists()
{
    if (NULL != NML_Main_Channel_List) {
	delete NML_Main_Channel_List;
	NML_Main_Channel_List = (LinkedList *) NULL;
    }
    if (NULL != Dynamically_Allocated_NML_Objects) {
	delete Dynamically_Allocated_NML_Objects;
	Dynamically_Allocated_NML_Objects = (LinkedList *) NULL;
    }
    if (NULL != NML_Default_Super_Server) {
	delete NML_Default_Super_Server;
	NML_Default_Super_Server = (NML_SUPER_SERVER *) NULL;
    }
}

// This constructor declared private to prevent copying.
NML::NML(NML & nml)
{
}

/* Get Diagnostics Information. */
NML_DIAGNOSTICS_INFO *NML::get_diagnostics_info()
{
    if (NULL == cms) {
	return NULL;
    }
    return (NML_DIAGNOSTICS_INFO *) cms->get_diagnostics_info();
}

void nmlSetHostAlias(const char *hostName, const char *hostAlias)
{
    if (NULL == cmsHostAliases) {
	cmsHostAliases = new LinkedList;
    }
    CMS_HOST_ALIAS_ENTRY entry;
    strncpy(entry.host, hostName, 64);
    strncpy(entry.alias, hostAlias, 64);
    cmsHostAliases->storeAtTail(&entry, sizeof(entry), 1);
}

void nmlClearHostAliases()
{
    if (NULL != cmsHostAliases) {
	delete cmsHostAliases;
	cmsHostAliases = NULL;
    }
}

void nmlAllowNormalConnection()
{
    cms_connection_mode = CMS_NORMAL_CONNECTION_MODE;
}

void nmlForceRemoteConnection()
{
    cms_connection_mode = CMS_FORCE_REMOTE_CONNECTION_MODE;
}

void nmlForceLocalConnection()
{
    cms_connection_mode = CMS_FORCE_LOCAL_CONNECTION_MODE;
}
