/********************************************************************
* Description: exthalmot.c
*   Dispatcher of external motion functions for HAL drivers.
*   This should be the only point of contact between emc and HAL.
*
* Author:
* Created at:
* Computer:
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
/*
  exthalmot.c

  Modification history:

  01-Feb-2004  MGS created from extstgmot.c..
  */

#include "extintf.h"            /* these decls */
#include "hal.h"                /* decls for HAL implementation */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused))  ident[] = "";

int extMotInit(const char * stuff)
{
  return 0;
}

int extMotCycle(const char * stuff)
{
  return 0;
}

int extMotQuit(void)
{
  return 0;
}

int extDacNum(void)
{
  return 0;
}

int extDacWrite(int dac, double volts)
{
  return 0;
}

int extDacWriteAll(int max, double * volts)
{
  return 0;
}

int extEncoderSetIndexModel(unsigned int model)
{
  return 0;
}

int extEncoderNum(void)
{
  return 0;
}

int extEncoderRead(int encoder, double * counts)
{
  return 0;
}

int extEncoderReadAll(int max, double * counts)
{
  return 0;
}

int extEncoderResetIndex(int encoder)
{
  return 0;
}

int extEncoderReadLatch(int encoder, int * flag)
{
  return 0;
}

int extEncoderReadLevel(int encoder, int * flag)
{
  return 0;
}

int extMaxLimitSwitchRead(int axis, int * flag)
{
  return 0;
}

int extMinLimitSwitchRead(int axis, int * flag)
{
  return 0;
}

int extHomeSwitchRead(int axis, int * flag)
{
  return 0;
}

int extAmpEnable(int axis, int enable)
{
  return 0;
}

int extAmpFault(int axis, int * fault)
{
  return 0;
}

int extDioInit(const char * stuff)
{
  return 0;
}

int extDioQuit(void)
{
  return 0;
}

int extDioMaxInputs(void)
{
  return 0;
}

int extDioMaxOutputs(void)
{
  return 0;
}

int extDioRead(int index, int *value)
{
  return 0;
}

int extDioWrite(int index, int value)
{
  return 0;
}

int extDioCheck(int index, int *value)
{
  return 0;
}

int extDioByteRead(int index, unsigned char *byte)
{
  return 0;
}

int extDioShortRead(int index, unsigned short *sh)
{
  return 0;
}

int extDioWordRead(int index, unsigned int *word)
{
  return 0;
}

int extDioByteWrite(int index, unsigned char byte)
{
  return 0;
}

int extDioShortWrite(int index, unsigned short sh)
{
  return 0;
}

int extDioWordWrite(int index, unsigned int word)
{
  return 0;
}

int extDioByteCheck(int index, unsigned char *byte)
{
  return 0;
}

int extDioShortCheck(int index, unsigned short *sh)
{
  return 0;
}

int extDioWordCheck(int index, unsigned int *word)
{
  return 0;
}

int extAioInit(const char * stuff)
{
  return 0;
}

int extAioQuit(void)
{
  return 0;
}

int extAioMaxInputs(void)
{
  return 0;
}

int extAioMaxOutputs(void)
{
  return 0;
}

int extAioStart(int index)
{
  return 0;
}

void extAioWait(void)
{
  return;
}

int extAioRead(int index, double *volts)
{
  return 0;
}

int extAioWrite(int index, double volts)
{
  return 0;
}

int extAioCheck(int index, double *volts)
{
  return 0;
}
