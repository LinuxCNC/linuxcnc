#ifndef EMCMOTCFG_H
#define EMCMOTCFG_H

/*
  emcmotcfg.h

  Default values for compile-time parameters, used to initialize
  global variables in emcmotcfg.c.

  Modification history:
  
  9-Nov-2002 P.C. Increased EMCMOT_MAX_AXIS to 8 to allow for spindle
  control using a spare DAC.
  13-Mar-2000 WPS added unused attribute to emcmotcfg_h to avoid 
  'defined but not used' compiler warning.
  21-Jul-1999  FMP changed EMCMOT_MAX_AXIS to 6, in support of robot
  and hexapod kinematics. Note that re-making doesn't work, since there
  is a dependency somewhere that isn't captured. You need to do a
  'make PLAT=whatever clean headers depend all', or a ./compile in the
  nist directory, to get everything compiled.
  15-Jun-1999  FMP changed DEFAULT_VELOCITY, DEFAULT_ACCELERATION to
  1 and 10, respectively.
  8-Jun-1999  FMP removed DEFAULT_SM_CYCLE_TIME, since the stepper motor
  cycle time is now configured as twice the axis rate in the ini file.
  8-Jun-1999  RSB changed default queue size to 200
  18-Aug-1998  FMP added DEFAULT_BIAS, DEFAULT_MAX_ERROR
  6-Aug-1998  FMP added DEFAULT_SM_CYCLE_TIME decl
  8-Jul-1998  FMP added DEFAULT_EMCMOT_INIFILE
  6-Jul-1998  FMP changed BASE_ADDRESS to SHMEM_BASE_ADDRESS
  5-Jun-1998  FMP added DEFAULT_BACKLASH
  28-May-1998  FMP changed DEFAULT_TRAJ,SERVO_CYCLE_TIME to 10 ms, 1 ms
  respectively, since any faster may lead to indefinite postponements
  due to split reads
  1-Apr-1998  FMP added DEFAULT_EMCMOT_COMM_TIMEOUT,WAIT
  6-Jan-1998  FMP added PID gains, input/output offsets, default base address
  16-Oct-1997  FMP created
  */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) emcmotcfg_h[] = "$Id$";

/* default name of EMCMOT ini file */
#define DEFAULT_EMCMOT_INIFILE "emc.ini"/* same as for EMC-- we're in touch */

/* number of axes supported
   Note: this is not a global variable but a compile-time parameter
   since it sets array sizes, etc. */
#define EMCMOT_MAX_AXIS 8

#define EMCMOT_ERROR_NUM 32     /* how many errors we can queue */
#define EMCMOT_ERROR_LEN 256    /* how long error string can be */

/*
  base address-- make sure that

  DEFAULT_SHMEM_BASE_ADDRESS + sizeof(EMCMOT_STRUCT) < physical memory

  Examples:

  On a 32 MB computer, with sizeof(EMCMOT_STRUCT) = 241492,
  let's set aside 1 MB, so DEFAULT_SHMEM_BASE_ADDRESS is 31 MB:
  #define DEFAULT_SHMEM_BASE_ADDRESS = (31*0x100000)
  To set up Linux for this, put append="mem=31m" in lilo.conf.

  On a 64 MB computer, with sizeof(EMCMOT_STRUCT) = 1290068,
  let's set aside 2 MB, so DEFAULT_SHMEM_BASE_ADDRESS is 62 MB:
  #define DEFAULT_SHMEM_BASE_ADDRESS = (62*0x100000)
  To set up Linux for this, put append="mem=62m" in lilo.conf.
  */

/* base address for RTLINUX shared memory */
// #define DEFAULT_SHMEM_BASE_ADDRESS (31*0x100000)

/*
  Shared memory keys for simulated motion process. No base address
  values need to be computed, since operating system does this for us
  */
#define DEFAULT_SHMEM_KEY 100

/* default comm timeout, in seconds */
#define DEFAULT_EMCMOT_COMM_TIMEOUT 1.0
/* seconds to delay between comm retries */
#define DEFAULT_EMCMOT_COMM_WAIT 0.010

/* default cycle time for trajectory calculations; cycle time
   for emcmot.c main loop will be this times the interpolation rate */
#ifdef realtime
#define DEFAULT_TRAJ_CYCLE_TIME  0.010
#define DEFAULT_SERVO_CYCLE_TIME 0.001
#else
#define DEFAULT_TRAJ_CYCLE_TIME  0.200
#define DEFAULT_SERVO_CYCLE_TIME 0.020
#endif

/* initial velocity, accel used for coordinated moves */
#define DEFAULT_VELOCITY 1.0
#define DEFAULT_ACCELERATION 10.0

/* maximum and minimum limit defaults for all axes */
#define DEFAULT_MAX_LIMIT 1000
#define DEFAULT_MIN_LIMIT -1000

/* output clamps before scaling */
#define DEFAULT_MAX_OUTPUT 10.0
#define DEFAULT_MIN_OUTPUT -10.0

/* size of motion queue */
#define DEFAULT_TC_QUEUE_SIZE 200

/* size of window for averages */
#define DEFAULT_MMXAVG_SIZE 100

/* max following error */
#define DEFAULT_MAX_FERROR 100

/* PID gains */
#define DEFAULT_P_GAIN 0.0
#define DEFAULT_I_GAIN 0.0
#define DEFAULT_D_GAIN 0.0
#define DEFAULT_FF0_GAIN 0.0
#define DEFAULT_FF1_GAIN 0.0
#define DEFAULT_FF2_GAIN 0.0
#define DEFAULT_BACKLASH 0.0
#define DEFAULT_BIAS 0.0
#define DEFAULT_MAX_ERROR 0.0

/* input, output scales */
#define DEFAULT_INPUT_SCALE 1.0
#define DEFAULT_INPUT_OFFSET 0.0
#define DEFAULT_OUTPUT_SCALE 1.0
#define DEFAULT_OUTPUT_OFFSET 0.0

#endif
