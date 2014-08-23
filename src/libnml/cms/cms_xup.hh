/********************************************************************
* Description: cms_xup.hh
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

#ifndef CMS_XUP_HH
#define CMS_XUP_HH

extern "C" {
#include <rpc/rpc.h>		/* struct XDR */

}
#include "cms_up.hh"		/* class CMS_UPDATER */
class CMS_XDR_UPDATER:public CMS_UPDATER {
  public:
    CMS_STATUS update(bool &x);
    CMS_STATUS update(char &x);
    CMS_STATUS update(unsigned char &x);
    CMS_STATUS update(short int &x);
    CMS_STATUS update(unsigned short int &x);
    CMS_STATUS update(int &x);
    CMS_STATUS update(unsigned int &x);
    CMS_STATUS update(long int &x);
    CMS_STATUS update(unsigned long int &x);
    CMS_STATUS update(float &x);
    CMS_STATUS update(double &x);
    CMS_STATUS update(long double &x);
    CMS_STATUS update(char *x, unsigned int len);
    CMS_STATUS update(unsigned char *x, unsigned int len);
    CMS_STATUS update(short *x, unsigned int len);
    CMS_STATUS update(unsigned short *x, unsigned int len);
    CMS_STATUS update(int *x, unsigned int len);
    CMS_STATUS update(unsigned int *x, unsigned int len);
    CMS_STATUS update(long *x, unsigned int len);
    CMS_STATUS update(unsigned long *x, unsigned int len);
    CMS_STATUS update(float *x, unsigned int len);
    CMS_STATUS update(double *x, unsigned int len);
    CMS_STATUS update(long double *x, unsigned int len);
    int set_mode(CMS_UPDATER_MODE);
    void rewind();
    int get_encoded_msg_size();
    void set_encoded_data(void *, long _encoded_data_size);
  protected:
    int check_pointer(char *, long);
      CMS_XDR_UPDATER(CMS *);
      virtual ~ CMS_XDR_UPDATER();
    friend class CMS;
    XDR *encode_data_stream;	/* XDR streams for data */
    XDR *decode_data_stream;
    XDR *encode_header_stream;	/* XDR streams for header */
    XDR *decode_header_stream;
    XDR *encode_queuing_header_stream;	/* XDR streams for header */
    XDR *decode_queuing_header_stream;
    XDR *current_stream;
};

#endif
// !defined(CMS_XUP_HH)
