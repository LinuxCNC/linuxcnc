/*************************************************************************
* File: shmem.cc                                                         *
* Authors: Fred Proctor, Will Shackleford                                *
* Purpose: C++ file for the Communication Management System (CMS).       *
*          Includes:                                                     *
*                    1. Member Functions for class SHMEM.                *
* Notes: The class SHMEM should be used by procedures accessing a shared *
*  memory buffer on the same processor.                                  *
* The RCS_SEMAPHORE is no longer used. Instead a section of the shared   *
* memory buffer itself is used to guarantee mutual exclusion.            *
*************************************************************************/

/* Include Files */

extern "C" {

#include <stdio.h>		/* sscanf() */
#include <stddef.h>		/* size_t */
#include <sys/stat.h>		/* S_IRUSR, etc. */
#include <sys/types.h>		/* key_t */
#include <errno.h>		/* errno */
#include <string.h>		/* strchr(), memcpy(), memset() */
#include <stdlib.h>		/* strtod */
}
#include "rcs_print.hh"		/* rcs_print_error() */
#include "cms.hh"		/* class CMS */
#include "shmem.hh"		/* class SHMEM */
#include "shm.hh"		/* class RCS_SHAREDMEM */
#include "timer.hh"		/* etime(), esleep() */
/* Common  Definitions. */
#define MODE (0777)		/* rw-rw-r-- permissions */
			     /* SHMEM Member Functions. *//* Constructor for hard coded tests. */
    SHMEM::SHMEM(char *n, long s, int nt, key_t k, int m):CMS(s)
{
    /* Set pointers to null so only properly opened pointers are closed. */
    shm = NULL;
//  sem = NULL;

    /* save constructor args */
    master = m;
    key = k;
    /* open the shared mem buffer and create mutual exclusion semaphore */
    open();
}

/* Constructor for use with cms_config. */
SHMEM::SHMEM(char *bufline, char *procline, int set_to_server,
    int set_to_master):CMS(bufline, procline, set_to_server)
{
    /* Set pointers to null so only properly opened pointers are closed. */
    shm = NULL;
    sem = NULL;
    sem_delay = 0.00001;
//    char *semdelay_equation;
    use_os_sem = 1;
    use_os_sem_only = 1;
    mutex_type = OS_SEM_MUTEX;
    second_read = 0;

    if (status < 0) {
	rcs_print_error("SHMEM: status = %d\n", status);
	return;
    }

    /* Save parameters from configuration file. */
    if (sscanf(bufline, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %d", &key) != 1) {
	rcs_print_error("SHMEM: Invalid configuration file format.\n");
	return;
    }

    master = is_local_master;
    if (1 == set_to_master) {
	master = 1;
    } else if (-1 == set_to_master) {
	master = 0;
    }

    /* Open the shared memory buffer and create mutual exclusion semaphore. */
    open();
}

SHMEM::~SHMEM()
{
    /* detach from shared memory and semaphores */
    close();
}

/*
  Open the SHMEM buffer
  */
int SHMEM::open()
{
    /* Set pointers to NULL incase error occurs. */
    sem = NULL;
    shm = NULL;
    shm_addr_offset = NULL;
    second_read = 0;

    /* set up the shared memory address and semaphore, in given state */
    if (master) {
	shm = new RCS_SHAREDMEM(key, size, RCS_SHAREDMEM_CREATE, (int) MODE);
	if (shm->addr == NULL) {
	    switch (shm->create_errno) {
	    case EACCES:
		status = CMS_PERMISSIONS_ERROR;
		break;

	    case EEXIST:
		status = CMS_RESOURCE_CONFLICT_ERROR;
		break;

	    case ENOMEM:
	    case ENOSPC:
		status = CMS_CREATE_ERROR;
		break;

	    default:
		status = CMS_MISC_ERROR;
	    }
	    delete shm;
	    shm = NULL;
	    return -1;
	}
	sem =
	    new RCS_SEMAPHORE(key, RCS_SEMAPHORE_CREATE, timeout,
	    (int) MODE, (use_os_sem_only != 0));
	if (NULL == sem) {
	    rcs_print_error("CMS: couldn't create RCS_SEMAPHORE.\n");
	    rcs_print_error(" Possibly out of memory?\n");
	    status = CMS_CREATE_ERROR;
	    return -1;
	}
	if (!sem->valid()) {
	    rcs_print_error("CMS: RCS_SEMAPHORE is invalid.\n");
	    status = CMS_MISC_ERROR;
	    return -1;
	}

	in_buffer_id = 0;
    } else {
	shm = new RCS_SHAREDMEM(key, size, RCS_SHAREDMEM_NOCREATE);
	if (NULL == shm) {
	    rcs_print_error
		("CMS: couldn't create RCS_SHAREDMEM(%d(0x%X), %d(0x%X), RCS_SHAREDMEM_NOCREATE).\n",
		key, key, size, size);
	    status = CMS_CREATE_ERROR;
	    return -1;
	}
	if (shm->addr == NULL) {
	    switch (shm->create_errno) {
	    case EACCES:
		status = CMS_PERMISSIONS_ERROR;
		break;

	    case EEXIST:
		status = CMS_RESOURCE_CONFLICT_ERROR;
		break;

	    case ENOENT:
		status = CMS_NO_MASTER_ERROR;
		break;

	    case ENOMEM:
	    case ENOSPC:
		status = CMS_CREATE_ERROR;
		break;

	    default:
		status = CMS_MISC_ERROR;
	    }
	    delete shm;
	    shm = NULL;
	    return -1;
	}
	sem = new RCS_SEMAPHORE(key, RCS_SEMAPHORE_NOCREATE, timeout);
	if (NULL == sem) {
	    rcs_print_error("CMS: couldn't create RCS_SEMAPHORE.\n");
	    rcs_print_error(" Possibly out of memory?\n");
	    status = CMS_CREATE_ERROR;
	    return -1;
	}
	if (!sem->valid()) {
	    rcs_print_error("CMS: RCS_SEMAPHORE is invalid.\n");
	    status = CMS_MISC_ERROR;
	    return -1;
	}
    }

    if (min_compatible_version < 3.44 && min_compatible_version > 0) {
	total_subdivisions = 1;
    }

    if (min_compatible_version > 2.57 || min_compatible_version <= 0) {
	if (!shm->created) {
	    char *cptr = (char *) shm->addr;
	    cptr[31] = 0;
	    if (strncmp(cptr, BufferName, 31)) {
		rcs_print_error
		    ("Shared memory buffers %s and %s may conflict. (key=%d(0x%X))\n",
		    BufferName, cptr, key, key);
		strncpy(cptr, BufferName, 32);
	    }
	}
	if (master) {
	    strncpy((char *) shm->addr, BufferName, 32);
	}
	shm_addr_offset = (void *) ((char *) (shm->addr) + 32);
	max_message_size -= 32;	/* size of cms buffer available for user */

	/* messages = size - CMS Header space */
	if (enc_max_size <= 0 || enc_max_size > size) {
	    if (neutral) {
		max_encoded_message_size -= 32;
	    } else {
		max_encoded_message_size -=
		    (cms_encoded_data_explosion_factor * 32);
	    }
	}
	/* Maximum size of message after being encoded. */
	guaranteed_message_space -= 32;	/* Largest size message before being
					   encoded that can be guaranteed to
					   fit after xdr. */
	size -= 32;
	size_without_diagnostics -= 32;
	subdiv_size =
	    (size_without_diagnostics -
	    total_connections) / total_subdivisions;
	subdiv_size -= (subdiv_size % 4);
    } else {
	if (master) {
	    memset(shm->addr, 0, size);
	}
	shm_addr_offset = shm->addr;
    }
    skip_area = 32 + total_connections;
    mao.data = shm_addr_offset;
    mao.timeout = timeout;
    mao.total_connections = total_connections;
    mao.sem_delay = sem_delay;
    mao.connection_number = connection_number;
    mao.split_buffer = split_buffer;
    mao.read_only = 0;
    mao.sem = sem;

    fast_mode = !queuing_enabled && !split_buffer && !neutral &&
	(mutex_type == NO_SWITCHING_MUTEX);
    handle_to_global_data = dummy_handle = new PHYSMEM_HANDLE;
    handle_to_global_data->set_to_ptr(shm_addr_offset, size);
    if ((connection_number < 0 || connection_number >= total_connections)
	&& (mutex_type == MAO_MUTEX || mutex_type == MAO_MUTEX_W_OS_SEM)) {
	rcs_print_error("Bad connection number %d\n", connection_number);
	status = CMS_MISC_ERROR;
	return -1;
    }
    return 0;
}

/* Closes the  shared memory and mutual exclusion semaphore  descriptors. */
int SHMEM::close()
{
    int nattch = 0;
    second_read = 0;

    if (NULL != shm) {
	/* see if we're the last one */
	nattch = shm->nattch();
	shm->delete_totally = delete_totally;
	delete shm;
	shm = NULL;
    }
    if (NULL != sem) {
	/* if we're the last one, then make us the master so that the
	   semaphore will go away */
	if ((nattch <= 1 && nattch > -1) || delete_totally) {
	    sem->setflag(RCS_SEMAPHORE_CREATE);
	} else {
	    sem->setflag(RCS_SEMAPHORE_NOCREATE);
	}
	delete sem;
    }
    return 0;
}

/* Access the shared memory buffer. */
CMS_STATUS SHMEM::main_access(void *_local)
{
    /* Check pointers. */
    if (shm == NULL) {
	second_read = 0;
	return (status = CMS_MISC_ERROR);
    }

    mao.read_only = ((internal_access_type == CMS_CHECK_IF_READ_ACCESS) ||
	(internal_access_type == CMS_PEEK_ACCESS) ||
	(internal_access_type == CMS_READ_ACCESS));

    if (sem == NULL) {
	second_read = 0;
	return (status = CMS_MISC_ERROR);
    }
    switch (sem->wait()) {
    case -1:
	rcs_print_error("SHMEM: Can't take semaphore\n");
	second_read = 0;
	return (status = CMS_MISC_ERROR);
    case -2:
	if (timeout > 0) {
	    rcs_print_error("SHMEM: Timed out waiting for semaphore.\n");
	    rcs_print_error("buffer = %s, timeout = %lf sec.\n",
		BufferName, timeout);
	}
	second_read = 0;
	return (status = CMS_TIMED_OUT);
    default:
	break;
    }

    if (second_read > 0 && enable_diagnostics) {
	disable_diag_store = 1;
    }

    /* Perform access function. */
    internal_access(shm->addr, size, _local);

    disable_diag_store = 0;

    sem->post();

    switch (internal_access_type) {

    case CMS_READ_ACCESS:
	break;
    case CMS_WRITE_ACCESS:
    case CMS_WRITE_IF_READ_ACCESS:
	break;

    default:
	break;
    }
    second_read = 0;
    return (status);
}
