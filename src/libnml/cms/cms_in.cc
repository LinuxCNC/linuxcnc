/********************************************************************
* Description: cms_in.cc
*   Provides the internal interface of CMS to the buffer.
*   The following applies to the member functions in this file:
*   They work through a handle to a shared physical memory object.
*   They should only be called when this process has sole access to
*   this shared physical memory object. This is normally achieved by
*   taking a mutual-exclusion semaphore before calling the
*   internal_access function above from the main_access function
*   of a derived class.
*   If they begin with "queue" then they are for buffers where messages
*   are to be queued, other wise they are for buffers with will have
*   only one message at a time.
*   Queuing buffers store a CMS_QUEUING_HEADER at the beginning and a
*   CMS_HEADER before each message. Non-queuing buffers have only a
*   CMS_HEADER before the only message.
*   If they end in "encoded" they are for buffers that will neutrally
*   encoded in some processor architecture independant data format such
*   as XDR (eXternal Data Representation), otherwise the buffer must be
*   in the format used by the same compiler as this is compiled in and
*   for the same processor architecture and the function name will end
*   in "raw".
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

#include "cms.hh"		/* class CMS */
#include "cmsdiag.hh"		// class CMS_DIAG_PROC_INFO, CMS_DIAG_HEADER
#include "rcs_print.hh"		/* rcs_print_error() */
#include "physmem.hh"		/* class PHYSMEM_HANDLE */

/* CMS Member Functions. */

int cms_print_queue_free_space = 0;
int cms_print_queue_full_messages = 1;

/*************************************************************************
* This function should be called by functions overloading main_access()
* It uses a dummy physmem handle so that reads and writes work on
* the memory at _global.
* Parameters:
* _local - Address of local buffer where user has stored messages in or will
* read messages from whithin this process.
* _global - Address of shared or global memory buffer used to communicate with
* other  process.
************************************************************************/
CMS_STATUS CMS::internal_access(void *_global, long _global_size,
    void *_local)
{
    /* Don't bother trying to create a physmem handle for a NULL pointer. */
    if (NULL == _global) {
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Create a dummy physmem handle if I don't already have one. */
    if (NULL == dummy_handle) {
	dummy_handle = new PHYSMEM_HANDLE;
    }

    /* Check for problem allocating memory. */
    if (NULL == dummy_handle) {
	rcs_print_error("CMS: Couldn't create PHYSMEM_HANDLE.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    dummy_handle->set_to_ptr(_global, _global_size);
    internal_access(dummy_handle, _local);
    return (status);
}

 /* This function should be called by classes which overload the main_access
    function. */
CMS_STATUS CMS::internal_access(PHYSMEM_HANDLE * _global, void *_local)
{
    status = CMS_STATUS_NOT_SET;
    if (NULL == _global) {
	rcs_print_error("CMS: Handle to global memory is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
    handle_to_global_data = _global;

    if (CMS_CLEAR_ACCESS == internal_access_type) {
	internal_clear();
	return (status);
    }

    if (min_compatible_version > 2.58 || min_compatible_version < 1E-6) {
	handle_to_global_data->offset += skip_area;
    }

    if (CMS_GET_DIAG_INFO_ACCESS == internal_access_type) {
	internal_retrieve_diag_info(handle_to_global_data, _local);
	return (status);
    }

    long orig_offset = handle_to_global_data->offset;
    if (enable_diagnostics) {
	handle_to_global_data->offset +=
	    sizeof(CMS_DIAG_HEADER) +
	    total_connections * sizeof(CMS_DIAG_PROC_INFO);
	handle_to_global_data->enable_byte_counting = 1;
	pre_op_total_bytes_moved = handle_to_global_data->total_bytes_moved;
    }

    char was_read_byte;
    write_just_completed = 0;
    int read_only = ((internal_access_type == CMS_CHECK_IF_READ_ACCESS) ||
	(internal_access_type == CMS_READ_ACCESS) ||
	(internal_access_type == CMS_PEEK_ACCESS));

    long offset_before_split = handle_to_global_data->offset;

    if (total_subdivisions >= 1 && current_subdivision > 0
	&& current_subdivision < total_subdivisions) {
	handle_to_global_data->offset += (current_subdivision * subdiv_size);
	handle_to_global_data->size =
	    ((current_subdivision + 1) * subdiv_size);
	if (handle_to_global_data->size > size) {
	    handle_to_global_data->size = size;
	}
    }

    if (split_buffer) {
	if (internal_access_type == CMS_WRITE_IF_READ_ACCESS) {
	    handle_to_global_data->offset++;
	    handle_to_global_data->read(&was_read_byte, 1);
	    handle_to_global_data->offset--;
	    header.was_read = (was_read_byte == toggle_bit + 1);
	    if (!header.was_read) {
		status = CMS_WRITE_WAS_BLOCKED;
		return (status);
	    }
	    internal_access_type = CMS_WRITE_ACCESS;
	}
	if (read_only == toggle_bit) {
	    handle_to_global_data->offset += 2;
	    handle_to_global_data->size = half_size;
	} else {
	    handle_to_global_data->offset += half_offset;
	    handle_to_global_data->size = size;
	}
    }

    if (!queuing_enabled) {
	if (neutral) {
	    switch (internal_access_type) {
	    case CMS_CHECK_IF_READ_ACCESS:
		check_if_read_encoded();
		break;
	    case CMS_READ_ACCESS:
		read_encoded();
		break;
	    case CMS_PEEK_ACCESS:
		peek_encoded();
		break;
	    case CMS_WRITE_ACCESS:
		write_encoded();
		break;
	    case CMS_WRITE_IF_READ_ACCESS:
		write_if_read_encoded();
		break;
	    case CMS_GET_MSG_COUNT_ACCESS:
		get_msg_count_encoded();
		break;
	    default:
		return (status = CMS_INTERNAL_ACCESS_ERROR);
	    }
	} else {
	    switch (internal_access_type) {
	    case CMS_CHECK_IF_READ_ACCESS:
		check_if_read_raw();
		break;
	    case CMS_READ_ACCESS:
		read_raw();
		break;
	    case CMS_PEEK_ACCESS:
		peek_raw();
		break;
	    case CMS_WRITE_ACCESS:
		write_raw(_local);
		break;
	    case CMS_WRITE_IF_READ_ACCESS:
		write_if_read_raw(_local);
		break;
	    case CMS_GET_MSG_COUNT_ACCESS:
		get_msg_count_raw();
		break;
	    default:
		return (status = CMS_INTERNAL_ACCESS_ERROR);
	    }
	}
    } else {
	if (neutral) {
	    switch (internal_access_type) {
	    case CMS_CHECK_IF_READ_ACCESS:
		queue_check_if_read_encoded();
		break;
	    case CMS_READ_ACCESS:
		queue_read_encoded();
		break;
	    case CMS_PEEK_ACCESS:
		queue_peek_encoded();
		break;
	    case CMS_WRITE_ACCESS:
		queue_write_encoded();
		break;
	    case CMS_WRITE_IF_READ_ACCESS:
		queue_write_if_read_encoded();
		break;
	    case CMS_GET_QUEUE_LENGTH_ACCESS:
		queue_get_queue_length_encoded();
		break;
	    case CMS_GET_SPACE_AVAILABLE_ACCESS:
		queue_get_space_available_encoded();
		break;
	    case CMS_GET_MSG_COUNT_ACCESS:
		queue_get_msg_count_encoded();
		break;
	    default:
		return (status = CMS_INTERNAL_ACCESS_ERROR);
	    }
	} else {
	    switch (internal_access_type) {
	    case CMS_CHECK_IF_READ_ACCESS:
		queue_check_if_read_raw();
		break;
	    case CMS_READ_ACCESS:
		queue_read_raw();
		break;
	    case CMS_PEEK_ACCESS:
		queue_peek_raw();
		break;
	    case CMS_WRITE_ACCESS:
		queue_write_raw(_local);
		break;
	    case CMS_WRITE_IF_READ_ACCESS:
		queue_write_if_read_raw(_local);
		break;
	    case CMS_GET_QUEUE_LENGTH_ACCESS:
		queue_get_queue_length_raw();
		break;
	    case CMS_GET_SPACE_AVAILABLE_ACCESS:
		queue_get_space_available_raw();
		break;
	    case CMS_GET_MSG_COUNT_ACCESS:
		queue_get_msg_count_raw();
		break;
	    default:
		return (status = CMS_INTERNAL_ACCESS_ERROR);
	    }
	}
    }

    if (split_buffer) {
	handle_to_global_data->offset = offset_before_split + 1;
	if (internal_access_type == CMS_READ_ACCESS) {
	    was_read_byte = 1;
	} else if (!read_only) {
	    was_read_byte = 0;
	}
	if (-1 == handle_to_global_data->write(&was_read_byte, 1)) {
	    rcs_print_error("CMS: can not set was read flag.\n");
	}
    }

    if (enable_diagnostics) {
	handle_to_global_data->offset = orig_offset;
	calculate_and_store_diag_info(handle_to_global_data, _local);
    }
    return (status);
}

/* Clear the shared or global memory. */
CMS_STATUS CMS::internal_clear()
{
    in_buffer_id = 0;

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    if (-1 == handle_to_global_data->clear_memory()) {
	rcs_print_error("CMS: Can't clear global_memory.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
    int temp_offset = handle_to_global_data->offset;
    handle_to_global_data->offset = 0;
    if (-1 == handle_to_global_data->write(BufferName, 32)) {
	rcs_print_error("CMS: Can't clear reset name in global memory.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
    handle_to_global_data->offset = temp_offset;

    return (status = CMS_CLEAR_OK);
}

/* Determine whether the message in the buffer has been read at least once. */
int CMS::check_if_read_raw()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the message. */
    if (-1 == handle_to_global_data->read(&header, sizeof(CMS_HEADER))) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }
    return ((int) header.was_read);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_check_if_read_raw()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    return ((int) (queuing_header.queue_length == 0));
}

/* Determine whether the message in the buffer has been read at least once. */
int CMS::check_if_read_encoded()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Decode the header and store in header structure. */
    decode_header();

    return ((int) header.was_read);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_check_if_read_encoded()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Decode the header and store in queuing_header structure. */
    decode_queuing_header();

    return ((int) (queuing_header.queue_length == 0));
}

/* Determine whether the message in the buffer has been read at least once. */
int CMS::get_msg_count_raw()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the message. */
    if (-1 == handle_to_global_data->read(&header, sizeof(CMS_HEADER))) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }
    return (header.write_id);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_get_msg_count_raw()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    return (header.write_id = queuing_header.write_id);
}

/* Determine whether the message in the buffer has been read at least once. */
int CMS::get_msg_count_encoded()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Decode the header and store in header structure. */
    decode_header();

    return ((int) header.write_id);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_get_msg_count_encoded()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Decode the header and store in queuing_header structure. */
    decode_queuing_header();

    return (header.write_id = queuing_header.write_id);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_get_queue_length_raw()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    return (queuing_header.queue_length);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_get_queue_length_encoded()
{
    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Decode the header and store in queuing_header structure. */
    decode_queuing_header();

    return (queuing_header.queue_length);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_get_space_available_raw()
{
    long queuing_header_offset;

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Store the original offset so that we can update the header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Determine amount of free space and location of next node. */
    if (queuing_header.tail > queuing_header.head) {
	free_space = 0;
	if (handle_to_global_data->size - queuing_header.tail -
	    queuing_header_offset > 0) {
	    free_space =
		handle_to_global_data->size - queuing_header.tail -
		queuing_header_offset;
	}
	if (queuing_header.head - sizeof(CMS_QUEUING_HEADER) > 0) {
	    free_space += queuing_header.head - sizeof(CMS_QUEUING_HEADER);
	}
    } else if (queuing_header.tail < queuing_header.head) {
	free_space = queuing_header.head - queuing_header.tail;
    }

    if (queuing_header.queue_length == 0) {
	free_space = handle_to_global_data->size
	    - sizeof(CMS_QUEUING_HEADER) - queuing_header_offset;
    }

    if (cms_print_queue_free_space) {
	rcs_print("queue free space = %ld\n", free_space);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
    }

    return (free_space);
}

 /* Determine if all of the messages in the buffer have been read. This means 
    the queue is empty. */
int CMS::queue_get_space_available_encoded()
{
    long queuing_header_offset;

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Store the original offset so that we can update the header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS: Error reading from global memory for %s at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	status = CMS_INTERNAL_ACCESS_ERROR;
	return 0;
    }

    /* Decode the header and store in queuing_header structure. */
    decode_queuing_header();

    /* Determine amount of free space and location of next node. */
    if (queuing_header.tail > queuing_header.head) {
	free_space = 0;
	if (handle_to_global_data->size - queuing_header.tail -
	    queuing_header_offset > 0) {
	    free_space =
		handle_to_global_data->size - queuing_header.tail -
		queuing_header_offset;
	}
	if (queuing_header.head - encoded_queuing_header_size -
	    queuing_header_offset > 0) {
	    free_space += queuing_header.head - encoded_queuing_header_size -
		queuing_header_offset;
	}
    } else if (queuing_header.tail < queuing_header.head) {
	free_space = queuing_header.head - queuing_header.tail;
    }

    if (queuing_header.queue_length == 0) {
	free_space = handle_to_global_data->size
	    - encoded_queuing_header_size - queuing_header_offset;
    }

    if (cms_print_queue_free_space) {
	rcs_print("queue free space = %ld\n", free_space);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
    }
    return (free_space);
}

/* It takes several steps to perform a read operation. */
/* 1. Read the header. */
/* 2. Check the id and size. */
/* 3. If id and size are ok, then read the message and update the header. */
CMS_STATUS CMS::read_raw()
{
    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS:(%s) handle_to_global_data is NULL.\n",
	    BufferName);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the header for the message. */
    if (-1 == handle_to_global_data->read(&header, sizeof(CMS_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Set status to CMS_READ_OLD or CMS_READ_OK */
    if (check_id(header.write_id) == CMS_READ_OK) {
	/* Check the size of the message. */
	if (header.in_buffer_size > max_message_size) {
	    rcs_print_error
		("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
		BufferName, header.in_buffer_size, max_message_size);
	    return (status = CMS_INTERNAL_ACCESS_ERROR);
	}

	/* Read the message. */
	handle_to_global_data->offset += sizeof(CMS_HEADER);
	if (-1 ==
	    handle_to_global_data->read(subdiv_data,
		(long) header.in_buffer_size)) {
	    rcs_print_error
		("CMS:(%s) Error reading from global memory at %s:%d\n",
		BufferName, __FILE__, __LINE__);
	    return (status = CMS_INTERNAL_ACCESS_ERROR);
	}
	handle_to_global_data->offset -= sizeof(CMS_HEADER);
    }

    /* Update the header. */
    header.was_read = 1;
    if (-1 == handle_to_global_data->write(&header, sizeof(CMS_HEADER))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status);
}

/* It takes several steps to perform a read operation when queuing is enabled. */
/* 1. Read the queuing_header at the beginning of the buffer. */
/* 2. Get the head of the queue from the queuing_header. */
/* 3. Read the message header at the head of the queue. */
/* 4. Check the id and size. */
/* 5. If id and size are ok, */
/* then read the message and */
/* update both the queuing header and message header. */
CMS_STATUS CMS::queue_read_raw()
{
    long queuing_header_offset;

    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Store the original offset so that we can update the header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the queuing header for the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check to see if there are any unread messages. */
    if (queuing_header.queue_length == 0) {
	return (status = CMS_READ_OLD);
    }

    /* Read the header for the message. */
    handle_to_global_data->offset += queuing_header.head;
    if (-1 == handle_to_global_data->read(&header, sizeof(CMS_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check the size of the message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Update the message header. */
    header.was_read = 1;
    if (-1 == handle_to_global_data->write(&header, sizeof(CMS_HEADER))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the message. */
    handle_to_global_data->offset += sizeof(CMS_HEADER);
    if (-1 ==
	handle_to_global_data->read(subdiv_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Update the queuing header for the buffer. */
    queuing_header.head += header.in_buffer_size + sizeof(CMS_HEADER);
    if (queuing_header.head >= queuing_header.end_queue_space) {
	queuing_header.head = sizeof(CMS_QUEUING_HEADER);
    }
    queuing_header.queue_length--;
    if (queuing_header.queue_length == 0) {
	queuing_header.head = queuing_header.tail =
	    sizeof(CMS_QUEUING_HEADER);
	queuing_header.end_queue_space = queuing_header.tail;
    }
    handle_to_global_data->offset = queuing_header_offset;
    if (-1 == handle_to_global_data->write(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check_id so that debug variables for messages missed can be set. */
    check_id(header.write_id);

    return (status);
}

/* It takes several steps to perform a read operation on an neutral buffer.*/
/* 1. Read the encoded  header. */
/* 2. Decode the header. */
/* 3. Check the id and size. */
/* 4. If id and size are ok, then read the message and update the header. */
CMS_STATUS CMS::read_encoded()
{
    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to the global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the encoded header for the message. */
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Decode the header and store in header structure. */
    decode_header();

    /* Determine if the message in the buffer is new to this process. */
    if (check_id(header.write_id) == CMS_READ_OK) {
	/* Check the size of the message. */
	if (header.in_buffer_size > max_message_size) {
	    rcs_print_error
		("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
		BufferName, header.in_buffer_size, max_message_size);
	    return (status = CMS_INTERNAL_ACCESS_ERROR);
	}

	/* Read the message. */
	handle_to_global_data->offset += encoded_header_size;
	if (-1 == handle_to_global_data->read(encoded_data,
		(long) header.in_buffer_size)) {
	    rcs_print_error
		("CMS:(%s) Error writing to global memory at %s:%d\n",
		BufferName, __FILE__, __LINE__);
	    return (status = CMS_INTERNAL_ACCESS_ERROR);
	}
	handle_to_global_data->offset -= encoded_header_size;
    }

    /* Update the header. */
    header.was_read = 1;

    encode_header();
    if (-1 ==
	handle_to_global_data->write(encoded_header, encoded_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status);
}

/* It takes several steps to perform a read operation */
 /* when queuing is enabled on a neutral buffer. */
/* 1. Read the encoded queuing_header at the beginning of the buffer. */
/* 2. Decode the queuing_header for the buffer. */
/* 3. Get the head of the queue from the queuing_header. */
/* 4. Read the message header at the head of the queue. */
/* 5. Decode the message header. */
/* 6. Check the id and size. */
/* 7. If id and size are ok, */
 /* then read the message */
CMS_STATUS CMS::queue_read_encoded()
{
    long queuing_header_offset;

    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Store the original offset so we can update the queuing header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the encoded header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Decode the queuing header and store in the queuing_header structrure. */
    decode_queuing_header();

    /* Determine if there are any unread messages. */
    if (queuing_header.queue_length == 0) {
	return (status = CMS_READ_OLD);
    }

    /* Read the header for the message. */
    handle_to_global_data->offset += queuing_header.head;
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head, queuing_header.tail,
	    queuing_header.end_queue_space, queuing_header.queue_length,
	    queuing_header.write_id);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Decode the message header and store in the header structure. */
    decode_header();

    /* Check the size of the message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Update the message header. */
    header.was_read = 1;
    encode_header();
    if (-1 ==
	handle_to_global_data->write(encoded_header, encoded_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the message. */
    handle_to_global_data->offset += encoded_header_size;
    if (-1 ==
	handle_to_global_data->read(encoded_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head, queuing_header.tail,
	    queuing_header.end_queue_space, queuing_header.queue_length,
	    queuing_header.write_id);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Update the buffer header. */
    queuing_header.head += header.in_buffer_size + encoded_header_size;
    if (queuing_header.head >= queuing_header.end_queue_space) {
	queuing_header.head = encoded_queuing_header_size;
    }
    queuing_header.queue_length--;
    if (queuing_header.queue_length == 0) {
	queuing_header.head = queuing_header.tail =
	    encoded_queuing_header_size;
	queuing_header.end_queue_space = queuing_header.tail;
    }
    encode_queuing_header();
    handle_to_global_data->offset = queuing_header_offset;
    if (-1 == handle_to_global_data->write(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check_id so that debug variables for messages missed can be set. */
    check_id(header.write_id);

    return (status);
}

/****************************************************************
* Peek operations are the same as reads,
 except that the header(s) are not updated.
****************************************************************/

/* It takes several steps to perform a peek operation. */
/* 1. Read the header. */
/* 2. Check the id and size. */
/* 3. If id and size are ok, then read the message. */
CMS_STATUS CMS::peek_raw()
{
    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS:(%s) handle_to_global_data is NULL.\n",
	    BufferName);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the header for the message. */
    if (-1 == handle_to_global_data->read(&header, sizeof(CMS_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Set status to CMS_READ_OLD or CMS_READ_OK */
    if (check_id(header.write_id) == CMS_READ_OLD) {
	return (status);	/* Don't bother copying out an old message. */
    }

    /* Check the size of the message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the message. */
    handle_to_global_data->offset += sizeof(CMS_HEADER);
    if (-1 ==
	handle_to_global_data->read(subdiv_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status);
}

/* It takes several steps to perform a peek  operation when queuing is enabled. */
/* 1. Read the queuing_header at the beginning of the buffer. */
/* 2. Get the head of the queue from the queuing_header. */
/* 3. Read the message header at the head of the queue. */
/* 4. Check the id and size. */
/* 5. If id and size are ok, */
 /* then read the message */
CMS_STATUS CMS::queue_peek_raw()
{
    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the queuing header for the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check to see if there are any unread messages. */
    if (queuing_header.queue_length == 0) {
	return (status = CMS_READ_OLD);
    }

    /* Read the header for the message. */
    handle_to_global_data->offset += queuing_header.head;
    if (-1 == handle_to_global_data->read(&header, sizeof(CMS_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check the size of the message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the message. */
    handle_to_global_data->offset += sizeof(CMS_HEADER);
    if (-1 ==
	handle_to_global_data->read(subdiv_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check_id so that debug variables for messages missed can be set. */
    check_id(header.write_id);

    return (status);
}

/* It takes several steps to perform a peek operation on an neutral buffer.*/
/* 1. Read the encoded  header. */
/* 2. Decode the header. */
/* 3. Check the id and size. */
/* 4. If id and size are ok, then read the message. */
CMS_STATUS CMS::peek_encoded()
{
    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to the global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the encoded header for the message. */
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Decode the header and store in header structure. */
    decode_header();

    /* Determine if the message in the buffer is new to this process. */
    if (CMS_READ_OLD == check_id(header.write_id)) {
	return (CMS_READ_OLD);	/* Don't bother reading an old message. */
    }

    /* Check the size of the message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the message. */
    handle_to_global_data->offset += encoded_header_size;
    if (-1 == handle_to_global_data->read(encoded_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status);
}

/* It takes several steps to perform a peek operation */
 /* when queuing is enabled on a neutral buffer. */
/* 1. Read the encoded queuing_header at the beginning of the buffer. */
/* 2. Decode the queuing_header for the buffer. */
/* 3. Get the head of the queue from the queuing_header. */
/* 4. Read the message header at the head of the queue. */
/* 5. Decode the message header. */
/* 6. Check the id and size. */
/* 7. If id and size are ok, */
 /* then read the message */
CMS_STATUS CMS::queue_peek_encoded()
{
    /* Produce error message if process does not have permission to read. */
    if (!read_permission_flag) {
	rcs_print_error("CMS: %s was not configured to read %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Check that the handle to global memory exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the encoded header for the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Decode the queuing header and store in the queuing_header structrure. */
    decode_queuing_header();

    /* Determine if there are any unread messages. */
    if (queuing_header.queue_length == 0) {
	return (status = CMS_READ_OLD);
    }

    /* Read the header for the message. */
    handle_to_global_data->offset += queuing_header.head;
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Decode the message header and store in the header structure. */
    decode_header();

    /* Check the size of the message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size of %ld exceeds maximum of %ld\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the message. */
    handle_to_global_data->offset += encoded_header_size;
    if (-1 ==
	handle_to_global_data->read(encoded_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check_id so that debug variables for messages missed can be set. */
    check_id(header.write_id);

    return (status);
}

/* It takes several steps to perform a write operation. */
/* 1. Read the header. */
/* 2. Update the header. */
/* 3. Write the message. */
/* Parameters: */
 /* user_data - pointer to where the user stored the message to be written. */
CMS_STATUS CMS::write_raw(void *user_data)
{
    long current_header_in_buffer_size;

    /* Produce error message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header_in_buffer_size = header.in_buffer_size;

    /* Check that handle to global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
#if 0
    /* Check that buffer is large enough for this message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size %ld exceeds maximum for this buffer of %ld.\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
#endif

    /* Read the header. */
    if (-1 == handle_to_global_data->read(&header, sizeof(header))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Update the header. */
    header.was_read = 0;
    header.write_id++;
    if (split_buffer) {
	if ((header.write_id & 1) != toggle_bit) {
	    header.write_id++;
	}
    }
    header.in_buffer_size = current_header_in_buffer_size;
    if (-1 == handle_to_global_data->write(&header, sizeof(header))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    if (!disable_final_write_raw_for_dma) {
	handle_to_global_data->offset += sizeof(CMS_HEADER);
	if (-1 == handle_to_global_data->write(user_data, (long)
		current_header_in_buffer_size)) {
	    rcs_print_error
		("CMS:(%s) Error writing %ld bytes to global memory at offset %p\n (See  %s line %d.)\n",
		BufferName, header.in_buffer_size, user_data, __FILE__,
		__LINE__);
	    return (status = CMS_INTERNAL_ACCESS_ERROR);
	}
    }

    return (status = CMS_WRITE_OK);
}

/* It takes several steps to perform a write operation when queuing is enabled. */
/* 1. Read the qeuing header at the begining of the buffer. */
/* 2. Determine the amount of free space and where the next node can be placed.*/
/* 3. Set up message header from info in the queuing header. */
/* 4. Write the message header and message  at the tail of the queue. */
/* 5. Update the queuing header. */
/* Parameters: */
 /* user_data - pointer to where the user stored the message to be written. */
CMS_STATUS CMS::queue_write_raw(void *user_data)
{
    CMS_HEADER current_header;
    long queuing_header_offset;
    long original_tail;

    /* Produce error message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header = header;

    /* Check that the handle to the global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Store the original offset so we can update the queuing header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the queuing header at the beginning of the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Determine amount of free space and location of next node. */

    if (handle_to_global_data->size - queuing_header.tail -
	queuing_header_offset >
	((long) (header.in_buffer_size + sizeof(CMS_HEADER)))
	&& queuing_header.tail > queuing_header.head) {
	free_space =
	    handle_to_global_data->size - queuing_header.tail -
	    queuing_header_offset;
    } else if (queuing_header.tail < queuing_header.head) {
	free_space = queuing_header.head - queuing_header.tail;
    } else if (queuing_header.head >
	((long) (queuing_header_offset + sizeof(CMS_QUEUING_HEADER) +
		(header.in_buffer_size + sizeof(CMS_HEADER))))) {
	queuing_header.end_queue_space = queuing_header.tail;
	queuing_header.tail = sizeof(CMS_QUEUING_HEADER);
	free_space =
	    queuing_header.head - sizeof(CMS_QUEUING_HEADER) -
	    queuing_header_offset;
    } else {
	free_space = 0;
    }

    if (queuing_header.queue_length == 0) {
	queuing_header.head = queuing_header.tail =
	    sizeof(CMS_QUEUING_HEADER);
	queuing_header.queue_length = 0;
	queuing_header.end_queue_space = queuing_header.tail;
	free_space = handle_to_global_data->size
	    - sizeof(CMS_QUEUING_HEADER) - queuing_header_offset;
    }

    if (cms_print_queue_free_space) {
	rcs_print("queue free space = %ld\n", free_space);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
    }

    /* Check to see if there is enough free space. */
    if (free_space < ((long) (header.in_buffer_size + sizeof(CMS_HEADER)))) {
	if (cms_print_queue_free_space || cms_print_queue_full_messages) {
	    rcs_print_error("CMS: %s message queue is full.\n", BufferName);
	    rcs_print_error
		("(continued) CMS: Message requires %ld bytes but only %ld bytes are left.\n",
		header.in_buffer_size, free_space);
	}
	return (status = CMS_QUEUE_FULL);
    }

    /* Store original tail so we'll know where to store the message. */
    original_tail = queuing_header.tail;

    /* Update the queuing header. */
    queuing_header.tail += header.in_buffer_size + sizeof(CMS_HEADER);
    queuing_header.queue_length++;
    queuing_header.write_id++;
    if (queuing_header.end_queue_space < queuing_header.tail) {
	queuing_header.end_queue_space = queuing_header.tail;
    }
    if (-1 == handle_to_global_data->write(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Setup message header. */
    header.write_id = queuing_header.write_id;
    header.was_read = 0;
    header.in_buffer_size = current_header.in_buffer_size;

    /* Write the message header. */
    handle_to_global_data->offset += original_tail;
    if (-1 == handle_to_global_data->write(&header, sizeof(header))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    handle_to_global_data->offset += sizeof(CMS_HEADER);
    if (-1 == handle_to_global_data->write(user_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status = CMS_WRITE_OK);
}

/* It takes several steps to perform a write operation on a neutral buffer. */
/* 1. Read the header. */
/* 2. Update the header. */
/* 3. Write the message. */
CMS_STATUS CMS::write_encoded()
{
    CMS_HEADER current_header;

    /* Produce error message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header = header;

    /* Check that handle to global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check that buffer is large enough for this message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size %ld exceeds maximum for this buffer of %ld.\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the header. */
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
    /* Decode the header and store in the header structure. */
    decode_header();

    /* Update the header. */
    header.was_read = 0;
    header.write_id++;
    if (split_buffer && (header.write_id % 2) != toggle_bit) {
	header.write_id++;
    }
    header.in_buffer_size = current_header.in_buffer_size;
    encode_header();
    if (-1 ==
	handle_to_global_data->write(encoded_header, encoded_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    handle_to_global_data->offset += encoded_header_size;
    if (-1 == handle_to_global_data->write(encoded_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status = CMS_WRITE_OK);
}

/* It takes several steps to perform a write operation when queuing is enabled. */
/* 1. Read the qeuing header at the begining of the buffer. */
/* 2. Determine the amount of free space and where the next node can be placed.*/
/* 3. Set up message header from info in the queuing header. */
/* 4. Write the message header and message  at the tail of the queue. */
/* 5. Update the queuing header. */
/* Parameters: */
 /* user_data - pointer to where the user stored the message to be written. */
CMS_STATUS CMS::queue_write_encoded()
{
    CMS_HEADER current_header;
    long queuing_header_offset;
    long original_tail;

    /* Produce error message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header = header;

    /* Check that the handle to the global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Store the original offset so we can update the queuing header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the queuing header at the beginning of the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
    /* Decode queuing header and store in queuing_header structure. */
    decode_queuing_header();

    /* Determine amount of free space and location of next node. */
    if (handle_to_global_data->size - queuing_header.tail -
	queuing_header_offset
	>
	header.in_buffer_size + encoded_header_size
	&& queuing_header.tail > queuing_header.head) {
	free_space =
	    handle_to_global_data->size - queuing_header.tail -
	    queuing_header_offset;
    } else if (queuing_header.tail < queuing_header.head) {
	free_space = queuing_header.head - queuing_header.tail;
    } else if (queuing_header.head >
	encoded_header_size + queuing_header_offset +
	header.in_buffer_size + encoded_queuing_header_size) {
	queuing_header.end_queue_space = queuing_header.tail;
	queuing_header.tail = encoded_queuing_header_size;
	free_space =
	    queuing_header.head - encoded_queuing_header_size -
	    queuing_header_offset;
    } else {
	free_space = 0;
    }

    if (queuing_header.queue_length == 0) {
	queuing_header.head = queuing_header.tail =
	    encoded_queuing_header_size;
	queuing_header.queue_length = 0;
	queuing_header.end_queue_space = queuing_header.tail;
	free_space = handle_to_global_data->size
	    - encoded_queuing_header_size - queuing_header_offset;
    }

    if (cms_print_queue_free_space) {
	rcs_print("queue free space = %ld\n", free_space);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
    }

    /* Check to see if there is enough free space. */
    if (free_space < header.in_buffer_size + encoded_header_size) {
	if (cms_print_queue_free_space || cms_print_queue_full_messages) {
	    rcs_print_error("CMS: %s message queue is full.\n", BufferName);
	    rcs_print_error
		("(continued) CMS: Message requires %ld bytes but only %ld bytes are left.\n",
		header.in_buffer_size, free_space);
	}
	return (status = CMS_QUEUE_FULL);
    }

    /* Store original tail so we'll know where to store the message. */
    original_tail = queuing_header.tail;

    /* Update the queuing header. */
    queuing_header.tail += header.in_buffer_size + encoded_header_size;
    queuing_header.queue_length++;
    queuing_header.write_id++;
    if (queuing_header.end_queue_space < queuing_header.tail) {
	queuing_header.end_queue_space = queuing_header.tail;
    }
    encode_queuing_header();
    if (-1 == handle_to_global_data->write(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Setup message header. */
    header.write_id = queuing_header.write_id;
    header.was_read = 0;
    header.in_buffer_size = current_header.in_buffer_size;

    /* Re-encode the header. */
    encode_header();

    /* Write the message header. */
    handle_to_global_data->offset += original_tail;
    if (-1 ==
	handle_to_global_data->write(encoded_header, encoded_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    handle_to_global_data->offset += encoded_header_size;
    if (-1 == handle_to_global_data->write(encoded_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status = CMS_WRITE_OK);
}

/* It takes several steps to perform a write operation. */
/* 1. Read the header. */
/* 2. Update the header. */
/* 3. Write the message. */
/* Parameters: */
 /* user_data - pointer to where the user stored the message to be written. */
CMS_STATUS CMS::write_if_read_raw(void *user_data)
{
    CMS_HEADER current_header;

    /* Produce error message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header = header;

    /* Check that handle to global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check that buffer is large enough for this message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size %ld exceeds maximum for this buffer of %ld.\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the header. */
    if (-1 == handle_to_global_data->read(&header, sizeof(header))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check if the message in the buffer has been read. */
    if (!header.was_read) {
	return (status = CMS_WRITE_WAS_BLOCKED);
    }

    /* Update the header. */
    header.was_read = 0;
    header.write_id++;
    if (split_buffer && (header.write_id % 2) != toggle_bit) {
	header.write_id++;
    }
    header.in_buffer_size = current_header.in_buffer_size;
    if (-1 == handle_to_global_data->write(&header, sizeof(header))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    handle_to_global_data->offset += sizeof(CMS_HEADER);
    if (-1 == handle_to_global_data->write(user_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status = CMS_WRITE_OK);
}

/* It takes several steps to perform a write operation when queuing is enabled. */
/* 1. Read the qeuing header at the begining of the buffer. */
/* 2. Determine the amount of free space and where the next node can be placed.*/
/* 3. Set up message header from info in the queuing header. */
/* 4. Write the message header and message  at the tail of the queue. */
/* 5. Update the queuing header. */
/* Parameters: */
 /* user_data - pointer to where the user stored the message to be written. */
CMS_STATUS CMS::queue_write_if_read_raw(void *user_data)
{
    CMS_HEADER current_header;
    long queuing_header_offset;
    long original_tail;

    /* Produce error message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header = header;

    /* Check that the handle to the global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Store the original offset so we can update the queuing header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the queuing header at the beginning of the buffer. */
    if (-1 == handle_to_global_data->read(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check if all the messages in the buffer have been read. */
    if (0 != queuing_header.queue_length) {
	return (status = CMS_WRITE_WAS_BLOCKED);
    }

    /* Determine amount of free space and location of next node. */
    if (handle_to_global_data->size - queuing_header.tail -
	queuing_header_offset >
	((long) (header.in_buffer_size + sizeof(CMS_HEADER)))
	&& queuing_header.tail > queuing_header.head) {
	free_space =
	    handle_to_global_data->size - queuing_header.tail -
	    queuing_header_offset;
    } else if (queuing_header.tail < queuing_header.head) {
	free_space = queuing_header.head - queuing_header.tail;
    } else if (queuing_header.head >
	((long) (queuing_header_offset + sizeof(CMS_QUEUING_HEADER) +
		header.in_buffer_size + sizeof(CMS_HEADER)))) {
	queuing_header.end_queue_space = queuing_header.tail;
	queuing_header.tail = sizeof(CMS_QUEUING_HEADER);
	free_space =
	    queuing_header.head - sizeof(CMS_QUEUING_HEADER) -
	    queuing_header_offset;
    } else {
	free_space = 0;
    }

    if (queuing_header.queue_length == 0) {
	queuing_header.head = queuing_header.tail =
	    sizeof(CMS_QUEUING_HEADER);
	queuing_header.queue_length = 0;
	queuing_header.end_queue_space = queuing_header.tail;
	free_space = handle_to_global_data->size
	    - sizeof(CMS_QUEUING_HEADER) - queuing_header_offset;
    }

    if (cms_print_queue_free_space) {
	rcs_print("queue free space = %ld\n", free_space);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
    }

    /* Check to see if there is enough free space. */
    if (free_space < ((long) (header.in_buffer_size + sizeof(CMS_HEADER)))) {
	if (cms_print_queue_free_space || cms_print_queue_full_messages) {
	    rcs_print_error("CMS: %s message queue is full.\n", BufferName);
	    rcs_print_error
		("(continued) CMS: Message requires %ld bytes but only %ld bytes are left.\n",
		header.in_buffer_size, free_space);
	}
	return (status = CMS_QUEUE_FULL);
    }

    /* Store original tail so we'll know where to store the message. */
    original_tail = queuing_header.tail;

    /* Update the queuing header. */
    queuing_header.tail += header.in_buffer_size + sizeof(CMS_HEADER);
    queuing_header.queue_length++;
    queuing_header.write_id++;
    if (queuing_header.end_queue_space < queuing_header.tail) {
	queuing_header.end_queue_space = queuing_header.tail;
    }
    if (-1 == handle_to_global_data->write(&queuing_header,
	    sizeof(CMS_QUEUING_HEADER))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Setup message header. */
    header.write_id = queuing_header.write_id;
    header.was_read = 0;
    header.in_buffer_size = current_header.in_buffer_size;

    /* Write the message header. */
    handle_to_global_data->offset += original_tail;
    if (-1 == handle_to_global_data->write(&header, sizeof(header))) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    handle_to_global_data->offset += sizeof(CMS_HEADER);
    if (-1 == handle_to_global_data->write(user_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status = CMS_WRITE_OK);
}

/* It takes several steps to perform a write operation on a neutral buffer. */
/* 1. Read the header. */
/* 2. Update the header. */
/* 3. Write the message. */
CMS_STATUS CMS::write_if_read_encoded()
{
    CMS_HEADER current_header;

    /* Produce error message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header = header;

    /* Check that handle to global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Check that buffer is large enough for this message. */
    if (header.in_buffer_size > max_message_size) {
	rcs_print_error
	    ("CMS:(%s) Message size %ld exceeds maximum for this buffer of %ld.\n",
	    BufferName, header.in_buffer_size, max_message_size);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Read the header. */
    if (-1 == handle_to_global_data->read(encoded_header,
	    encoded_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
    /* Decode the header and store in the header structure. */
    decode_header();

    /* Check if the message in the buffer has been read. */
    if (!header.was_read) {
	return (status = CMS_WRITE_WAS_BLOCKED);
    }

    /* Update the header. */
    header.was_read = 0;
    header.write_id++;
    if (split_buffer && (header.write_id % 2) != toggle_bit) {
	header.write_id++;
    }
    header.in_buffer_size = current_header.in_buffer_size;
    encode_header();
    if (-1 ==
	handle_to_global_data->write(encoded_header, encoded_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    handle_to_global_data->offset += encoded_header_size;
    if (-1 == handle_to_global_data->write(encoded_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status = CMS_WRITE_OK);
}

/* It takes several steps to perform a write operation when queuing is enabled. */
/* 1. Read the qeuing header at the begining of the buffer. */
/* 2. Determine the amount of free space and where the next node can be placed.*/
/* 3. Set up message header from info in the queuing header. */
/* 4. Write the message header and message  at the tail of the queue. */
/* 5. Update the queuing header. */
/* Parameters: */
 /* user_data - pointer to where the user stored the message to be written. */
CMS_STATUS CMS::queue_write_if_read_encoded()
{
    CMS_HEADER current_header;
    long queuing_header_offset;
    long original_tail;

    /* Produce warning message if process does not have permission to read. */
    if (!write_permission_flag) {
	rcs_print_error("CMS: %s was not configured to write to %s\n",
	    ProcessName, BufferName);
	return (status = CMS_PERMISSIONS_ERROR);
    }

    /* Store the header information to use after reading the header in the
       buffer. */
    current_header = header;

    /* Check that the handle to the global memory object exists. */
    if (NULL == handle_to_global_data) {
	rcs_print_error("CMS: handle_to_global_data is NULL.\n");
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Store the original offset so we can update the queuing header later. */
    queuing_header_offset = handle_to_global_data->offset;

    /* Read the queuing header at the beginning of the buffer. */
    if (-1 == handle_to_global_data->read(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error
	    ("CMS:(%s) Error reading from global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }
    /* Decode queuing header and store in queuing_header structure. */
    decode_queuing_header();

    /* Check if all the messages in the buffer have been read. */
    if (0 != queuing_header.queue_length) {
	return (status = CMS_WRITE_WAS_BLOCKED);
    }

    /* Determine amount of free space and location of next node. */
    if (handle_to_global_data->size - queuing_header.tail -
	queuing_header_offset > header.in_buffer_size + encoded_header_size
	&& queuing_header.tail > queuing_header.head) {
	free_space =
	    handle_to_global_data->size - queuing_header.tail -
	    queuing_header_offset;
    } else if (queuing_header.tail < queuing_header.head) {
	free_space = queuing_header.head - queuing_header.tail;
    } else if (queuing_header.head >
	encoded_header_size + queuing_header_offset +
	header.in_buffer_size + encoded_queuing_header_size) {
	queuing_header.end_queue_space = queuing_header.tail;
	queuing_header.tail = encoded_queuing_header_size;
	free_space =
	    queuing_header.head - encoded_queuing_header_size -
	    queuing_header_offset;
    } else {
	free_space = 0;
    }

    if (queuing_header.queue_length == 0) {
	queuing_header.head = queuing_header.tail =
	    encoded_queuing_header_size;
	queuing_header.queue_length = 0;
	queuing_header.end_queue_space = queuing_header.tail;
	free_space = handle_to_global_data->size
	    - encoded_queuing_header_size - queuing_header_offset;
    }

    if (cms_print_queue_free_space) {
	rcs_print("queue free space = %ld\n", free_space);
	rcs_print(" { head=%ld,tail=%ld,end=%ld,length=%ld,id=%ld }\n",
	    queuing_header.head,
	    queuing_header.tail,
	    queuing_header.end_queue_space,
	    queuing_header.queue_length, queuing_header.write_id);
    }

    /* Check to see if there is enough free space. */
    if (free_space < header.in_buffer_size + encoded_header_size) {
	if (cms_print_queue_free_space || cms_print_queue_full_messages) {
	    rcs_print_error("CMS: %s message queue is full.\n", BufferName);
	    rcs_print_error
		("(continued) CMS: Message requires %ld bytes but only %ld bytes are left.\n",
		header.in_buffer_size, free_space);
	}
	return (status = CMS_QUEUE_FULL);
    }

    /* Store original tail so we'll know where to store the message. */
    original_tail = queuing_header.tail;

    /* Update the queuing header. */
    queuing_header.tail += header.in_buffer_size + encoded_header_size;
    queuing_header.queue_length++;
    queuing_header.write_id++;
    if (queuing_header.end_queue_space < queuing_header.tail) {
	queuing_header.end_queue_space = queuing_header.tail;
    }
    encode_queuing_header();
    if (-1 == handle_to_global_data->write(encoded_queuing_header,
	    encoded_queuing_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Setup message header. */
    header.write_id = queuing_header.write_id;
    header.was_read = 0;
    header.in_buffer_size = current_header.in_buffer_size;

    /* Re-encode the header. */
    encode_header();

    /* Write the message header. */
    handle_to_global_data->offset += original_tail;
    if (-1 ==
	handle_to_global_data->write(encoded_header, encoded_header_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    /* Write the message. */
    handle_to_global_data->offset += encoded_header_size;
    if (-1 == handle_to_global_data->write(encoded_data,
	    (long) header.in_buffer_size)) {
	rcs_print_error("CMS:(%s) Error writing to global memory at %s:%d\n",
	    BufferName, __FILE__, __LINE__);
	return (status = CMS_INTERNAL_ACCESS_ERROR);
    }

    return (status = CMS_WRITE_OK);
}
