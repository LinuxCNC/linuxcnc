#ifndef EXTINTF_H
#define EXTINTF_H

/*
  extintf.h

  Declarations of external interfaces to encoders, DACs, limit switches,
  estops, etc. These functions are expected to be provided by board
  implementations. Query functions give range of axes supported, etc.

  Modification history:

  2-Aug-2001  FMP added extAioStart,Wait
  13-Mar-2000 WPS added unused attribute to parport_h to avoid
  'defined but not used' compiler warning, and added (void) to functions
  with no arguments to avoid 'declaration is not a prototype' compiler
  warnings.  7-Aug-1998 FMP changed extInit/Quit() to
  extDio/Aio/MotInit/Quit()
  31-Mar-1998  FMP added analog IO stuff
  25-Nov-1997 FMP changed extLimitSwitchRead to extPos,NegLimit...
  3-Nov-1997   FMP added extEstopWrite()
  24-Oct-1997  FMP added digital I/O stuff
  15-Oct-1997  FMP added extEstopRead()
  01-Aug-1997  FMP changed encoder counts from int to double
  28-Jul-1997  FMP added extLimitSwitchRead()
  20-Jun-1997  FMP created
*/

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) extintf_h[] =
    "$Id$";

#ifdef __cplusplus
extern "C" {
#endif

/* Init/quit functions *//*
   extDioInit(const char * stuff)

   Call once before any of the other digital IO functions are called.
   'stuff' argument can be used to pass board-specific stuff like config
   files.
 */ extern int extDioInit(const char *stuff);

/*
  extDioQuit()

  Call once, after which point no other functions will be called until
  after a call to extDioInit().
  */
    extern int extDioQuit(void);

/*
  extAioInit(const char * stuff)

  Call once before any of the other analog IO functions are called.
  'stuff' argument can be used to pass board-specific stuff like config
  files.
  */
    extern int extAioInit(const char *stuff);

/*
  extAioQuit()

  Call once, after which point no other functions will be called until
  after a call to extAioInit().
  */
    extern int extAioQuit(void);

/*
  extMotInit(const char * stuff)

  Call once before any of the other motion IO functions are called..
*/
    extern int extMotInit(void);

/*
  extMotQuit()

  Call once, after which point no other functions will be called until
  after a call to extMotInit().
  */
    extern int extMotQuit(void);

/* DAC functions */

/*
  extDacNum()

  returns number of DACs in system.
  */
    extern int extDacNum(void);

/*
  extDacWrite(int dac, double volts)

  writes the value to the DAC at indicated dac, 0 .. max DAC - 1.
  Value is in volts. Returns 0 if OK, -1 if dac is out of range.
  */
    extern int extDacWrite(int dac, double volts);

/*
  extDacWriteAll(int max, double * volts)

  writes the values to the DAC range. Values are array of DAC outputs
  in volts. Returns 0 if OK, -1 if not max is out of range. This is
  provided for systems in which DACs can be written all at once for
  speed or synchronicity. max is number of DACs, so for dacs 0..3
  max would be 4.
  */
    extern int extDacWriteAll(int max, double *volts);

/* Encoder functions */

/*
  extEncoderIndexModel()

  Returns the type of encoder indexing done by the board.

  The handling of index pulses for encoder homing is problematic. There
  are two models (at least) for how incremental encoders can be homed
  off the index pulse. The first assumes the encoder index pulse sets
  a flag when it occurs, but the count value doesn't change. The controller
  polls on this flag, and when it is seen to be latched the controller
  reads the position and uses this as an offset. There is a latency that
  makes this less accurate than the second method, in which the occurrence
  of the index pulse resets the count automatically. The problem is that
  the controller logic is different for the two methods, so if you replace
  a board which does it the first way with one that does it the second,
  you need to recode the logic. The function "extEncoderIndexModel()"
  returns the model used by the board, so at least you can detect it
  and if you have coded up both types you can switch automatically.

  EXT_ENCODER_INDEX_MODEL_MANUAL
  indicates that the index pulse sets a latch flag, but you have to
  read this and then the encoder value and handle offsets yourself.
  The board won't change its count value on the index pulse.

  EXT_ENCODER_INDEX_MODEL_AUTO
  indicates that the index pulse zeros the encoder count automatically.
  */

/* flags defined bit-exclusive for OR'ing if board can do multiple ways */
#define EXT_ENCODER_INDEX_MODEL_MANUAL 0x00000001
#define EXT_ENCODER_INDEX_MODEL_AUTO   0x00000002

    extern unsigned int extEncoderIndexModel(void);

/*
  extEncoderSetIndexModel(unsigned int model)

  For boards that support multiple index models, select which one
  is to be used. Returns 0 if OK, -1 if model can't be supported.
  */
    extern int extEncoderSetIndexModel(unsigned int model);

/*
  extEncoderNum()

  returns number of encoders in system.
  */
    extern int extEncoderNum(void);

/*
  extEncoderRead(int encoder, double * counts)

  Stores the encoder's counts in counts arg. Returns 0 if
  OK or -1 if encoder is out of range. encoder is in range
  0 .. max encoder - 1.
  */
    extern int extEncoderRead(int encoder, double *counts);

/*
  extEncoderReadAll(int max, double * counts)

  Stores the range of encoders' counts in counts array. Returns 0 if
  OK or -1 if the max is greater than number of encoders. max is
  number of encoders, so for encoders 0..3 max would be 4.
  */
    extern int extEncoderReadAll(int max, double *counts);

/*
  extEncoderResetIndex(int encoder)

  Resets index latching for the indicated axis. Returns 0 if OK or -1
  if the index is out of range. This applies to both
  EXT_ENCODER_INDEX_MODEL_MANUAL and EXT_ENCODER_INDEX_MODEL_AUTO.
  For the first, it resets the latch flag. For the second, it enables
  zeroing on the next index. encoder is range 0..max encoder - 1.
  */
    extern int extEncoderResetIndex(int encoder);

/*
  extEncoderReadLatch(int encoder, int * flag)

  For EXT_ENCODER_INDEX_MODEL_MANUAL, stores 1 if index has latched
  the flag, 0 if not. For EXT_ENCODER_INDEX_MODEL_AUTO, stores 1 if
  the encoder has been zeroed. Returns 0 if OK, -1 if not valid.
  */
    extern int extEncoderReadLatch(int encoder, int *flag);

/*
  extEncoderReadLevel(int encoder, int * index)

  For EXT_ENCODER_INDEX_MODEL_MANUAL, stores 1 if encoder is on
  the index pulse right now, 0 if not. Useful for slow polling
  to get more accuracy since the manual model has latency.

  Not valid for EXT_ENCODER_INDEX_MODEL_AUTO. Returns 0 if OK,
  -1 if not valid.
  */
    extern int extEncoderReadLevel(int encoder, int *flag);

/* Limit switch functions */

/*
  extMaxLimitSwitchRead(int axis, int * flag)
  extMinLimitSwitchRead(int axis, int * flag)

  sets *flag to 0 if the limit switch is not tripped, i.e., everything
  is fine, 1 if the limit switch is tripped, i.e., the axis went
  too far in the associated direction.

  Maximum is defined as the direction in which encoder values increase,
  minimum is the other direction.

  Returns 0 if OK, -1 if not valid (axis is out of range).
  */
    extern int extMaxLimitSwitchRead(int axis, int *flag);
    extern int extMinLimitSwitchRead(int axis, int *flag);

/*
  extHomeSwitchRead(int axis, int * flag)

  sets *flag to 0 if the home switch is not tripped, i.e., everything
  is fine, 1 if the home switch is tripped.

  Returns 0 if OK, -1 if not valid (axis is out of range).
  */
    extern int extHomeSwitchRead(int axis, int *flag);

/* Amp functions */

/*
   extAmpEnable(int axis, int enable)

   enables or disables amplifier for indicated axis; enable flag is
   1 to enable, 0 to disable

   Returns 0 if OK, -1 if not valid (axis is out of range)
   */
    extern int extAmpEnable(int axis, int enable);

/*
  extAmpFault(int axis, int * fault)

  Copies into 'fault' the fault state of the amplifier. 1 is faulted,
  0 is OK.

  Returns 0 if OK, -1 if not valid (axis out of range)
  */
    extern int extAmpFault(int axis, int *fault);

/*
  Digital I/O model

  If you need to call board-specific code to set up the analog I/O
  registers, do it in extInit().

  "index" begins at 0.

  Code for each implementation should shift index up into appropriate
  register so that it matches initialization and R/W setup in extInit().

  Returns 0 if OK, -1 if error (invalid index).
  */

/* returns the max input index, output index; max bytes, shorts, and words */
    extern int extDioMaxInputs(void);	/* index < this for extDioRead() */
    extern int extDioMaxOutputs(void);	/* index < this for
					   extDioWrite(),Check() */

/* reads value of digital input at index, stores in value */
    extern int extDioRead(int index, int *value);

/* writes value (non-zero means 1, 0 is 0) at digital out at index */
    extern int extDioWrite(int index, int value);

/* reads value of digital OUT at index, stores in value. Useful
   for checking values of previous writes. Returns 0 if OK, -1 if
   bad index or can't read if they're write-only. */
    extern int extDioCheck(int index, int *value);

/* byte, short, and word reads, writes. Index starts at 0, indexes up
   through bytes, short ints, and ints */
    extern int extDioByteRead(int index, unsigned char *byte);
    extern int extDioShortRead(int index, unsigned short *sh);
    extern int extDioWordRead(int index, unsigned int *word);
    extern int extDioByteWrite(int index, unsigned char byte);
    extern int extDioShortWrite(int index, unsigned short sh);
    extern int extDioWordWrite(int index, unsigned int word);
    extern int extDioByteCheck(int index, unsigned char *byte);
    extern int extDioShortCheck(int index, unsigned short *sh);
    extern int extDioWordCheck(int index, unsigned int *word);

    /* 
       Analog I/O model

       Analog I/O presumes units of volts. In your implementations of these
       functions you need to linearize as appropriate.

       If you need to call board-specific code to set up the analog I/O
       registers, do it in extInit().

       "index" begins at 0.

       Code for each implementation should shift index up into appropriate
       register so that it matches initialization and R/W setup in extInit().

       Returns 0 if OK, -1 if error (invalid index). */

    /* returns the max input index, output index */
    extern int extAioMaxInputs(void);	/* index < this for extAioRead() */
    extern int extAioMaxOutputs(void);	/* index < this for
					   extAioWrite(),Check() */

/* starts an analog input conversion */
    extern int extAioStart(int index);

/* waits for conversion */
    extern void extAioWait(void);

/* reads value of analog input at index, stores in volts */
    extern int extAioRead(int index, double *value);

/* writes value in volts to analog out at index */
    extern int extAioWrite(int index, double volts);

/* reads value of analog OUT at index, stores in volts. Useful
   for checking values of previous writes. Returns 0 if OK, -1 if
   bad index or can't read if they're write-only. */
    extern int extAioCheck(int index, double *volts);

/* writes a byte of IO synched with motion start/end */
    extern int extMotDout(unsigned char byte);

#ifdef __cplusplus
}
#endif
#endif				/* EXTINTF_H */
