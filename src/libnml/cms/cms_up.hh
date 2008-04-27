/********************************************************************
* Description: cms_up.hh
*   This C++ header file defines the abstract CMS_UPDATER class
*   that defines the interface used by CMS to convert between local
*   machine-specific data representations and network machine-independant
*   represantations such as XDR via the derived classes of CMS_UPDATER.
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

#ifndef CMS_UP_HH
#define CMS_UP_HH

#include "cms.hh"		/* enum CMS_STATUS, class CMS */

enum CMS_UPDATER_MODE {
    CMS_NO_UPDATE = 0,
    CMS_ENCODE_DATA,
    CMS_DECODE_DATA,
    CMS_ENCODE_HEADER,
    CMS_DECODE_HEADER,
    CMS_ENCODE_QUEUING_HEADER,
    CMS_DECODE_QUEUING_HEADER
};

struct CMS_POINTER_TABLE_ENTRY {
    void *ptr;
};

/* Abstract CMS_UPDATER CLASS */
class CMS_UPDATER {
  public:
    virtual CMS_STATUS update(bool &x) = 0;
    virtual CMS_STATUS update(char &x) = 0;
    virtual CMS_STATUS update(unsigned char &x) = 0;
    virtual CMS_STATUS update(short int &x) = 0;
    virtual CMS_STATUS update(unsigned short int &x) = 0;
    virtual CMS_STATUS update(int &x) = 0;
    virtual CMS_STATUS update(unsigned int &x) = 0;
    virtual CMS_STATUS update(long int &x) = 0;
    virtual CMS_STATUS update(unsigned long int &x) = 0;
    virtual CMS_STATUS update(float &x) = 0;
    virtual CMS_STATUS update(double &x) = 0;
    virtual CMS_STATUS update(long double &x) = 0;
    virtual CMS_STATUS update(char *x, unsigned int len) = 0;
    virtual CMS_STATUS update(unsigned char *x, unsigned int len) = 0;
    virtual CMS_STATUS update(short *x, unsigned int len) = 0;
    virtual CMS_STATUS update(unsigned short *x, unsigned int len) = 0;
    virtual CMS_STATUS update(int *x, unsigned int len) = 0;
    virtual CMS_STATUS update(unsigned int *x, unsigned int len) = 0;
    virtual CMS_STATUS update(long *x, unsigned int len) = 0;
    virtual CMS_STATUS update(unsigned long *x, unsigned int len) = 0;
    virtual CMS_STATUS update(float *x, unsigned int len) = 0;
    virtual CMS_STATUS update(double *x, unsigned int len) = 0;
    virtual CMS_STATUS update(long double *x, unsigned int len) = 0;
    /* Neutrally Encoded Buffer positioning functions. */
    virtual void rewind();	/* positions at beginning */
    virtual int get_encoded_msg_size() = 0;	/* Store last position in
						   header.size */
    virtual int set_mode(CMS_UPDATER_MODE);
    virtual CMS_UPDATER_MODE get_mode();
    virtual void set_encoded_data(void *, long _encoded_data_size);

  protected:

  /**********************************************
  * Aliases to variables in the CMS parent
  * using aliases lets CMS and its CMS_UPDATER share this information
  * more conveniently and allowed the CMS_UPDATER functions to be pulled out
  * of CMS with fewer changes.
  *********************************************/
    void *(&encoded_data);	/* pointer to local copy of encoded data */
    void *(&encoded_header);	/* pointer to local copy of encoded header */
    void *(&encoded_queuing_header);	/* pointer to local copy of encoded
					   queue info */
      CMS_STATUS & status;
    long &size;
    long &encoded_header_size;	/* Dynamically determined size */
    long &encoded_queuing_header_size;	/* Dynamically determined size */
    int &using_external_encoded_data;
    int &pointer_check_disabled;
    long &encoded_data_size;

    virtual int check_pointer(char *ptr, long bytes);
    CMS_UPDATER_MODE mode;
    CMS *cms_parent;

      CMS_UPDATER(CMS *, int create_encoded_data =
	1, long _neutral_size_factor = 4);
      virtual ~ CMS_UPDATER();
    long neutral_size_factor;

    int encoding;

    /* Friends */
    friend class CMS;
};

#endif /* !defined( CMS_UP_HH ) */
