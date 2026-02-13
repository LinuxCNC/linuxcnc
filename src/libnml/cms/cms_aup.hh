/********************************************************************
* Description: cms_aup.hh
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
#ifndef CMS_AUP_HH
#define CMS_AUP_HH

#include "cms_up.hh"		/* class CMS_UPDATER */

class CMS_ASCII_UPDATER:public CMS_UPDATER {
  public:
    CMS_STATUS update(bool &x);
    CMS_STATUS update(int8_t &x);
    CMS_STATUS update(uint8_t &x);
    CMS_STATUS update(int16_t &x);
    CMS_STATUS update(uint16_t &x);
    CMS_STATUS update(int32_t &x);
    CMS_STATUS update(uint32_t &x);
    CMS_STATUS update(int64_t &x);
    CMS_STATUS update(uint64_t &x);
    CMS_STATUS update(float &x);
    CMS_STATUS update(double &x);
    CMS_STATUS update(long double &x);
    CMS_STATUS update(int8_t *x, unsigned int len);
    CMS_STATUS update(uint8_t *x, unsigned int len);
    CMS_STATUS update(int16_t *x, unsigned int len);
    CMS_STATUS update(uint16_t *x, unsigned int len);
    CMS_STATUS update(int32_t *x, unsigned int len);
    CMS_STATUS update(uint32_t *x, unsigned int len);
    CMS_STATUS update(int64_t *x, unsigned int len);
    CMS_STATUS update(uint64_t *x, unsigned int len);
    CMS_STATUS update(float *x, unsigned int len);
    CMS_STATUS update(double *x, unsigned int len);
    CMS_STATUS update(long double *x, unsigned int len);
    int set_mode(CMS_UPDATER_MODE);
    void rewind();
    int get_encoded_msg_size();
  protected:
    int check_pointer(char *, long);
      CMS_ASCII_UPDATER(CMS *);
      virtual ~ CMS_ASCII_UPDATER();
    friend class CMS;
    char *begin_current_string;
    char *end_current_string;
    long max_length_current_string;
    long length_current_string;
    int warning_count;
    int warning_count_max;
};

#endif
