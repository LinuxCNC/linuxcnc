/********************************************************************
* Description: extintf.h
*   Declarations of external interfaces to encoders, DACs, limit
*   switches, etc. These functions are expected to be provided by 
*   board implementations. Query functions give range of axes supported,
*   encoder model, etc.
*   Not all functions are presently used.
*
* Author: 
* License:
* Created at: Mon Feb 23 11:41:51 UTC 2004
* Computer: Babylon
* System: Linux
*    
* Copyright (c) 2004 root  All rights reserved.
*
* Last change: 
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef EXTINTF_H
#define EXTINTF_H

#ifdef __cplusplus
extern "C" {
#endif

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
  are four models (at least) for how incremental encoders can be homed
  off the index pulse.

  EXT_ENCODER_INDEX_MODEL_RAW
  indicates that the index pulse is not latchable, so you have to
  poll the index input and read the encoder value while the index input
  is active. This is the least accurate method due to the latency between
  detecting the index pulse and reading the position value. If the axis
  speed is high enough, the index pulse may be missed entirely if the
  polling rate is too slow.

  EXT_ENCODER_INDEX_MODEL_MANUAL
  indicates that the index pulse sets a latch flag, but you have to
  read this and then the encoder value and handle offsets yourself.
  The encoder won't latch or change its count value on the index pulse.
  This method still suffers from the latency problem, but latching the
  encoder pulse ensures that it won't be missed.

  EXT_ENCODER_INDEX_MODEL_AUTO
  indicates that the index pulse zeros the encoder count automatically.
  in which the occurrence of the index pulse resets the count automatically.

  The problem is that the controller logic is different for each method, so
  if you replace an encoder or interface board which does it one way with
  one that does it another way, you need to recode the logic. The function
  "extEncoderIndexModel()" returns the models supported by the installed
  interface, so at least you can detect it and if it supports multiple models
  you can select the most appropriate one.
*/

/* flags defined bit-exclusive for OR'ing if encoder can do multiple ways */
#define EXT_ENCODER_INDEX_MODEL_RAW    0x00000000
#define EXT_ENCODER_INDEX_MODEL_MANUAL 0x00000001
#define EXT_ENCODER_INDEX_MODEL_AUTO   0x00000002

    extern unsigned int extEncoderIndexModel(void);

/*
  extEncoderSetIndexModel(unsigned int model)
  For interfaces that support multiple index models, select which one
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

/* Probe Functions */
/*
  Each axis has a probe input pin that can be linked to a HAL pin. The
  signal type is "float" to allow the creation of probes that output not
  just "tripped" or "not-tripped", but actual deflection values. This
  should allow future software to produce better surface models by using
  probe data more sophisticated than a simple point cloud.
  For now, only extProbeCheck() is called, and simple switch type probe
  drivers can set their "probe" output pin to any non-zero value to indicate
  "tripped", and to 0.0 to indicate "not-tripped". The probe driver's "probe"
  output pin (must be type hal_float_t) can be linked to any of the "probe"
  input pins on the HAL motion component.
*/

/*
  int extProbeCheck(void)
  Sets flag to 1 if any probe input is tripped, 0 if not.
*/
    extern int extProbeCheck(int *flag);

/*
  int extProbeRead(int axis, double * counts)
  Stores the probe value in counts arg. Returns 0 if
  OK or -1 if encoder is out of range. axis is in range
  0 .. max axis - 1.
*/
    extern int extProbeRead(int axis, double *counts);

/*
  int extProbeReadAll(int max, double * counts)
  Stores the range of probe values in counts array. Returns 0 if
  OK or -1 if the max is greater than number of axes. max is
  number of axes, so for axes 0..3 max would be 4.
*/
    extern int extProbeReadAll(int max, double *counts);

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

/* writes a byte of IO synched with motion start/end */
    extern int extMotDout(unsigned char byte);

#ifdef __cplusplus
}
#endif
#endif				/* EXTINTF_H */
