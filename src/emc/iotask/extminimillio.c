/*
  extmmio.c

  Dispatcher of external functions for NIST minimill

  Modification history:

  7-Aug-1998  FMP changed extInit/Quit to extDio/AioInit/Quit()
  2-Apr-1998  FMP used sim implementation since can't share STG with
  RT motion system
  1-Apr-1998  FMP created
  */

#include "extintf.h"            /* these decls */
#include "sim.h"                /* decls for sim implementation */

/* ident tag */
/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

int extDioInit(const char * stuff)
{
  return simDioInit(stuff);
}

int extDioQuit()
{
  return simDioQuit();
}

int extDioMaxInputs()
{
  return simDioMaxInputs();
}

int extDioMaxOutputs()
{
  return simDioMaxOutputs();
}

int extDioRead(int index, int *value)
{
  return simDioRead(index, value);
}

int extDioWrite(int index, int value)
{
  return simDioWrite(index, value);
}

int extDioCheck(int index, int *value)
{
  return simDioCheck(index, value);
}

int extDioByteRead(int index, unsigned char *byte)
{
  return simDioByteRead(index, byte);
}

int extDioShortRead(int index, unsigned short *sh)
{
  return simDioShortRead(index, sh);
}

int extDioWordRead(int index, unsigned int *word)
{
  return simDioWordRead(index, word);
}

int extDioByteWrite(int index, unsigned char byte)
{
  return simDioByteWrite(index, byte);
}

int extDioShortWrite(int index, unsigned short sh)
{
  return simDioShortWrite(index, sh);
}

int extDioWordWrite(int index, unsigned int word)
{
  return simDioWordWrite(index, word);
}

int extDioByteCheck(int index, unsigned char *byte)
{
  return simDioByteCheck(index, byte);
}

int extDioShortCheck(int index, unsigned short *sh)
{
  return simDioShortCheck(index, sh);
}

int extDioWordCheck(int index, unsigned int *word)
{
  return simDioWordCheck(index, word);
}

int extAioInit(const char * stuff)
{
  return simAioInit(stuff);
}

int extAioQuit()
{
  return simAioQuit();
}

int extAioMaxInputs()
{
  return simAioMaxInputs();
}

int extAioMaxOutputs()
{
  return simAioMaxOutputs();
}

int extAioRead(int index, double *volts)
{
  return simAioRead(index, volts);
}

int extAioWrite(int index, double volts)
{
  return simAioWrite(index, volts);
}

int extAioCheck(int index, double *volts)
{
  return simAioCheck(index, volts);
}
