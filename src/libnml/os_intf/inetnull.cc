/********************************************************************
* Description: inetnull.cc
*   This file provides a set of functions to read from a local
*   password file
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

#include "inetfile.hh"
#include <stdio.h>

class INET_FILE {
  public:
    FILE * fp;
};

int inet_file_init(const char *agent_name, char *agent_version, int debug)
{
    return 0;
}

INET_FILE *inet_file_open(const char *url, char *type)
{

    FILE *fp = NULL;
    INET_FILE *inet_file = NULL;

    fp = fopen(url, type);
    if (NULL == fp) {
	return NULL;
    }
    inet_file = new INET_FILE;
    if (NULL != inet_file) {
	inet_file->fp = fp;
    }
    return inet_file;
}

char *inet_file_gets(char *str, int maxlen, INET_FILE * inet_file)
{
    if (NULL == inet_file) {
	return NULL;
    }
    return fgets(str, maxlen, inet_file->fp);
}

int inet_file_close(INET_FILE * inet_file)
{
    if (NULL != inet_file) {
	fclose(inet_file->fp);
	delete inet_file;
    }
    return 0;
}

int inet_file_eof(INET_FILE * inet_file)
{
    if (NULL == inet_file) {
	return 1;
    }
    return feof(inet_file->fp);
}

int inet_file_exit()
{
    return 0;
}

int inet_file_rewind(INET_FILE * ifp)
{
    if (NULL == ifp) {
	return -1;
    }
    if (ifp->fp != NULL) {
	rewind(ifp->fp);
	return 0;
    }
    return 0;
}
