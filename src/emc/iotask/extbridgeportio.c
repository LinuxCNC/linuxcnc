/*
  extshvio.c

  Dispatcher of external DIO functions for estop and general purpose
  DIO using parallel port, linked into 'emcio'. Almost the same
  as extppt.c, except it adds extEstopRead,Write().

  Written for Shaver Engineering Bridgeport mill.

  Modification history:

  7-Aug-1998  FMP changed extInit/Quit to extDio/AioInit/Quit()
  31-Mar-1998  FMP added STG analog IO
  3-Nov-1997 FMP created
  */

#include "extintf.h"            /* these decls */
#include "parport.h"            /* decls for parallel port */

int extDioInit(const char * stuff)
{
  return pptDioInit(stuff);
}

int extDioQuit()
{
  return pptDioQuit();
}

int extDioMaxInputs()
{
  return pptDioMaxInputs();
}

int extDioMaxOutputs()
{
  return pptDioMaxOutputs();
}

int extDioRead(int index, int *value)
{
  return pptDioRead(index, value);
}

int extDioWrite(int index, int value)
{
  return pptDioWrite(index, value);
}

int extDioCheck(int index, int *value)
{
  return pptDioCheck(index, value);
}

int extDioByteRead(int index, unsigned char *byte)
{
  return pptDioByteRead(index, byte);
}

int extDioShortRead(int index, unsigned short *sh)
{
  return pptDioShortRead(index, sh);
}

int extDioWordRead(int index, unsigned int *word)
{
  return pptDioWordRead(index, word);
}

int extDioByteWrite(int index, unsigned char byte)
{
  return pptDioByteWrite(index, byte);
}

int extDioShortWrite(int index, unsigned short sh)
{
  return pptDioShortWrite(index, sh);
}

int extDioWordWrite(int index, unsigned int word)
{
  return pptDioWordWrite(index, word);
}

int extDioByteCheck(int index, unsigned char *byte)
{
  return pptDioByteCheck(index, byte);
}

int extDioShortCheck(int index, unsigned short *sh)
{
  return pptDioShortCheck(index, sh);
}

int extDioWordCheck(int index, unsigned int *word)
{
  return pptDioWordCheck(index, word);
}

/*
  No analog IO for Shaver machine-- just stub it
  */

int extAioInit(const char * stuff)
{
  return 0;
}

int extAioQuit()
{
  return 0;
}

int extAioMaxInputs()
{
  return 0;
}

int extAioMaxOutputs()
{
  return 0;
}

int extAioRead(int index, double *volts)
{
  *volts = 0.0;

  return 0;
}

int extAioWrite(int index, double volts)
{
  return 0;
}

int extAioCheck(int index, double *volts)
{
  *volts = 0.0;

  return 0;
}
