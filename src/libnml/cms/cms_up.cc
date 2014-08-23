/********************************************************************
* Description: cms_up.cc
*   Provides the interface to CMS used by NML update functions
*   including a CMS update function for all the basic C data types.
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
#include "cms_up.hh"		/* class CMS_UPDATER */
#include "rcs_print.hh"		/* rcs_print_error() */

#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>		// malloc(), free()
#include <string.h>		// memset()
#ifdef __cplusplus
}
#endif

CMS_UPDATER::CMS_UPDATER(CMS * _cms_parent, int create_encoded_data,
    long _neutral_size_factor):
  /**********************************************
  * Aliases to variables in the CMS parent
  * using aliases lets CMS and its CMS_UPDATER share this information
  * more conveniently and allowed the CMS_UPDATER functions to be pulled out
  * of CMS with fewer changes. (WPS - 6/12/95)
  *********************************************/
encoded_data(_cms_parent->encoded_data),
encoded_header(_cms_parent->encoded_header),
encoded_queuing_header(_cms_parent->encoded_queuing_header),
status(_cms_parent->status),
size(_cms_parent->size),
encoded_header_size(_cms_parent->encoded_header_size),
encoded_queuing_header_size(_cms_parent->encoded_queuing_header_size),
using_external_encoded_data(_cms_parent->using_external_encoded_data),
pointer_check_disabled(_cms_parent->pointer_check_disabled),
encoded_data_size(_cms_parent->encoded_data_size)
{
    cms_parent = _cms_parent;
    mode = CMS_NO_UPDATE;
    neutral_size_factor = _neutral_size_factor;
    if (encoded_data == NULL && create_encoded_data) {
	if (cms_parent->enc_max_size > 0
	    && cms_parent->enc_max_size < neutral_size_factor * size) {
	    set_encoded_data(malloc(cms_parent->enc_max_size),
		cms_parent->enc_max_size);
	} else {
	    set_encoded_data(malloc(neutral_size_factor * size),
		neutral_size_factor * size);
	}
	using_external_encoded_data = 0;
    }
}

CMS_UPDATER::~CMS_UPDATER()
{
}

void CMS_UPDATER::rewind()
{
}

int CMS_UPDATER::set_mode(CMS_UPDATER_MODE _mode)
{
    mode = _mode;
    switch (mode) {
    case CMS_NO_UPDATE:
	break;

    case CMS_ENCODE_DATA:
	encoding = 1;
	break;

    case CMS_DECODE_DATA:
	encoding = 0;
	break;

    case CMS_ENCODE_HEADER:
	encoding = 1;
	break;

    case CMS_DECODE_HEADER:
	encoding = 0;
	break;

    case CMS_ENCODE_QUEUING_HEADER:
	encoding = 1;
	break;

    case CMS_DECODE_QUEUING_HEADER:
	encoding = 0;
	break;

    default:
	rcs_print_error("CMS updater in invalid mode.\n");
	return (-1);
    }
    return (0);
}

CMS_UPDATER_MODE CMS_UPDATER::get_mode()
{
    return mode;
}

int CMS_UPDATER::check_pointer(char *_pointer, long _bytes)
{
    if (NULL == cms_parent) {
	return (-1);
    }
    return (cms_parent->check_pointer(_pointer, _bytes));
}

void CMS_UPDATER::set_encoded_data(void *_encoded_data,
    long _encoded_data_size)
{
    if (NULL != encoded_data &&
	!using_external_encoded_data && encoded_data != _encoded_data) {
	free(encoded_data);
	encoded_data = NULL;
    }
    encoded_data = _encoded_data;
    encoded_data_size = _encoded_data_size;
    using_external_encoded_data = 1;
}
