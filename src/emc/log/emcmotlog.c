/*
  emcmotlog.c

  Definitions for EMC data logging functions

  Modification history:

  20-Mar-2000 WPS added unused attribute to ident to avoid 'defined but not used' compiler warning.
  22-Dec-1997  FMP created
  */

#include "emcmotlog.h"

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

/* ident tag */
static char __attribute__((unused))  ident[] = "$Id$";

int emcmotLogInit(EMCMOT_LOG *log, int type, int size)
{
  log->type = type;
  log->size = size;
  log->start = 0;
  log->end = 0;
  log->howmany = 0;

  return 0;
}

int emcmotLogAdd(EMCMOT_LOG *log, EMCMOT_LOG_STRUCT val)
{
  log->log[log->end] = val;

  log->end++;
  if (log->end >= log->size)
    {
      log->end = 0;
    }

  log->howmany++;
  if (log->howmany > log->size)
    {
      log->howmany = log->size;
      log->start++;
      if (log->start >= log->size)
        {
          log->start = 0;
        }
    }

  return 0;
}

int emcmotLogGet(EMCMOT_LOG *log, EMCMOT_LOG_STRUCT *val)
{
  if (log->howmany == 0)
    {
      return -1;
    }

  *val = log->log[log->start];
  log->start++;
  if (log->start >= log->size)
    {
      log->start = 0;
    }

  log->howmany--;

  return 0;
}
