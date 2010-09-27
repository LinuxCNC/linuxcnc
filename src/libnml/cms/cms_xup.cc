/********************************************************************
* Description: cms_xup.cc
*   Provides the interface to CMS used by NML update functions
*   including a CMS update function for all the basic C data types
*   to convert NMLmsgs to XDR.
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

extern "C" {
#include <stdlib.h>		/* malloc(), free() */
}

#include "cms.hh"		/* class CMS */
#include "cms_xup.hh"		/* class CMS_XDR_UPDATER */
#include "rcs_print.hh"		/* rcs_print_error() */

/* Member functions for CMS_XDR_UPDATER Class */
CMS_XDR_UPDATER::CMS_XDR_UPDATER(CMS * _cms_parent):CMS_UPDATER(_cms_parent,
    0, 2)
{
    /* Set pointers to NULL. */
    encode_data_stream = (XDR *) NULL;
    decode_data_stream = (XDR *) NULL;
    encode_header_stream = (XDR *) NULL;
    decode_header_stream = (XDR *) NULL;
    encode_queuing_header_stream = (XDR *) NULL;
    decode_queuing_header_stream = (XDR *) NULL;
    encoded_header = NULL;
    encoded_queuing_header = NULL;

    if (!cms_parent->isserver) {
	encoded_data = NULL;
    }
    using_external_encoded_data = 0;

    /* Store and validate constructors arguments. */
    cms_parent = _cms_parent;
    if (NULL == cms_parent) {
	rcs_print_error("CMS parent for updater is NULL.\n");
	status = CMS_UPDATE_ERROR;
	return;
    }

    /* Allocate memory for: */
    /* - encoded copies of the global buffer and the header */
    /* - XDR streams. */
    /* (Also initialize the XDR streams.) */

    /* Allocate the encoded header too large, */
    /* and find out what size it really is. */
    encoded_header = malloc(neutral_size_factor * sizeof(CMS_HEADER));
    if (encoded_header == NULL) {
	rcs_print_error("CMS:can't malloc encoded_header");
	status = CMS_CREATE_ERROR;
	return;
    }

    encode_header_stream = (XDR *) malloc(sizeof(XDR));
    if (encode_header_stream == NULL) {
	cms_parent->status = CMS_CREATE_ERROR;
	rcs_print_error("CMS:can't malloc encode_header_stream");
	return;
    }
    xdrmem_create((XDR *) encode_header_stream, (char *) encoded_header,
	(int) neutral_size_factor * sizeof(CMS_HEADER), XDR_ENCODE);

    decode_header_stream = (XDR *) malloc(sizeof(XDR));
    if (decode_header_stream == NULL) {
	rcs_print_error("CMS:can't malloc decode_header_stream");
	status = CMS_CREATE_ERROR;
	return;
    }
    xdrmem_create((XDR *) decode_header_stream, (char *) encoded_header,
	(int) neutral_size_factor * sizeof(CMS_HEADER), XDR_DECODE);

    /* If queuing is enabled, then initialize streams for */
    /* encoding and decoding the header with the queue information. */
    if (cms_parent->queuing_enabled) {
	/* Allocate the encoded header too large, */
	/* and find out what size it really is. */
	encoded_queuing_header =
	    malloc(neutral_size_factor * sizeof(CMS_QUEUING_HEADER));
	if (encoded_queuing_header == NULL) {
	    rcs_print_error("CMS:can't malloc encoded_queuing_header");
	    status = CMS_CREATE_ERROR;
	    return;
	}
	encode_queuing_header_stream = (XDR *) malloc(sizeof(XDR));
	if (encode_queuing_header_stream == NULL) {
	    status = CMS_CREATE_ERROR;
	    rcs_print_error("CMS:can't malloc encode_queuing_header_stream");
	    return;
	}
	xdrmem_create((XDR *) encode_queuing_header_stream,
	    (char *) encoded_queuing_header,
	    (int) neutral_size_factor * sizeof(CMS_QUEUING_HEADER),
	    XDR_ENCODE);

	decode_queuing_header_stream = (XDR *) malloc(sizeof(XDR));
	if (decode_queuing_header_stream == NULL) {
	    rcs_print_error("CMS:can't malloc decode_queuing_header_stream");
	    status = CMS_CREATE_ERROR;
	    return;
	}
	xdrmem_create((XDR *) decode_queuing_header_stream,
	    (char *) encoded_queuing_header,
	    (int) neutral_size_factor * sizeof(CMS_QUEUING_HEADER),
	    XDR_DECODE);
    }
    if (!cms_parent->isserver) {
	if (cms_parent->enc_max_size > 0
	    && cms_parent->enc_max_size < neutral_size_factor * size) {
	    set_encoded_data(malloc(cms_parent->enc_max_size),
		cms_parent->enc_max_size);
	} else {
	    set_encoded_data(malloc(neutral_size_factor * size),
		neutral_size_factor * size);
	}
    }
    using_external_encoded_data = 0;
}

CMS_XDR_UPDATER::~CMS_XDR_UPDATER()
{
    /* If encoded buffers were used destroy the xdr streams and free */
    /* memory used for encoded buffers. */
    if (NULL != encode_data_stream) {
	xdr_destroy(encode_data_stream);
	free(encode_data_stream);
	encode_data_stream = (XDR *) NULL;
    }
    if (NULL != decode_data_stream) {
	xdr_destroy(decode_data_stream);
	free(decode_data_stream);
	decode_data_stream = (XDR *) NULL;
    }
    if (NULL != encode_header_stream) {
	xdr_destroy(((XDR *) encode_header_stream));
	free(encode_header_stream);
	encode_header_stream = (XDR *) NULL;
    }
    if (NULL != decode_header_stream) {
	xdr_destroy(((XDR *) decode_header_stream));
	free(decode_header_stream);
	decode_header_stream = (XDR *) NULL;
    }
    if (NULL != encode_queuing_header_stream) {
	xdr_destroy(((XDR *) encode_queuing_header_stream));
	free(encode_queuing_header_stream);
	encode_queuing_header_stream = (XDR *) NULL;
    }
    if (NULL != decode_queuing_header_stream) {
	xdr_destroy(((XDR *) decode_queuing_header_stream));
	free(decode_queuing_header_stream);
	decode_queuing_header_stream = (XDR *) NULL;
    }
    if (NULL != encoded_data && !using_external_encoded_data) {
	free(encoded_data);
	encoded_data = NULL;
    }
    if (NULL != encoded_header) {
	free(encoded_header);
	encoded_header = NULL;
    }
    if (NULL != encoded_queuing_header) {
	free(encoded_queuing_header);
	encoded_queuing_header = NULL;
    }
}

void CMS_XDR_UPDATER::set_encoded_data(void *_encoded_data,
    long _encoded_data_size)
{
    /* If the encoded data area has already been setup then release it. */
    if (NULL != encoded_data && !using_external_encoded_data) {
	free(encoded_data);
	encoded_data = NULL;
    }

    encoded_data_size = _encoded_data_size;
    encoded_data = _encoded_data;
    using_external_encoded_data = 1;
    if (encoded_data == NULL) {
	rcs_print_error
	    ("CMS: Attempt to set  encoded_data buffer to NULL.\n");
	status = CMS_MISC_ERROR;
	return;
    }
    /* Allocate memory for XDR structures. */
    if (NULL == encode_data_stream) {
	encode_data_stream = (XDR *) malloc(sizeof(XDR));
    } else {
	xdr_destroy(encode_data_stream);
    }
    if (NULL == encode_data_stream) {
	rcs_print_error("CMS:can't malloc encode_data_stream");
	status = CMS_CREATE_ERROR;
	return;
    }
    if (NULL == decode_data_stream) {
	decode_data_stream = (XDR *) malloc(sizeof(XDR));
    } else {
	xdr_destroy(decode_data_stream);
    }
    if (NULL == decode_data_stream) {
	rcs_print_error("CMS:can't malloc decode_data_stream");
	status = CMS_CREATE_ERROR;
	return;
    }

    /* Initialize the XDR streams. */
    int nsize = (int) (neutral_size_factor * size);
    if (nsize > cms_parent->max_encoded_message_size
	&& cms_parent->max_encoded_message_size > 0) {
	nsize = cms_parent->max_encoded_message_size;
    }
    if (nsize > cms_parent->enc_max_size && cms_parent->enc_max_size > 0) {
	nsize = cms_parent->enc_max_size;
    }
    xdrmem_create(encode_data_stream, (char *) encoded_data, nsize,
	XDR_ENCODE);
    xdrmem_create(decode_data_stream, (char *) encoded_data, nsize,
	XDR_DECODE);

}

int CMS_XDR_UPDATER::set_mode(CMS_UPDATER_MODE _mode)
{
    mode = _mode;
    CMS_UPDATER::set_mode(_mode);
    switch (mode) {
    case CMS_NO_UPDATE:
	current_stream = (XDR *) NULL;
	break;

    case CMS_ENCODE_DATA:
	current_stream = encode_data_stream;
	break;

    case CMS_DECODE_DATA:
	current_stream = decode_data_stream;
	break;

    case CMS_ENCODE_HEADER:
	current_stream = encode_header_stream;
	break;

    case CMS_DECODE_HEADER:
	current_stream = decode_header_stream;
	break;

    case CMS_ENCODE_QUEUING_HEADER:
	current_stream = encode_queuing_header_stream;
	break;

    case CMS_DECODE_QUEUING_HEADER:
	current_stream = decode_queuing_header_stream;
	break;

    default:
	rcs_print_error("CMS updater in invalid mode.(%d)\n", mode);
	return (-1);
    }
    return (0);
}

int CMS_XDR_UPDATER::check_pointer(char *_pointer, long _bytes)
{
    if (NULL == cms_parent || NULL == current_stream) {
	rcs_print_error("CMS_XDR_UPDATER: Required pointer is NULL.\n");
	return (-1);
    }
    if ((current_stream == encode_data_stream) ||
	(current_stream == decode_data_stream) ||
	(mode == CMS_ENCODE_DATA) || (mode == CMS_DECODE_DATA)) {
	int xdr_pos = xdr_getpos(current_stream);
	if (xdr_pos + _bytes > encoded_data_size) {
	    rcs_print_error
		("Encoded message buffer full. (xdr_pos=%d,_bytes=%ld,(xdr_pos+_bytes)=%ld,encoded_data_size=%ld)\n",
		xdr_pos, _bytes, (xdr_pos + _bytes), encoded_data_size);
	    return -1;
	}
    }
    return (cms_parent->check_pointer(_pointer, _bytes));
}

/* Repositions the data buffer to the very beginning */
void CMS_XDR_UPDATER::rewind()
{
    CMS_UPDATER::rewind();
    if (NULL != current_stream) {
	xdr_setpos(current_stream, 0);
    } else {
	rcs_print_error
	    ("CMS_XDR_UPDATER: Can't rewind because current_stream is NULL.\n");
    }
    if (NULL != cms_parent) {
	cms_parent->format_size = 0;
    }
}

int CMS_XDR_UPDATER::get_encoded_msg_size()
{
    if (NULL == current_stream) {
	rcs_print_error
	    ("CMS_XDR_UPDATER can not provide encoded_msg_size because the current stream is NULL.\n");
	return (-1);
    }
    return (xdr_getpos(current_stream));
}

/* bool functions */

CMS_STATUS CMS_XDR_UPDATER::update(bool &x)
{
    /* Check to see if the pointers are in the proper range. */
    if (-1 == check_pointer((char *) &x, sizeof(char))) {
	return (CMS_UPDATE_ERROR);
    }
    if (xdr_char(current_stream, (char *) &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_char failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

/* Char functions */

CMS_STATUS CMS_XDR_UPDATER::update(char &x)
{
    /* Check to see if the pointers are in the proper range. */
    if (-1 == check_pointer((char *) &x, sizeof(char))) {
	return (CMS_UPDATE_ERROR);
    }
    if (xdr_char(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_char failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(char *x, unsigned int len)
{
    /* Check to see if the pointers are in the proper range. */
    if (-1 == check_pointer((char *) x, len * sizeof(char))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_bytes(current_stream, (char **) &x, &len, len) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_bytes failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned char &x)
{
    /* Check to see if the pointers are in the proper range. */
    if (-1 == check_pointer((char *) &x, sizeof(char))) {
	return (CMS_UPDATE_ERROR);
    }
    if (xdr_u_char(current_stream, (unsigned char *) &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_u_char failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned char *x, unsigned int len)
{
    /* Check to see if the pointers are in the proper range. */
    if (-1 == check_pointer((char *) x, len * sizeof(unsigned char))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_bytes(current_stream, (char **) &x, &len, len) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_bytes failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

/* SHORT */

CMS_STATUS CMS_XDR_UPDATER::update(short int &x)
{
    /* Check to see if the pointers are in the proper range. */
    if (-1 == check_pointer((char *) &x, sizeof(short))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_short(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_short failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(short *x, unsigned int len)
{
    /* Check to see if the pointers are in the proper range. */
    if (-1 == check_pointer((char *) x, len * sizeof(short))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_vector(current_stream, (char *) x, len, sizeof(short),
	    (xdrproc_t) xdr_short) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_short) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned short int &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(unsigned short))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_u_short(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_u_short failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }

    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned short *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(unsigned short))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_vector(current_stream,
	    (char *) x, len,
	    sizeof(unsigned short), (xdrproc_t) xdr_u_short) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_u_short) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

/* INT */

CMS_STATUS CMS_XDR_UPDATER::update(int &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(int))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_int(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_int failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(int *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(int))) {
	return (CMS_UPDATE_ERROR);
    }
    if (xdr_vector(current_stream, (char *) x, len, sizeof(int),
	    (xdrproc_t) xdr_int) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector( ... xdr_int) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned int &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(unsigned int))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_u_int(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_u_int failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned int *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(unsigned int))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_vector(current_stream,
	    (char *) x, len,
	    sizeof(unsigned int), (xdrproc_t) xdr_u_int) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_u_int) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

/* LONG */

CMS_STATUS CMS_XDR_UPDATER::update(long int &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(long))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_long(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_long failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(long *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(long))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_vector(current_stream, (char *) x, len, sizeof(long),
	    (xdrproc_t) xdr_long) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_long) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned long int &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(unsigned long))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_u_long(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_u_long failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }

    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(unsigned long *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(unsigned long))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_vector(current_stream,
	    (char *) x, len, sizeof(unsigned long),
	    (xdrproc_t) xdr_u_long) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_u_long) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

/* FLOAT */

CMS_STATUS CMS_XDR_UPDATER::update(float &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(float))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_float(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_float failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(float *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(float))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_vector(current_stream, (char *) x, len, sizeof(float),
	    (xdrproc_t) xdr_float) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_float) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(double &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(double))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_double(current_stream, &x) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_double failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(double *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(double))) {
	return (CMS_UPDATE_ERROR);
    }

    if (xdr_vector(current_stream, (char *) x, len, sizeof(double),
	    (xdrproc_t) xdr_double) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_double) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }

    return (status);
}

/* NOTE: Because XDR does not include seperate facilities for long doubles. */
/* Some resolution will be lost if long doubles are passed through XDR. */
/* This routine is included only for the sake of completeness. */
/* Avoid using long doubles in NML messages. */
CMS_STATUS CMS_XDR_UPDATER::update(long double &x)
{
    if (-1 == check_pointer((char *) &x, sizeof(long double))) {
	return (CMS_UPDATE_ERROR);
    }

    double y;

    y = (double) x;

    if (xdr_double(current_stream, &y) != TRUE) {
	rcs_print_error("CMS_XDR_UPDATER: xdr_double failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }
    x = (long double) y;
    return (status);
}

CMS_STATUS CMS_XDR_UPDATER::update(long double *x, unsigned int len)
{
    if (-1 == check_pointer((char *) x, len * sizeof(long double))) {
	return (CMS_UPDATE_ERROR);
    }

    unsigned int i;
    double *y;
    y = (double *) malloc(sizeof(double) * len);
    for (i = 0; i < len; i++) {
	y[i] = (double) x[i];
    }

    if (xdr_vector(current_stream, (char *) y, len, sizeof(double),
	    (xdrproc_t) xdr_double) != TRUE) {
	rcs_print_error
	    ("CMS_XDR_UPDATER: xdr_vector(... xdr_double) failed.\n");
	return (status = CMS_UPDATE_ERROR);
    }

    for (i = 0; i < len; i++) {
	x[i] = (long double) y[i];
    }
    free(y);
    return (status);
}
