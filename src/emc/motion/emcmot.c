#define _XOPEN_SOURCE
/*
  emcmot.c

  Real-time motion controller for queued 3-axis motion planning, independent
  axis motion, cubic subinterpolation, and stepper or servo control.

  Modification history:
  9-Sep-2003  FMP took out rt_mount/unmount_rtai(), which should not be done
  by application modules
  18-Jul-2003 MGS rewrote freqfunc, added EMC_AXIS_SET_STEP_PARAMS, and
  removed binfunc. Each axis should have a SETUP_TIME and HOLD_TIME entry
  in the .ini file.
  11-Jul-2003 MGS deleted unreachable code that was shadowed by an #ifdef/#else.
  Also, moved an incorrectly placed #endif.
  10-Jun-2003 WPS -- disable the debug stuff by default since it might use too
  much of the CPU on slow CPU's or stepper systems with a lot of pulses to put out.
  29-Jan-2003 WPS changed call to simMotInit to extMotCycle
  10-Dec-2002 WPS eliminated use of direct time stamp measurements, apparently
  the linux kernel no longer exports cpu_khz  for modules to use although
  which is needed for direct time stamp measurments unless we want to do our own
  calibration or add an EMC specific kernel patch to export it.
  (The variable is still there and computed) 
  9-Dec-2002 WPS made stepping type 4 use oneshot mode and set its period 
  just as emcstepmot.c (steppermod) does, also adds an extra warning
  if the timing goes way out of sync a cycle to cycle time 10 times
  what it should be. Adds MODULE_LICENSE.
  16-Nov-2002 P.C. Made high byte writes of stepper output conditional on
  th number of axes - This allows the same port to be used for IO as well.
  15-Nov-2002  FMP changed code in SCALE OUTPUTS section for steppers, so
  that zero rawOutput wouldn't disable stepping for the binning algo.
  9-Nov-2002 P.C. Included DIO set/reset command to emcmotCommandHandler.
  25-Sep-2002 P.C. Merged Jon Elson's ppmc code.
  22-Sep-2002 P.C. tnb_fourphase_drive modified to work with rtai.
  3-Sep-2002  FMP added hiByte axes to binning algorithm, which were
  surprisingly never there
  13-Aug-2002 WPS added stepping type 4 to use the binning algorithm from 
  emcstepmot.c for step/dir outputs 
  15-July-2002 P.C. Moved the hardware init stuff around so that in the
  event of failure, shared memory isn't left hanging around on the abort.
  31-May-2002 P.C Added DRO_BASE_ADDRESS support for the Kaluga/Mauch DRO
  card. This defaults to 0x200, but the parameter can be passed on loading.
  15-May-2002 P.C. Changed PARPORT_BASE_ADDRESS and STG_BASE_ADDRESS to a
  more generic IO_BASE_ADDRESS.
  13-May-2002  FMP ifdef'ed out positionInputDebounce[] so that it's not
  defined with SIMULATED_MOTORS, to get rid of an unused variable warning
  3-Apr-2002 WPS committed the fourphase stepping stuff, (it is completely
  untested but atleast you have to go out of ones way to set the 
  STEPPING_TYPE for it to affect) It contains code from Lawrance Glaister
  based on a March 2001 version of EMC, I got the code indirectly
  through Hassan S. Rabe.
  14-Dec-2001  FMP set coarseJointPos[] to be the homed position at the
  instant it homes. This was left out and caused a one-cycle setting of
  the limit bit and an annoying error popup.
  12-Dec-2001 FMP fixed calculation of continuous jog goal position when
  not homed, to be "here" plus/minus the axis range. Before it was just
  the axis range, and in cases where the raw position was outside joint
  limits, this could case motion only in one direction (toward the good
  range).
  27-Aug-2001  PC added a few headers to the rtai section.
  17-Aug-2001  FMP added EMCMOT_SET_AOUT,DOUT stuff
  15-Aug-2001  FMP added EMCMOT_SET_AXIS_VEL_LIMIT to set per-axis vel
  limits, and made bigVel an array, for each axis
  8-Aug-2001  FMP reversed PERIOD *= reperiod and PERIOD /= resperiod
  statements so that they do the intended quantization of PERIOD to resperiod.
  Prior to this reversal they had no effect.
  28-Jun-2001  FMP added logStartTime as base for logged times, so that
  the values are small enough that the increments show up
  1-Jun-2001  FMP added EMCMOT_SET_DEBUG handling as config info. We don't
  reference this now, but we can do it in the future. Note that we can't
  printk anything, so we need to do something else, like write to the
  error log or something.
  31-May-2001  FMP took out #define EMCMOT_C, EMCSTEPMOT_C that was
  used to ifdef the init_module, cleanup_module declarations in emcmot.h,
  since these functions are only used here.
  25-May-2001 WPS report an error when checkJog returns 0.
  21-May-2001  FMP took out FIFO stuff, since it's no longer used
  18-May-2001 WPS moved alot of stuff into the emcmotDebug structure
  which is copied into shared memory. The Task level doesn't use this
  but it will hopefully make debugging some problems easier.
  17-May-2001  FMP changed handling of transitions into teleop mode, clearing
  the homed flags for axes that have moved outside of coordinated mode
  for machines with KINEMATICS_INVERSE_ONLY
  15-May-2001  FMP added handling of EMCMOT_NO_FORWARD_KINEMATICS with
  kinematicsType(), using kinType as the master. Now, kinType is set via
  a call to kinematicsType() at the beginning of init_module. kinType is then
  reset to KINEMATICS_INVERSE_ONLY if EMCMOT_NO_FORWARD_KINEMATICS is set.
  This lets you override the kinematics and force out calls to the forward
  kinematics, and is backward-compatible with scripts that set this as
  an insmod parameter.
  11-Apr-2001 MAX added support for linear encoders.
  13-May-2001 KJO moved ifdefed stuff below comment block.
  16-Jan-2001 WPS added teleop mode stuff. This mode is intended to be
  used for joystick control of non-slideways machines. Like coordinated
  mode it accepts commands with velocities in world coordinates, like
  free mode it is intended for manual control. It bypasses the
  trajectory planner and never queues motions.
  13-Jan-2001 LPG fixed bug in phase drive pdmult[] calc that caused estop on
                  moves with very high gain / high motion errors
  05-Nov-2000 LPG added alternate phase drive system, fixed original phase
                  drive routine, changed default PERIOD to avoid locking up
		  slower boxes
  24-Oct-2000 terrylr added define EMCMOT_C to allow for the conditional
  declaration of init_module & cleanup_module in emcmot.h.
  21-Sep-2000  FMP added check against GET_AXIS_ENABLE_FLAG() before
  aborting amp faults. Changed AMP_FAULT_DEBOUNCE from 10 to 100. For
  500 usec servos, this is 50 msecs, and fixes the race condition problem
  between amp enable and a reset of amp fault, in systems where amp fault
  equals not amp enable.
  15-Aug-2000  FMP added alter to comping
  8-Aug-2000  FMP added EMCMOT_COMP stuff
  8-Aug-2000  FMP pulled setting of probing flag into part of code for
  EMCMOT_PROBE that succeeds in initiating a probe, rather than at the
  beginning. Replaced a slash-slash comment with a slash-star.
  27-Jul-2000 WPS added probing commands and status variables.
  15-Jun-2000 WPS changed include asm/io.h to sys/io.h for rtlinux_2_2.
  12-Apr-2000 WPS eliminated redundant second include string.h
  30-Mar-2000 WPS added ferrorCurrent and ferrorHighMark junk.
  20-Mar-2000 WPS removed freqTask time variables that were previously added
  for debugging.
  13-Mar-2000 WPS added unused attribute to ident to avoid
  'defined but not used' compiler warning, and added (void) to many functions
  with no arguments to avoid 'declaration is not a prototype' compiler warning
  13-Mar-2000  FMP added 6 axis support to frequency emcmotDebug->stepping.
  9-Mar-2000  WPS set PERIOD in pre-2.0 rtlinux to convert from nanosecs
  to RT_TIME units.
  6-Mar-2000 WPS eliminated the #include <linux/rt_sched.h> and
  <linux/rt_time.h>, which appeared to be only needed for pre-
  rtlinux_09J platforms and those are unlikely to work anyway.
  3-Mar-2000  FMP changed this file name from emcfreqmot.c to emcmot.c,
  along with changing emcmot.c to emcstepmot.c. This file has thus
  been promoted.
  29-Feb-2000  FMP added backlash compensation blending, and took it out
  of pid.c. This way, it's part of the trajectory planning and will work
  regardless of PID or other compensation. Note that the backlash value
  is part of the PID structure, and should be taken out. This impacts
  NML and the motion interface, etc. so we'll leave it alone for now.
  28-Feb-2000 WPS added #include <rtl.h> and <time.h> so that there would
  be no undefined symbol clock_getres. (This function is inlined in
  /usr/src/rtlinux-2.0/rtl/include/posix/time.h).
  25-Feb-2000  FMP added OUTPUT_DELTA to compare output for steppers, so
  we can disable the frequency generator.
  24-Feb-2000 FMP used extEncoderReadAll() for stepper motors, putting
  the open-loop feedback of accumulated pulses into the ext intf
  wrapper, e.g., extsmmot.c. This is to support using a DRO for stepper
  feedback. Removed forcing of encoder latch for steppers here, and
  called extEncoderReadLatch() which is now coded up in the ext intf
  wrapper to return 1 always. Used output scale instead of input scale
  for stepper motor scale factor.
  16-Feb-2000  FMP fixed WPS's ifdef and capitalization bugs.
  7-Feb-2000 WPS updated it for RT linux 2.0.
  30-Jan-2000  FMP added quantization of computed position about input
  scale, using rounding; added up/down frequency generator code with
  inhibit of clocking when there's a direction change
  22-Jan-2000  FMP added triggering of POS_VOLTAGE log with dac write
  18-Jan-2000  FMP converted to frequency emcmotDebug->stepping, with error feeding
  frequency from PID. Need to figure out how to keep old style.
  20-Dec-1999  FMP restored dual-cycle version of stepper code, from
  1.119 version, which ran best on Bridgeport.
  17-Dec-1999  FMP ran sm task at half, not quarter time, so the cycle
  time would be the same as everybody expects. Note that this is twice
  as fast as it need run, since we're at one full pulse per sm task.
  9-Dec-1999  FMP reversed priorities on motion and stepper tasks, so that
  stepper task had highest priority; only cleared clock bits at start of
  stepper task, leaving direction bits alone.
  7-Dec-1999  FMP changed smController to run at full pulse rate
  3-Dec-1999  FMP added smCyclesPerAxis as configuration variable, that isn't
  recalculated using floating point in smController each cycle. This was to
  speed it up.
  23-Nov-1999  FMP restored last output bytes in smController to preserve
  last values of bits not changed.
  16-Nov-1999  FMP changed the logic in the stepper code so that pulses
  are spread across the whole servo cycle, instead of bunched at the front,
  and the duty cycle is 50-50. Added later fix on smLastUp so that a group
  never finished on an up pulse.
  12-Nov-1999  FMP took negative axis in EMCMOT_OVERRIDE_LIMITS to mean
  don't override limits
  3-Nov-1999  FMP added EMCMOT_LOG_POS_VOLTAGE
  29-Oct-1999  FMP changed rtlinux_b11 to rtlinux2
  22-Oct-1999  FMP added MODULE_PARM() calls for globals
  21-Oct-1999  FMP added rtlinux_b11 port to 2.2 kernels; ioremap(),
  iounmap() calls for 2.2.
  21-Oct-1999  FMP added call to SET_AXIS_HOMED_FLAG(axis, 0) for steppers,
  to clear the homed flag if we went into the disabled state. This is to
  catch the case where steppers aren't on a full pulse when power is taken
  away, which could make the system off by pulse requiring homing again.
  18-Oct-1999  FMP added missing call to tpClear() for each axis' motion
  planner, to fix problem with axes jumping when emcmotDebug->overriding a limit.
  8-Oct-1999  FMP added isHoming() to check for any axis homing, to support
  homing off a limit switch tied into all axes.
  5-Oct-1999  FMP added limitFerror, the speed-dependent following error.
  Now following error depends linearly on speed, so for slower moves the
  fatal following error is lower.
  30-Sep-1999  FMP didn't flag an error or abort on hardware limits when
  homing, and used them as a home condition along with home switch.
  29-Sep-1999  FMP added homeOffset[] to status, EMCMOT_SET_HOME_OFFSET to cmd
  21-Sep-1999  WPS eliminate sscanf and printf calls not supported under CE.
  14-Sep-1999  FMP added emcmotDebug->inverseOutputScale[], like emcmotDebug->inverseInputScale[], to
  save a division at the servo cycle.
  17-Aug-1999  FMP fixed typo from yesterday
  16-Aug-1999  FMP allowed calculation of actualPos if not homed, if we
  have trivial kinematics
  9-Aug-1999  FMP added emcmotDebug->inverseInputScale[] = 1/inputScale[], to convert
  a division into a multiplication to save calc time at the servo rate.
  Added setting of home position(s) to be emcmotDebug->jointHome[], worldHome, from
  EMCMOT_SET_WORLD_HOME, EMCMOT_SET_JOINT_HOME. Revised calls to kinematics
  to use new naming convention "kinematicsInv/For/Home".
  4-Aug-1999  FMP fixed watchdog so that sound port bit is zeroed when
  watchdog is turned off; fixed newly-introduced bug in soft limit checking
  that gave a soft limit when not homed, for negative directions.
  3-Aug-1999  FMP bracketed checks for limit, fault, home switch with
  check that the joint is active; set default for axis to be deactivated
  2-Aug-1999  FMP split out Cartesian and joint motion more explicitly;
  added emcmotDebug->cubicOffset in place of cubicDrain when a joint homed. Reworked
  the way kinematics functions were called, and coord/free mode interacted.
  Added coarseJointPos[] array. Called forward kins at the simulated
  trajectory rate when disabled, via the interpolationCounter var. Fixed
  handling of axes >=4 in smController().
  26-Jul-1999  FMP put 6 axes' worth of bits into stepper motor task
  23-Jul-1999  FMP fixed bug in calculating smUnitsPerStep from 1/input scale,
  in which negative input scales gave negative units per step, which were
  always assumed to be positive in the backlash calcs. Converted backlash
  comp from step-per-cycle application to acc/dec application.
  Suppressed redundant writes of stepper motor byte in smController.
  Added smInhibit flag that causes smController to set command steps
  equal to accum steps, so we won't have stepper runaway.
  22-Jul-1999  FMP added backlash comp for steppers, in which backlash
  takeup was meted out one pulse per servo cycle; fixed homing so that
  there was a controlled move from the post-home decel back to home.
  Previously after the post-home decel the home position was given directly
  to the servos, so it screamed back the full decel distance. This was so
  small that no one noticed.
  14-Jul-1999  FMP set emcmotStatus->qVscale,axVscale[] only when
  EMCMOT_SCALE was read, not every cycle; put head/tail in command handler
  and moved down a bit in controller
  1-Jul-1999  FMP reduced calls to kinematicsForward()
  24-Jun-1999  FMP added vremap(), vfree() calls to RT Linux shared mem;
  changed EMCMOT_NO_MAIN to EMCMOT_MAIN
  16-Jun-1999  FMP added axisPos[] to status
  14-Jun-1999  FMP added #ifdef STEPPER_MOTORS bracketing around some more
  stepper data
  11-Jun-1999  FMP calculated smTask cycle time using T = 1/2 * 1/(V*P),
  where V is max vel, P is pulses per unit. Whenever these are changed,
  the task rate is changed also.
  10-Jun-1999  FMP added magFerror to solve problem that following error
  was only compared in the positive direction against the max
  8-Jun-1999  FMP ran smTask at twice the servo rate, instead of at rate
  configured by now-obsolete SM_CYCLE_TIME. It's twice so the effective
  pulse rate is the same as the servo cycle time, e.g., 100 microsecond
  cycle time means a pulse every 100 microseconds at max speed. Set
  'emcmotDebug->bigVel' with emcmotConfig->limitVel, for upper velocity limit.
  Added EMCMOT_SET_VEL_LIMIT
  7-Jun-1999  FMP fixed logging of trajectory cycle points with the logIt
  flag, without which points were logged every servo cycle; #ifdef'ed out
  PID, output, and rawoutput sections in code if we're doing steppers
  2-Jun-1999  FMP bracketed debounce of inputs so that it's not done when
  using steppers-- the debounce caused motion to stop when jog speeds were
  less than 1.0.
  21-May-1999  FMP added EMCMOT_LOG_TYPE_TRAJ_POS for logging of Cartesian
  points from the trajectory planner at the traj rate
  8-May-1999  FMP bracketed references to emcmotLog out of RT_FIFO sections;
  added 1 to size of fifos in rtf_create. Note: the status structure worked
  fine when created with sizeof(status structure), but the command structure
  caused infinite wait during Linux process write() to fifo if it was exactly
  the size of the command. When it was made one byte larger, it worked.
  8-Mar-1999  FMP changed calls to tpCreate to provide a third argument
  for the queue space, and added queueTcSpace, emcmotDebug->freeAxisTcSpace arrays for
  this memory.
  3-Mar-1999  FMP took out static malloc stuff, since we now use rtmalloc.h
  in files that do malloc(), which is macroed to kmalloc().
  25-Feb-1999  FMP changed queue.queue.full to tcqFull(&queue.queue) to
  reflect the change in tc.c; added commandEcho to status
  24-Feb-1999  RSB added switched for rtlinux_1_0
  18-Feb-1999  FMP replaced hard-coded fifo numbers with symbolic names
  from emcmot.h
  12-Feb-1999  FMP reportError'ed following error, and SET_AXIS_ERROR_FLAG
  in addition to SET_AXIS_FERROR_FLAG; cleared error flags when emcmotDebug->enabling
  10-Feb-1999  RSB changed the rtlinux-0.9H switch to rtlinux-0.9J
  10-Feb-1999  FMP set the non-moving axes for the homing destination to
  0 so they wouldn't erroneously count toward total displacement for move;
  fixed a failure to reset lastInput[] when the input offset changed due
  to homing
  7-Feb-1999  FMP added input debounce with emcmotDebug->bigVel and lastInput[], so that
  inputs/cycle time > 10X max speed are set to the last input value; made
  emcmotStatus->ferror the mag of the max; added logging of instantaneous
  following error "thisFerror[]"
  4-Feb-1999  FMP removed reportError on limits, since servo jitter about
  a limit caused flooding of these reports; added fix to weird jog problem
  caused when you switched to another axis, and the previous value it had
  for another axis' goal position wasn't "here", so it added to the scalar
  distance and slowed the move
  22-Jan-1999 WPS added memset to init sharedmemory area.
  21-Jan-1999  FMP fixed bug in continuous jogging where values for
  non-jogged axes were uninitialized, instead of read out of status
  12-Nov-1998  FMP made fixes to axis homing and other problems discovered
  by Angelo: axis home destination is twice axis range; setting offset to
  latch position is now accompanied by corresponding correction of input
  position to avoid momentary jump.
  22-Oct-1998  FMP zeroed shared memory; removed call to extEncoderReadAll()
  after extEncoderSetIndexModel() in init_module()
  13-Oct-1998  FMP changed continuous jogging to go to soft limit
  7-Oct-1998 FMP added homing vel stuff
  25-Sep-1998  FMP set axis error flag if on hard or soft limit
  2-Sep-1998  FMP put inRange back in, and included soft and hard limits
  in check to prevent moves from being initiated; added call to tpAbort()
  when inRange() failed in EMCMOT_SET_LINE,CIRCLE command cases
  10-Aug-1998  FMP fixed cut-and-paste bug in which maxLimitSwitchCount
  was being used for min limit switch debounce
  5-Aug-1998  FMP added baseline stepper motor control, keeping all original
  calcs and bracketing new code with ifdef STEPPER_MOTORS
  8-Jul-1998  FMP replaced -shm, -base cmd line args to main() with
  sym=value style, as per insmod, using getArgs() function here
  6-Jul-1998  FMP added -base to main() simulation, not that the simulation
  uses physical shared memory at this point
  15-Jun-1998  FMP added EMCMOT_SET_TERM_COND
  27-May-1998  FMP incremented head and tail in emcmotController()
  11-May-1998  FMP took out <sys/types.h> for size_t, since it gave
  compiler errors. I inlined decl of size_t as unsigned int.
  3-Apr-1998  FMP added active axis concept, with GET_AXIS_ACTIVE_FLAG(),
  SET_AXIS_ACTIVE_FLAG(), etc.
  26-Mar-1998  FMP replaced local etime(), esleep(), and shmget()/shmat()
  with rcslib equivalents, for portability; changed saveLatch[] to doubles
  from ints since that's what they really are
  23-Mar-1998  FMP pulled reportError() out of plat ifdefs since it was
  the same for all
  3-Mar-1998  FMP added -shm to main() process
  25-Feb-1998  FMP added AXIS_ERROR_FLAG
  23-Feb-1998  FMP added logging all inpos, outpos
  17-Feb-1998  FMP added homed bit
  13-Feb-1998  FMP forced inRange() to return 1 always, since it was
  erroneously returning 0 sometimes and causing dropped moves. Will fix later.
  12-Feb-1998  FMP added error logging to memory queue
  9-Feb-1998  FMP fixed bug in which I forgot to set free planner traj
  times in setTrajCycleTime()
  6-Feb-1998  FMP added log types
  22-Jan-1998  FMP changed priority to 1
  20-Jan-1998  FMP added COMM_COPY compile flag to select between direct
  access of upper memory or OS shared memory (fast), or copy-in, copy-out
  access (slowwwww).
  13-Jan-1998  FMP took out stdlib.h, when switching to rtlinux-0.5a
  12-Jan-1998  FMP put RT_FIFO switch in, since we were having NaN
  problems with shared memory (race condition?)
  12-Jan-1998  FMP took rt_use_fp() out of emcmotCommandHandler since
  it's now just a function within emcmotController, which does call it
  8-Jan-1998  FMP wrote polarity for emcmotDebug->enabling, instead of direct 1, 0;
  disabled amps in cleanup_module since we took it out of extQuit code;
  fixed bug where max lim switch flag was set for both max and min
  7-Jan-1998  FMP got rid of rt_task_suspend call in emcmotCommandHandler,
  when changing cycle times. It hangs now that emcmotCommandHandler is
  part of RT task
  6-Jan-1998  FMP switched to shared memory instead of fifos; added
  PID gain and input/output offset globals for init
  25-Nov-1997  FMP changed calls from extLimitSwitchRead() to
  extPos,NegLimit...
  4-Nov-1997  FMP put call to rt_use_fp() in emcmotController(). It was
  in emcmotCommandHandler(), and everything seemed to work OK, but rtlinux
  fp code example had it in cyclic code.
  16-Oct-1997  FMP added compile-time globals
  15-Oct-1997  FMP fixed bug in jog message handlers where vel was being
  set to status, not command
  25-Sep-1997  FMP added EMCMOT_NO_MAIN flag for not compiling in the
  default main loop
  13-Aug-1997  FMP removed setting of pidOutput[] to 0 when axis disabled,
  to allow for DAC writes to go out
  01-Aug-1997  FMP changed emcmotDebug->jointPos from int to double; called pidReset()
  upon emcmotDebug->enabling
  15-Jul-1997  FMP changed estop to enable, for consistency with interface
  30-Jun-1997  FMP added axis software limits; incremental and abs jog
  24-Jun-1997  FMP combined with non-RT-Linux code; added estop; added
  external interface "ext*" calls
  2-May-1997  FMP added logging
  17-Apr-1997  FMP added computeTime calcs
  16-Apr-1997  FMP created
  */

#include <linux/types.h>
#include <stdarg.h>		/* va_list */
#include <float.h>		/* DBL_MIN */
#include <math.h>		/* fabs */

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#include "posemath.h"		/* PmPose, pmCartMag() */
#include "emcpos.h"
#include "emcmotcfg.h"
#include "emcmotglb.h"
#include "emcmot.h"
#include "emcpid.h"
#include "cubic.h"
#include "tc.h"
#include "tp.h"
#include "extintf.h"
#include "mmxavg.h"
#include "emcmotlog.h"

/* FIXME-- testing output rounding to input resolution */
#define NO_ROUNDING

/* FIXME-- testing axis comping */
#define COMPING

int IGNORE_REALTIME_ERRORS = 0;
int DEBUG_EMCMOT = 0;

static EMCMOT_DEBUG localEmcmotDebug;

#ifdef SIMULATED_MOTORS
#include "sim.h"		/* simMotInit() */
#endif

/* lpg changed period from 20000 to avoid flattening a P133                */
/* this is programmable in the .ini file, so faster boxes can use 16us     */
static int PERIOD = 48000;	/* fundamental period for timer interrupts */
static int PERIOD_NSEC = 48000000;

#define DEFAULT_EMCMOT_TASK_PRIORITY 2	/* the highest is 1, the lowest is
					   RT_LOWEST_PRIORITY */
#define DEFAULT_EMCMOT_TASK_STACK_SIZE 8192

/* These are "defined but not used. */
/*
static int EMCMOT_TASK_PRIORITY=DEFAULT_EMCMOT_TASK_PRIORITY;
static int EMCMOT_TASK_STACK_SIZE=DEFAULT_EMCMOT_TASK_STACK_SIZE;
*/

/*
  Principles of communication:

  Data is copied in or out from the various types of comm mechanisms:
  mbuff mapped memory for Linux/RT-Linux, or OS shared memory for Unixes.

  emcmotStruct is ptr to this memory.

  emcmotCommand points to emcmotStruct->command,
  emcmotStatus points to emcmotStruct->status,
  emcmotError points to emcmotStruct->error, and
  emcmotLog points to emcmotStruct->log.
  emcmotComp[] points to emcmotStruct->comp[].
 */

/* RTAPI shmem stuff */
static int key = 101;
static int shmem_mem;		/* the shared memory ID */

/* need to point to command, status, error, and log */
EMCMOT_STRUCT *emcmotStruct;

/* ptrs to either buffered copies or direct memory for
   command and status */
static EMCMOT_COMMAND *emcmotCommand;
static EMCMOT_STATUS *emcmotStatus;
static EMCMOT_STATUS *emcmotStatus;
static EMCMOT_CONFIG *emcmotConfig;
static EMCMOT_DEBUG *emcmotDebug;
static EMCMOT_ERROR *emcmotError;	/* unused for RT_FIFO */
static EMCMOT_LOG *emcmotLog;	/* unused for RT_FIFO */
static EMCMOT_IO *emcmotIo;	/* new struct added 8/21/2001 JME */
static EMCMOT_COMP *emcmotComp[EMCMOT_MAX_AXIS];	/* unused for RT_FIFO 
							 */

static EMCMOT_LOG_STRUCT ls;
static int logSkip = 0;		/* how many to skip, for per-cycle logging */
static int loggingAxis = 0;	/* record of which axis to log */
static int logIt = 0;
static int logStartTime;	/* set when logging is started, and
				   subtracted off each log time for better
				   resolution */

/* value for world home position */
static EmcPose worldHome = { {0.0, 0.0, 0.0},
0.0, 0.0, 0.0
};

/* kinematics flags */
static KINEMATICS_FORWARD_FLAGS fflags = 0;
static KINEMATICS_INVERSE_FLAGS iflags = 0;

/* the type of kinematics, from emcmot.h */
static int kinType = 0;

/* flag to handle need to re-home when joints have moved outside purview
   of Cartesian control, for machines with no forward kinematics.
   0 = need to home only joint that was jogged, 1 = need to home all joints
   if any axis has been jogged. Initially 0, it's set to 1 for any coordinated
   move that will in general move all joints. It's set back to zero when
   all joints have been rehomed. */
static int rehomeAll = 0;

/* debouncing */
#define LIMIT_SWITCH_DEBOUNCE 10
#define AMP_FAULT_DEBOUNCE 100
#define POSITION_INPUT_DEBOUNCE 10
#ifndef SIMULATED_MOTORS
static double positionInputDebounce[EMCMOT_MAX_AXIS] = { 0.0 };
#endif

static inline void MARK_EMCMOT_CONFIG_CHANGE(void)
{
    if (emcmotConfig->head == emcmotConfig->tail) {
	emcmotConfig->config_num++;
	emcmotStatus->config_num = emcmotConfig->config_num;
	emcmotConfig->head++;
    }
}

/* macros for reading, writing bit flags */

/* motion flags */

#define GET_MOTION_ERROR_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_ERROR_BIT ? 1 : 0)

#define SET_MOTION_ERROR_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_ERROR_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_ERROR_BIT;

#define GET_MOTION_COORD_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_COORD_BIT ? 1 : 0)

#define SET_MOTION_COORD_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_COORD_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_COORD_BIT;

#define GET_MOTION_TELEOP_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_TELEOP_BIT ? 1 : 0)

#define SET_MOTION_TELEOP_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_TELEOP_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_TELEOP_BIT;

#define GET_MOTION_INPOS_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_INPOS_BIT ? 1 : 0)

#define SET_MOTION_INPOS_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_INPOS_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_INPOS_BIT;

#define GET_MOTION_ENABLE_FLAG() (emcmotStatus->motionFlag & EMCMOT_MOTION_ENABLE_BIT ? 1 : 0)

#define SET_MOTION_ENABLE_FLAG(fl) if (fl) emcmotStatus->motionFlag |= EMCMOT_MOTION_ENABLE_BIT; else emcmotStatus->motionFlag &= ~EMCMOT_MOTION_ENABLE_BIT;

/* axis flags */

#define GET_AXIS_ENABLE_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0)

#define SET_AXIS_ENABLE_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_ENABLE_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_ENABLE_BIT;

#define GET_AXIS_ACTIVE_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_ACTIVE_BIT ? 1 : 0)

#define SET_AXIS_ACTIVE_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_ACTIVE_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_ACTIVE_BIT;

#define GET_AXIS_INPOS_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_INPOS_BIT ? 1 : 0)

#define SET_AXIS_INPOS_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_INPOS_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_INPOS_BIT;

#define GET_AXIS_ERROR_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_ERROR_BIT ? 1 : 0)

#define SET_AXIS_ERROR_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_ERROR_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_ERROR_BIT;

#define GET_AXIS_PSL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_PSL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MAX_SOFT_LIMIT_BIT;

#define GET_AXIS_NSL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_NSL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MIN_SOFT_LIMIT_BIT;

#define GET_AXIS_PHL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_PHL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MAX_HARD_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MAX_HARD_LIMIT_BIT;

#define GET_AXIS_NHL_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_NHL_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_MIN_HARD_LIMIT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_MIN_HARD_LIMIT_BIT;

#define GET_AXIS_HOME_SWITCH_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_HOME_SWITCH_BIT ? 1 : 0)

#define SET_AXIS_HOME_SWITCH_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_HOME_SWITCH_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_HOME_SWITCH_BIT;

#define GET_AXIS_HOMING_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_HOMING_BIT ? 1 : 0)

#define SET_AXIS_HOMING_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_HOMING_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_HOMING_BIT;

#define GET_AXIS_HOMED_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_HOMED_BIT ? 1 : 0)

#define SET_AXIS_HOMED_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_HOMED_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_HOMED_BIT;

#define GET_AXIS_FERROR_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_FERROR_BIT ? 1 : 0)

#define SET_AXIS_FERROR_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_FERROR_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_FERROR_BIT;

#define GET_AXIS_FAULT_FLAG(ax) (emcmotStatus->axisFlag[ax] & EMCMOT_AXIS_FAULT_BIT ? 1 : 0)

#define SET_AXIS_FAULT_FLAG(ax,fl) if (fl) emcmotStatus->axisFlag[ax] |= EMCMOT_AXIS_FAULT_BIT; else emcmotStatus->axisFlag[ax] &= ~EMCMOT_AXIS_FAULT_BIT;

/* polarity flags */

#define GET_AXIS_ENABLE_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_ENABLE_BIT ? 1 : 0)

#define SET_AXIS_ENABLE_POLARITY(ax,fl) MARK_EMCMOT_CONFIG_CHANGE(); if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_ENABLE_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_ENABLE_BIT;

#define GET_AXIS_PHL_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_MAX_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_PHL_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_MAX_HARD_LIMIT_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_MAX_HARD_LIMIT_BIT;

#define GET_AXIS_NHL_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_MIN_HARD_LIMIT_BIT ? 1 : 0)

#define SET_AXIS_NHL_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_MIN_HARD_LIMIT_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_MIN_HARD_LIMIT_BIT;

#define GET_AXIS_HOME_SWITCH_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_HOME_SWITCH_BIT ? 1 : 0)

#define SET_AXIS_HOME_SWITCH_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_HOME_SWITCH_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_HOME_SWITCH_BIT;

#define GET_AXIS_HOMING_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_HOMING_BIT ? 1 : 0)

#define SET_AXIS_HOMING_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_HOMING_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_HOMING_BIT;

#define GET_AXIS_FAULT_POLARITY(ax) (emcmotConfig->axisPolarity[ax] & EMCMOT_AXIS_FAULT_BIT ? 1 : 0)

#define SET_AXIS_FAULT_POLARITY(ax,fl) if (fl) emcmotConfig->axisPolarity[ax] |= EMCMOT_AXIS_FAULT_BIT; else emcmotConfig->axisPolarity[ax] &= ~EMCMOT_AXIS_FAULT_BIT;

/* FIXME */
int EMCMOT_NO_FORWARD_KINEMATICS = 0;

static double etime(void)
{
    return ((double) rtapi_get_time()) / 1.0e9;
}

static void reportError(const char *fmt, ...)
{
    /* use the rtapi_snprintf function where vsprintf is called for. */
    va_list args;
    char error[EMCMOT_ERROR_LEN];

    va_start(args, fmt);
    rtapi_snprintf(error, EMCMOT_ERROR_LEN, fmt, args);
    va_end(args);

    emcmotErrorPut(emcmotError, error);
}

/* isHoming() returns non-zero if any axes are homing, 0 if none are */
static int isHoming(void)
{
    int axis;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (GET_AXIS_HOMING_FLAG(axis)) {
	    return 1;
	}
    }

    /* got here, so none are homing */
    return 0;
}

/* clearHomes() will clear the homed flags for axes that have moved
   since homing, outside coordinated control, for machines with no
   forward kinematics. This is used in conjunction with the rehomeAll
   flag, which is set for any coordinated move that in general will
   result in all joints moving. The flag is consulted whenever a joint
   is jogged in joint mode, so that either its flag can be cleared if
   no other joints have moved, or all have to be cleared. */
static void clearHomes(int axis)
{
    int t;

    if (kinType == KINEMATICS_INVERSE_ONLY) {
	if (rehomeAll) {
	    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
		SET_AXIS_HOMED_FLAG(t, 0);
	    }
	} else {
	    SET_AXIS_HOMED_FLAG(axis, 0);
	}
    }
    if (0 != emcmotDebug) {
	emcmotDebug->allHomed = 0;
    }
}

/* axis lengths */
#define AXRANGE(axis) ((emcmotConfig->maxLimit[axis] - emcmotConfig->minLimit[axis]))

/* inRange() returns non-zero if the position lies within the axis
   limits, or 0 if not */
static int inRange(EmcPose pos)
{
    double joint[EMCMOT_MAX_AXIS];
    int axis;

    /* fill in all joints with 0 */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	joint[axis] = 0.0;
    }

    /* now fill in with real values, for joints that are used */
    kinematicsInverse(&pos, joint, &iflags, &fflags);

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (!GET_AXIS_ACTIVE_FLAG(axis)) {
	    /* if axis is not active, don't even look at its limits */
	    continue;
	}

	if (joint[axis] > emcmotConfig->maxLimit[axis] ||
	    joint[axis] < emcmotConfig->minLimit[axis]) {
	    return 0;		/* can't move further past limit */
	}
    }

    /* okay to move */
    return 1;
}

/* checkLimits() returns 1 if none of the soft or hard limits are
   set, 0 if any are set. Called on a linear and circular move. */
static int checkLimits(void)
{
    int axis;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (!GET_AXIS_ACTIVE_FLAG(axis)) {
	    /* if axis is not active, don't even look at its limits */
	    continue;
	}

	if (GET_AXIS_PSL_FLAG(axis) ||
	    GET_AXIS_NSL_FLAG(axis) ||
	    GET_AXIS_PHL_FLAG(axis) || GET_AXIS_NHL_FLAG(axis)) {
	    return 0;
	}
    }

    return 1;
}

/* check the value of the axis and velocity against current position,
   returning 1 (okay) if the request is to jog off the limit, 0 (bad)
   if the request is to jog further past a limit. Software limits are
   ignored if the axis hasn't been homed */
static int checkJog(int axis, double vel)
{
    if (emcmotStatus->overrideLimits) {
	return 1;		/* okay to jog when limits overridden */
    }

    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
	reportError("Can't jog out of range axis %d.", axis);
	return 0;		/* can't jog out-of-range axis */
    }

    if (vel > 0.0 && GET_AXIS_PSL_FLAG(axis)) {
	reportError("Can't jog axis %d further past max soft limit.", axis);
	return 0;
    }

    if (vel > 0.0 && GET_AXIS_PHL_FLAG(axis)) {
	reportError("Can't jog axis %d further past max hard limit.", axis);
	return 0;
    }

    if (vel < 0.0 && GET_AXIS_NSL_FLAG(axis)) {
	reportError("Can't jog axis %d further past min soft limit.", axis);
	return 0;
    }

    if (vel < 0.0 && GET_AXIS_NHL_FLAG(axis)) {
	reportError("Can't jog axis %d further past min hard limit.", axis);
	return 0;
    }

    /* okay to jog */
    return 1;
}

/* counter that triggers computation of forward kinematics during
   disabled state, making them run at the trajectory rate instead
   of the servo rate. In the disabled state the interpolators are
   not queried so we don't know when a trajectory cycle would occur,
   so we use this counter. It's loaded with emcmotConfig->interpolationRate
   whenever it goes to zero, during the code executed in the disabled
   state. */
static int interpolationCounter = 0;

/* call this when setting the trajectory cycle time */
static void setTrajCycleTime(double secs)
{
    static int t;

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return;
    }

    MARK_EMCMOT_CONFIG_CHANGE();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (secs / emcmotConfig->servoCycleTime + 0.5);

    /* set traj planner */
    tpSetCycleTime(&emcmotDebug->queue, secs);

    /* set the free planners, cubic interpolation rate and segment time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	tpSetCycleTime(&emcmotDebug->freeAxis[t], secs);
	cubicSetInterpolationRate(&emcmotDebug->cubic[t],
	    emcmotConfig->interpolationRate);
    }

    /* copy into status out */
    emcmotConfig->trajCycleTime = secs;
}

/* call this when setting the servo cycle time */
static void setServoCycleTime(double secs)
{
    static int t;

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return;
    }

    MARK_EMCMOT_CONFIG_CHANGE();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (emcmotConfig->trajCycleTime / secs + 0.5);

    /* set the cubic interpolation rate and PID cycle time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	cubicSetInterpolationRate(&emcmotDebug->cubic[t],
	    emcmotConfig->interpolationRate);
	cubicSetSegmentTime(&emcmotDebug->cubic[t], secs);
	pidSetCycleTime(&emcmotConfig->pid[t], secs);
    }

    /* copy into status out */
    emcmotConfig->servoCycleTime = secs;
}

/*
  emcmotCommandHandler() is called each main cycle to read the
  shared memory buffer
  */
static int emcmotCommandHandler(void)
{
    int axis;
    int valid;
    /* check for split read */
    if (emcmotCommand->head != emcmotCommand->tail) {
	emcmotDebug->split++;
	return 0;		/* not really an error */
    }

    if (emcmotCommand->commandNum != emcmotStatus->commandNumEcho) {
	/* increment head count-- we'll be modifying emcmotStatus */
	emcmotStatus->head++;
	emcmotDebug->head++;

	/* got a new command-- echo command and number... */
	emcmotStatus->commandEcho = emcmotCommand->command;
	emcmotStatus->commandNumEcho = emcmotCommand->commandNum;

	/* clear status value by default */
	emcmotStatus->commandStatus = EMCMOT_COMMAND_OK;

	/* log it, if appropriate */
	if (emcmotStatus->logStarted &&
	    emcmotStatus->logType == EMCMOT_LOG_TYPE_CMD) {
	    ls.item.cmd.time = etime();	/* don't subtract off logStartTime,
					   since we want an absolute time
					   value */
	    ls.item.cmd.command = emcmotCommand->command;
	    ls.item.cmd.commandNum = emcmotCommand->commandNum;
	    emcmotLogAdd(emcmotLog, ls);
	    emcmotStatus->logPoints = emcmotLog->howmany;
	}

	/* ...and process command */
	switch (emcmotCommand->command) {
	case EMCMOT_FREE:
	    /* change the mode to free axis motion */
	    /* can be done at any time */
	    /* reset the emcmotDebug->coordinating flag to defer transition
	       to controller cycle */
	    emcmotDebug->coordinating = 0;
	    emcmotDebug->teleoperating = 0;
	    break;

	case EMCMOT_COORD:
	    /* change the mode to coordinated axis motion */
	    /* can be done at any time */
	    /* set the emcmotDebug->coordinating flag to defer transition to
	       controller cycle */
	    emcmotDebug->coordinating = 1;
	    emcmotDebug->teleoperating = 0;
	    if (kinType != KINEMATICS_IDENTITY) {
		if (!emcmotDebug->allHomed) {
		    reportError
			("all axes must be homed before going into coordinated mode");
		    emcmotDebug->coordinating = 0;
		    break;
		}
	    }
	    break;

	case EMCMOT_TELEOP:
	    /* change the mode to teleop motion */
	    /* can be done at any time */
	    /* set the emcmotDebug->teleoperating flag to defer transition to
	       controller cycle */
	    emcmotDebug->teleoperating = 1;
	    if (kinType != KINEMATICS_IDENTITY) {
		if (!emcmotDebug->allHomed) {
		    reportError
			("all axes must be homed before going into teleop mode");
		    emcmotDebug->teleoperating = 0;
		    break;
		}

	    }
	    break;

	case EMCMOT_SET_NUM_AXES:
	    /* set the global NUM_AXES, which must be between 1 and
	       EMCMOT_MAX_AXIS, inclusive */
	    axis = emcmotCommand->axis;
	    /* note that this comparison differs from the check on the range
	       of 'axis' in most other places, since those checks are for a
	       value to be used as an index and here it's a value to be used
	       as a counting number. The indenting is different here so as
	       not to match macro editing on that other bunch. */
	    if (axis <= 0 || axis > EMCMOT_MAX_AXIS) {
		break;
	    }
	    NUM_AXES = axis;
	    emcmotConfig->numAxes = axis;
	    break;

	case EMCMOT_SET_WORLD_HOME:
	    worldHome = emcmotCommand->pos;
	    break;

	case EMCMOT_SET_JOINT_HOME:
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    emcmotDebug->jointHome[axis] = emcmotCommand->offset;	/* FIXME-- 
									   use 
									   'home' 
									   instead 
									 */
	    break;

	case EMCMOT_SET_HOME_OFFSET:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    emcmotConfig->homeOffset[axis] = emcmotCommand->offset;
	    break;

	case EMCMOT_OVERRIDE_LIMITS:
	    if (emcmotCommand->axis < 0) {
		/* don't override limits */
		emcmotStatus->overrideLimits = 0;
	    } else {
		emcmotStatus->overrideLimits = 1;
	    }
	    emcmotDebug->overriding = 0;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		SET_AXIS_ERROR_FLAG(axis, 0);
	    }
	    break;

	case EMCMOT_SET_TRAJ_CYCLE_TIME:
	    /* set the cycle time for trajectory calculations */
	    /* really should be done only at startup before controller is
	       run, but at least it requires no active motions and the
	       interpolators need to be cleared */
	    setTrajCycleTime(emcmotCommand->cycleTime);
	    break;

	case EMCMOT_SET_SERVO_CYCLE_TIME:
	    /* set the cycle time for servo calculations, which is the period 
	       for emcmotController execution */
	    /* really should be done only at startup before controller is
	       run, but at least it requires no active motions and the
	       interpolators need to be cleared */
	    /* FIXME-- add re-timing a task to RTAPI */
	    rtapi_print("emcmot: servo cycle time set to %d nsecs\n",
		(int) (emcmotCommand->cycleTime * 1.0e9));
	    break;

	case EMCMOT_SET_POSITION_LIMITS:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    /* set the position limits for the axis */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    emcmotConfig->minLimit[axis] = emcmotCommand->minLimit;
	    emcmotConfig->maxLimit[axis] = emcmotCommand->maxLimit;
	    break;

	case EMCMOT_SET_OUTPUT_LIMITS:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    /* set the output limits for the axis */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    emcmotConfig->minOutput[axis] = emcmotCommand->minLimit;
	    emcmotConfig->maxOutput[axis] = emcmotCommand->maxLimit;
	    break;

	case EMCMOT_SET_OUTPUT_SCALE:
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->scale == 0) {
		break;
	    }
	    emcmotStatus->outputScale[axis] = emcmotCommand->scale;
	    emcmotStatus->outputOffset[axis] = emcmotCommand->offset;
	    emcmotDebug->inverseOutputScale[axis] =
		1.0 / emcmotStatus->outputScale[axis];
	    break;

	case EMCMOT_SET_INPUT_SCALE:
	    /* 
	       change the scale factor for the position input, e.g., encoder
	       counts per unit. Note that this is not a good idea once things 
	       have gotten underway, since the axis will jump servo to the
	       "new" position, the gains will no longer be appropriate, etc. */
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->scale == 0.0) {
		break;
	    }
	    emcmotDebug->oldInputValid[axis] = 0;

	    /* now make them active */
	    emcmotStatus->inputScale[axis] = emcmotCommand->scale;
	    emcmotStatus->inputOffset[axis] = emcmotCommand->offset;
	    emcmotDebug->inverseInputScale[axis] =
		1.0 / emcmotStatus->inputScale[axis];
	    break;

	    /* 
	       Max and min ferror work like this: limiting ferror is
	       determined by slope of ferror line, = maxFerror/limitVel ->
	       limiting ferror = maxFerror/limitVel * vel. If ferror <
	       minFerror then OK else if ferror < limiting ferror then OK
	       else ERROR */

	case EMCMOT_SET_MAX_FERROR:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->maxFerror < 0.0) {
		break;
	    }
	    emcmotConfig->maxFerror[axis] = emcmotCommand->maxFerror;
	    break;

	case EMCMOT_SET_MIN_FERROR:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    axis = emcmotCommand->axis;
	    if (axis < 0 ||
		axis >= EMCMOT_MAX_AXIS || emcmotCommand->minFerror < 0.0) {
		break;
	    }
	    emcmotConfig->minFerror[axis] = emcmotCommand->minFerror;
	    break;

	case EMCMOT_JOG_CONT:
	    /* do a continuous jog, implemented as an incremental jog to the
	       software limit, or the full range of travel if software limits 
	       don't yet apply because we're not homed */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    /* requires no motion, in free mode, enable on */
	    if (GET_MOTION_COORD_FLAG()) {
		reportError("Can't jog axis in coordinated mode.");
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (!GET_MOTION_INPOS_FLAG()) {
		reportError("Can't jog axis when not in position.");
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (!GET_MOTION_ENABLE_FLAG()) {
		reportError("Can't jog axis when not enabled.");
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(axis, emcmotCommand->vel)) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (emcmotCommand->vel > 0.0) {
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->maxLimit[axis];
		} else {
		    emcmotDebug->freePose.tran.x =
			emcmotDebug->jointPos[axis] + AXRANGE(axis);
		}
	    } else {
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->minLimit[axis];
		} else {
		    emcmotDebug->freePose.tran.x =
			emcmotDebug->jointPos[axis] - AXRANGE(axis);
		}
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis], fabs(emcmotCommand->vel));
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    SET_AXIS_ERROR_FLAG(axis, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(axis);
	    break;

	case EMCMOT_JOG_INCR:
	    /* do an incremental jog */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    /* requires no motion, in free mode, enable on */
	    if (GET_MOTION_COORD_FLAG() ||
		!GET_MOTION_INPOS_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(axis, emcmotCommand->vel)) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    if (emcmotCommand->vel > 0.0) {
		emcmotDebug->freePose.tran.x = emcmotDebug->jointPos[axis] + emcmotCommand->offset;	/* FIXME-- 
													   use 
													   'goal' 
													   instead 
													 */
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    if (emcmotDebug->freePose.tran.x >
			emcmotConfig->maxLimit[axis]) {
			emcmotDebug->freePose.tran.x =
			    emcmotConfig->maxLimit[axis];
		    }
		}
	    } else {
		emcmotDebug->freePose.tran.x = emcmotDebug->jointPos[axis] - emcmotCommand->offset;	/* FIXME-- 
													   use 
													   'goal' 
													   instead 
													 */
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    if (emcmotDebug->freePose.tran.x <
			emcmotConfig->minLimit[axis]) {
			emcmotDebug->freePose.tran.x =
			    emcmotConfig->minLimit[axis];
		    }
		}
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis], fabs(emcmotCommand->vel));
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    SET_AXIS_ERROR_FLAG(axis, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(axis);

	    break;

	case EMCMOT_JOG_ABS:
	    /* do an absolute jog */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    /* requires no motion, in free mode, enable on */
	    if (GET_MOTION_COORD_FLAG() ||
		!GET_MOTION_INPOS_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    /* don't jog further onto limits */
	    if (!checkJog(axis, emcmotCommand->vel)) {
		SET_AXIS_ERROR_FLAG(axis, 1);
		break;
	    }

	    emcmotDebug->freePose.tran.x = emcmotCommand->offset;	/* FIXME-- 
									   use 
									   'goal' 
									   instead 
									 */
	    if (GET_AXIS_HOMED_FLAG(axis)) {
		if (emcmotDebug->freePose.tran.x >
		    emcmotConfig->maxLimit[axis]) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->maxLimit[axis];
		} else if (emcmotDebug->freePose.tran.x <
		    emcmotConfig->minLimit[axis]) {
		    emcmotDebug->freePose.tran.x =
			emcmotConfig->minLimit[axis];
		}
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis], fabs(emcmotCommand->vel));
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    SET_AXIS_ERROR_FLAG(axis, 0);
	    /* clear axis homed flag(s) if we don't have forward kins.
	       Otherwise, a transition into coordinated mode will incorrectly
	       assume the homed position. Do all if they've all been moved
	       since homing, otherwise just do this one */
	    clearHomes(axis);

	    break;

	case EMCMOT_SET_TERM_COND:
	    /* sets termination condition for motion emcmotDebug->queue */
	    tpSetTermCond(&emcmotDebug->queue, emcmotCommand->termCond);
	    break;

	case EMCMOT_SET_LINE:
	    /* emcmotDebug->queue up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for linear move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos)) {
		reportError("linear move %d out of range", emcmotCommand->id);
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!checkLimits()) {
		reportError("can't do linear move with limits exceeded");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

	    /* append it to the emcmotDebug->queue */
	    tpSetId(&emcmotDebug->queue, emcmotCommand->id);
	    if (-1 == tpAddLine(&emcmotDebug->queue, emcmotCommand->pos)) {
		reportError("can't add linear move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_CIRCLE:
	    /* emcmotDebug->queue up a circular move */
	    /* requires coordinated mode, enable on, not on limits */
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for circular move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos)) {
		reportError("circular move %d out of range",
		    emcmotCommand->id);
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!checkLimits()) {
		reportError("can't do circular move with limits exceeded");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

	    /* append it to the emcmotDebug->queue */
	    tpSetId(&emcmotDebug->queue, emcmotCommand->id);
	    if (-1 ==
		tpAddCircle(&emcmotDebug->queue, emcmotCommand->pos,
		    emcmotCommand->center, emcmotCommand->normal,
		    emcmotCommand->turn)) {
		reportError("can't add circular move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_VEL:
	    /* set the velocity for subsequent moves */
	    /* can do it at any time */
	    emcmotStatus->vel = emcmotCommand->vel;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpSetVmax(&emcmotDebug->freeAxis[axis], emcmotStatus->vel);
	    }
	    tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel);
	    break;

	case EMCMOT_SET_VEL_LIMIT:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    /* set the absolute max velocity for all subsequent moves */
	    /* can do it at any time */
	    emcmotConfig->limitVel = emcmotCommand->vel;
	    tpSetVlimit(&emcmotDebug->queue, emcmotConfig->limitVel);
	    break;

	case EMCMOT_SET_AXIS_VEL_LIMIT:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    tpSetVlimit(&emcmotDebug->freeAxis[axis], emcmotCommand->vel);
	    emcmotConfig->axisLimitVel[axis] = emcmotCommand->vel;
	    emcmotDebug->bigVel[axis] = 10 * emcmotCommand->vel;
	    break;

	case EMCMOT_SET_HOMING_VEL:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    /* set the homing velocity */
	    /* can do it at any time */
	    /* sign of vel should set polarity, and mag-sign are recorded */

	    /* check axis range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    if (emcmotCommand->vel < 0.0) {
		emcmotConfig->homingVel[axis] = -emcmotCommand->vel;
		SET_AXIS_HOMING_POLARITY(axis, 0);
	    } else {
		emcmotConfig->homingVel[axis] = emcmotCommand->vel;
		SET_AXIS_HOMING_POLARITY(axis, 1);
	    }
	    break;

	case EMCMOT_SET_ACC:
	    /* set the max acceleration */
	    /* can do it at any time */
	    emcmotStatus->acc = emcmotCommand->acc;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpSetAmax(&emcmotDebug->freeAxis[axis], emcmotStatus->acc);
	    }
	    tpSetAmax(&emcmotDebug->queue, emcmotStatus->acc);
	    break;

	case EMCMOT_PAUSE:
	    /* pause the motion */
	    /* can happen at any time */
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpPause(&emcmotDebug->freeAxis[axis]);
	    }
	    tpPause(&emcmotDebug->queue);
	    emcmotStatus->paused = 1;
	    break;

	case EMCMOT_RESUME:
	    /* resume paused motion */
	    /* can happen at any time */
	    emcmotDebug->stepping = 0;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpResume(&emcmotDebug->freeAxis[axis]);
	    }
	    tpResume(&emcmotDebug->queue);
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_STEP:
	    /* resume paused motion until id changes */
	    /* can happen at any time */
	    emcmotDebug->idForStep = emcmotStatus->id;
	    emcmotDebug->stepping = 1;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpResume(&emcmotDebug->freeAxis[axis]);
	    }
	    tpResume(&emcmotDebug->queue);
	    emcmotStatus->paused = 0;
	    break;

	case EMCMOT_SCALE:
	    /* override speed */
	    /* can happen at any time */
	    if (emcmotCommand->scale < 0.0) {
		emcmotCommand->scale = 0.0;	/* clamp it */
	    }
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpSetVscale(&emcmotDebug->freeAxis[axis],
		    emcmotCommand->scale);
		emcmotStatus->axVscale[axis] = emcmotCommand->scale;
	    }
	    tpSetVscale(&emcmotDebug->queue, emcmotCommand->scale);
	    emcmotStatus->qVscale = emcmotCommand->scale;
	    break;

	case EMCMOT_ABORT:
	    /* abort motion */
	    /* can happen at any time */
	    /* check for coord or free space motion active */
	    if (GET_MOTION_TELEOP_FLAG()) {
		emcmotDebug->teleop_data.desiredVel.tran.x = 0.0;
		emcmotDebug->teleop_data.desiredVel.tran.y = 0.0;
		emcmotDebug->teleop_data.desiredVel.tran.z = 0.0;
		emcmotDebug->teleop_data.desiredVel.a = 0.0;
		emcmotDebug->teleop_data.desiredVel.b = 0.0;
		emcmotDebug->teleop_data.desiredVel.c = 0.0;
	    } else if (GET_MOTION_COORD_FLAG()) {
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(0);
	    } else {
		/* check axis range */
		axis = emcmotCommand->axis;
		if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		    break;
		}
		tpAbort(&emcmotDebug->freeAxis[axis]);
		SET_AXIS_HOMING_FLAG(axis, 0);
		SET_AXIS_ERROR_FLAG(axis, 0);
	    }
	    break;

	case EMCMOT_DISABLE:
	    /* go into disable */
	    /* can happen at any time */
	    /* reset the emcmotDebug->enabling flag to defer disable until
	       controller cycle (it *will* be honored) */
	    emcmotDebug->enabling = 0;
	    if (kinType == KINEMATICS_INVERSE_ONLY) {
		emcmotDebug->teleoperating = 0;
		emcmotDebug->coordinating = 0;
	    }
	    break;

	case EMCMOT_ENABLE:
	    /* come out of disable */
	    /* can happen at any time */
	    /* set the emcmotDebug->enabling flag to defer enable until
	       controller cycle */
	    emcmotDebug->enabling = 1;
	    if (kinType == KINEMATICS_INVERSE_ONLY) {
		emcmotDebug->teleoperating = 0;
		emcmotDebug->coordinating = 0;
	    }
	    break;

	case EMCMOT_SET_PID:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    /* configure the PID gains */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    pidSetGains(&emcmotConfig->pid[axis], emcmotCommand->pid);
	    break;

	case EMCMOT_ACTIVATE_AXIS:
	    /* make axis active, so that amps will be enabled when system is
	       enabled or disabled */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    SET_AXIS_ACTIVE_FLAG(axis, 1);
	    break;

	case EMCMOT_DEACTIVATE_AXIS:
	    /* make axis inactive, so that amps won't be affected when system 
	       is enabled or disabled */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    SET_AXIS_ACTIVE_FLAG(axis, 0);
	    break;

	case EMCMOT_ENABLE_AMPLIFIER:
	    /* enable the amplifier directly, but don't enable calculations */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    extAmpEnable(axis, GET_AXIS_ENABLE_POLARITY(axis));
	    break;

	case EMCMOT_DISABLE_AMPLIFIER:
	    /* disable the axis calculations and amplifier, but don't disable 
	       calculations */
	    /* can be done at any time */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
	    break;

	case EMCMOT_OPEN_LOG:
	    /* open a data log */
	    axis = emcmotCommand->axis;
	    valid = 0;
	    if (emcmotCommand->logSize > 0 &&
		emcmotCommand->logSize <= EMCMOT_LOG_MAX) {
		/* handle log-specific data */
		switch (emcmotCommand->logType) {
		case EMCMOT_LOG_TYPE_AXIS_POS:
		case EMCMOT_LOG_TYPE_AXIS_VEL:
		case EMCMOT_LOG_TYPE_POS_VOLTAGE:
		    if (axis >= 0 && axis < EMCMOT_MAX_AXIS) {
			valid = 1;
		    }
		    break;

		default:
		    valid = 1;
		    break;
		}
	    }

	    if (valid) {
		/* success */
		loggingAxis = axis;
		emcmotLogInit(emcmotLog,
		    emcmotCommand->logType, emcmotCommand->logSize);
		emcmotStatus->logOpen = 1;
		emcmotStatus->logStarted = 0;
		emcmotStatus->logSize = emcmotCommand->logSize;
		emcmotStatus->logSkip = emcmotCommand->logSkip;
		emcmotStatus->logType = emcmotCommand->logType;
		emcmotStatus->logTriggerType = emcmotCommand->logTriggerType;
		emcmotStatus->logTriggerVariable =
		    emcmotCommand->logTriggerVariable;
		emcmotStatus->logTriggerThreshold =
		    emcmotCommand->logTriggerThreshold;
		if (axis >= 0 && axis < EMCMOT_MAX_AXIS
		    && emcmotStatus->logTriggerType == EMCLOG_DELTA_TRIGGER) {
		    switch (emcmotStatus->logTriggerVariable) {
		    case EMCLOG_TRIGGER_ON_FERROR:
			emcmotStatus->logStartVal =
			    emcmotDebug->ferrorCurrent[loggingAxis];
			break;

		    case EMCLOG_TRIGGER_ON_VOLT:
			emcmotStatus->logStartVal =
			    emcmotDebug->rawOutput[loggingAxis];
			break;
		    case EMCLOG_TRIGGER_ON_POS:
			emcmotStatus->logStartVal =
			    emcmotDebug->jointPos[loggingAxis];
			break;
		    case EMCLOG_TRIGGER_ON_VEL:
			emcmotStatus->logStartVal =
			    emcmotDebug->jointPos[loggingAxis] -
			    emcmotDebug->oldJointPos[loggingAxis];
			break;

		    default:
			break;
		    }
		}
	    }
	    break;

	case EMCMOT_START_LOG:
	    /* start logging */
	    /* first ignore triggered log types */
	    if (emcmotStatus->logType == EMCMOT_LOG_TYPE_POS_VOLTAGE) {
		break;
	    }
	    /* set the global baseTime, to be subtracted off log times,
	       otherwise time values are too large for the small increments
	       to appear */
	    if (emcmotStatus->logOpen &&
		emcmotStatus->logTriggerType == EMCLOG_MANUAL_TRIGGER) {
		logStartTime = etime();
		emcmotStatus->logStarted = 1;
		logSkip = 0;
	    }
	    break;

	case EMCMOT_STOP_LOG:
	    /* stop logging */
	    emcmotStatus->logStarted = 0;
	    break;

	case EMCMOT_CLOSE_LOG:
	    emcmotStatus->logOpen = 0;
	    emcmotStatus->logStarted = 0;
	    emcmotStatus->logSize = 0;
	    emcmotStatus->logSkip = 0;
	    emcmotStatus->logType = 0;
	    break;

	case EMCMOT_DAC_OUT:
	    /* write output to dacs directly */
	    /* will only persist if amplifiers are disabled */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    /* trigger log, if active */
	    if (emcmotStatus->logType == EMCMOT_LOG_TYPE_POS_VOLTAGE &&
		loggingAxis == axis && emcmotStatus->logOpen != 0) {
		emcmotStatus->logStarted = 1;
		logSkip = 0;
	    }
	    emcmotStatus->output[axis] = emcmotCommand->dacOut;
	    break;

	case EMCMOT_HOME:
	    /* home the specified axis */
	    /* need to be in free mode, enable on */
	    /* homing is basically a slow incremental jog to full range */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    if (GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		break;
	    }

	    if (GET_AXIS_HOMING_POLARITY(axis)) {
		emcmotDebug->freePose.tran.x = +2.0 * AXRANGE(axis);
	    } else {
		emcmotDebug->freePose.tran.x = -2.0 * AXRANGE(axis);
	    }

	    tpSetVmax(&emcmotDebug->freeAxis[axis],
		emcmotConfig->homingVel[axis]);
	    tpAddLine(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	    emcmotDebug->homingPhase[axis] = 1;
	    SET_AXIS_HOMING_FLAG(axis, 1);
	    SET_AXIS_HOMED_FLAG(axis, 0);
	    break;

	case EMCMOT_ENABLE_WATCHDOG:
	    emcmotDebug->wdEnabling = 1;
	    emcmotDebug->wdWait = emcmotCommand->wdWait;
	    if (emcmotDebug->wdWait < 0) {
		emcmotDebug->wdWait = 0;
	    }
	    break;

	case EMCMOT_DISABLE_WATCHDOG:
	    emcmotDebug->wdEnabling = 0;
	    break;

	case EMCMOT_SET_POLARITY:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }
	    if (emcmotCommand->level) {
		/* normal */
		emcmotConfig->axisPolarity[axis] |= emcmotCommand->axisFlag;
	    } else {
		/* inverted */
		emcmotConfig->axisPolarity[axis] &= ~emcmotCommand->axisFlag;
	    }
	    break;

	case EMCMOT_SET_PROBE_INDEX:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    emcmotConfig->probeIndex = emcmotCommand->probeIndex;
	    break;

	case EMCMOT_SET_PROBE_POLARITY:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    emcmotConfig->probePolarity = emcmotCommand->level;
	    break;

	case EMCMOT_CLEAR_PROBE_FLAGS:
	    emcmotStatus->probeTripped = 0;
	    emcmotStatus->probing = 1;
	    break;

	case EMCMOT_PROBE:
	    /* most of this is taken from EMCMOT_SET_LINE */
	    /* emcmotDebug->queue up a linear move */
	    /* requires coordinated mode, enable off, not on limits */
	    if (!GET_MOTION_COORD_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in coord mode for probe move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_COMMAND;
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!inRange(emcmotCommand->pos)) {
		reportError("probe move %d out of range", emcmotCommand->id);
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else if (!checkLimits()) {
		reportError("can't do probe move with limits exceeded");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_INVALID_PARAMS;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    }

	    /* append it to the emcmotDebug->queue */
	    tpSetId(&emcmotDebug->queue, emcmotCommand->id);
	    if (-1 == tpAddLine(&emcmotDebug->queue, emcmotCommand->pos)) {
		reportError("can't add probe move");
		emcmotStatus->commandStatus = EMCMOT_COMMAND_BAD_EXEC;
		tpAbort(&emcmotDebug->queue);
		SET_MOTION_ERROR_FLAG(1);
		break;
	    } else {
		emcmotStatus->probeTripped = 0;
		emcmotStatus->probing = 1;
		SET_MOTION_ERROR_FLAG(0);
		/* set flag that indicates all axes need rehoming, if any
		   axis is moved in joint mode, for machines with no forward
		   kins */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_TELEOP_VECTOR:
	    if (!GET_MOTION_TELEOP_FLAG() || !GET_MOTION_ENABLE_FLAG()) {
		reportError
		    ("need to be enabled, in teleop mode for teleop move");
	    } else {
		double velmag;
		emcmotDebug->teleop_data.desiredVel = emcmotCommand->pos;
		pmCartMag(emcmotDebug->teleop_data.desiredVel.tran, &velmag);
		if (emcmotDebug->teleop_data.desiredVel.a > velmag) {
		    velmag = emcmotDebug->teleop_data.desiredVel.a;
		}
		if (emcmotDebug->teleop_data.desiredVel.b > velmag) {
		    velmag = emcmotDebug->teleop_data.desiredVel.b;
		}
		if (emcmotDebug->teleop_data.desiredVel.c > velmag) {
		    velmag = emcmotDebug->teleop_data.desiredVel.c;
		}
		if (velmag > emcmotConfig->limitVel) {
		    pmCartScalMult(emcmotDebug->teleop_data.desiredVel.tran,
			emcmotConfig->limitVel / velmag,
			&emcmotDebug->teleop_data.desiredVel.tran);
		    emcmotDebug->teleop_data.desiredVel.a *=
			emcmotConfig->limitVel / velmag;
		    emcmotDebug->teleop_data.desiredVel.b *=
			emcmotConfig->limitVel / velmag;
		    emcmotDebug->teleop_data.desiredVel.c *=
			emcmotConfig->limitVel / velmag;
		}
		/* flag that all joints need to be homed, if any joint is
		   jogged individually later */
		rehomeAll = 1;
	    }
	    break;

	case EMCMOT_SET_DEBUG:
	    emcmotConfig->debug = emcmotCommand->debug;
	    MARK_EMCMOT_CONFIG_CHANGE();
	    break;

	case EMCMOT_SET_AOUT:
	    tpSetAout(&emcmotDebug->queue, emcmotCommand->index,
		emcmotCommand->minLimit, emcmotCommand->maxLimit);
	    break;

	case EMCMOT_SET_DOUT:
	    tpSetDout(&emcmotDebug->queue, emcmotCommand->index,
		emcmotCommand->start, emcmotCommand->end);
	    break;

	case EMCMOT_SET_INDEX_BIT:
	    if (emcmotCommand->level) {
		/* Set bit */
		extDioWrite(emcmotCommand->index, 1);
	    } else {
		/* Clear bit */
		extDioWrite(emcmotCommand->index, 0);
	    }
	    break;

	case EMCMOT_READ_INDEX_BIT:
	    extDioRead(emcmotCommand->index, &(emcmotStatus->level));
	    break;

	case EMCMOT_SET_STEP_PARAMS:
	    MARK_EMCMOT_CONFIG_CHANGE();
	    /* configure the step pulse times */
	    axis = emcmotCommand->axis;
	    if (axis < 0 || axis >= EMCMOT_MAX_AXIS) {
		break;
	    }

	    emcmotConfig->setup_time[axis] = emcmotCommand->setup_time;
	    emcmotConfig->hold_time[axis] = emcmotCommand->hold_time;
	    if (emcmotConfig->setup_time[axis] < 1.0) {
		emcmotConfig->setup_time[axis] = 1.0;
	    }

	    if (emcmotConfig->hold_time[axis] < 1.0) {
		emcmotConfig->hold_time[axis] = 1.0;
	    }
	    break;

	default:
	    reportError("unrecognized command %d", emcmotCommand->command);
	    emcmotStatus->commandStatus = EMCMOT_COMMAND_UNKNOWN_COMMAND;
	    break;

	}			/* end of: command switch */

	/* synch tail count */
	emcmotStatus->tail = emcmotStatus->head;
	emcmotConfig->tail = emcmotConfig->head;
	emcmotDebug->tail = emcmotDebug->head;

    }
    /* end of: if-new-command */
    return 0;
}

/*
  axisComp(int axis, int dir, double nominal) looks up the real axis
  position value, given the nominal value and the direction of motion,
  based on the emcmotComp tables. It returns the comped value. If there's
  an error, the comped value is set to the nominal value. The alter value
  is added regardless of whether the comp table is active (total > 1),
  but if it is active it's added before the comping since the alter is
  a nominal delta.
*/
static double axisComp(int axis, int dir, double nominput)
{
    int index;
    double avgint;
    double compin;
    int lower, upper;
    int total;
    double *nominal;
    double *ptr;
    double denom;

    /* first adjust nominput by the alter value, before looking it up */
    nominput += emcmotComp[axis]->alter;

    total = emcmotComp[axis]->total;
    if (total < 2) {
	return nominput;
    }
    avgint = emcmotComp[axis]->avgint;
    index = (int) (nominput / avgint);
    nominal = emcmotComp[axis]->nominal;
    if (dir < 0) {
	ptr = emcmotComp[axis]->reverse;
    } else {
	ptr = emcmotComp[axis]->forward;
    }

    /* set the comp input to the nominput, by default */
    compin = nominput;
    lower = upper = 0;
    while (index >= 0 && index < total) {
	if (nominput > nominal[index]) {
	    if (index == total - 1) {
		/* off the top */
		break;
	    } else if (nominput <= nominal[index + 1]) {
		/* in range */
		lower = index;
		upper = index + 1;
		denom = nominal[upper] - nominal[lower];
		if (denom < DBL_MIN) {
		    compin = nominput;
		    /* error */
		} else {
		    compin =
			ptr[lower] + (nominput -
			nominal[lower]) * (ptr[upper] - ptr[lower]) / denom;
		}
		break;
	    } else {
		/* index too low */
		index++;
		continue;
	    }
	} else if (nominput < nominal[index]) {
	    if (index == 0) {
		/* off the bottom */
		break;
	    } else if (nominput >= nominal[index - 1]) {
		/* in range */
		lower = index - 1;
		upper = index;
		denom = nominal[upper] - nominal[lower];
		if (denom < DBL_MIN) {
		    compin = nominput;
		    /* error */
		} else {
		    compin =
			ptr[lower] + (nominput -
			nominal[lower]) * (ptr[upper] - ptr[lower]) / denom;
		}
		break;
	    } else {
		/* index too high */
		index--;
		continue;
	    }
	} else {
	    /* nominput == nominal[index], so return ptr[index] */
	    compin = ptr[index];
	    lower = index;
	    upper = index;
	    break;
	}
    }

    return compin;
}

/* FIXME-- testing */
static int debouncecount[EMCMOT_MAX_AXIS] = { 0 };

/*
  emcmotController() runs the trajectory and interpolation calculations
  each control cycle

  Inactive axes are still calculated, but the PIDs are inhibited and
  the amp enable/disable are inhibited
  */
static void emcmotController(void *arg)
{
    int first = 1;		/* true the first time thru, for initing */
    double start, end, delta;	/* time stamping */
    int homeFlag;		/* result of ext read to home switch */
    int axis;			/* axis loop counter */
    int t;			/* loop counter if we're in axis loop */
    int isLimit;		/* result of ext read to limit sw */
    int whichCycle;		/* 0=none, 1=servo, 2=traj */
    int fault;
#ifndef NO_ROUNDING
    double numres;
#endif
    double thisFerror[EMCMOT_MAX_AXIS] = { 0.0 };
    double limitFerror;		/* current scaled ferror */
    double magFerror;
    double oldbcomp;
    int retval;
    /* end of backlash stuff */

#ifdef COMPING
    int dir[EMCMOT_MAX_AXIS] = { 1 };	/* flag for direction, used for axis
					   comp */
#endif /* COMPING */

#ifdef RTAPI
    for (;;) {
#endif

	/* 
	   This function does nothing in any of the external function
	   implementations, except simmot.c extMotCycle(0); */

	/* record start time */
	start = etime();

	/* READ COMMAND: */
	emcmotCommandHandler();

	/* increment head count */
	emcmotStatus->head++;

	/* READ INPUTS: */

	/* latch all encoder feedback into raw input array, done outside of
	   for-loop on joints below, since it's a single call for all joints */
	extEncoderReadAll(EMCMOT_MAX_AXIS, emcmotDebug->rawInput);

	/* process input and read limit switches */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    /* save old cycle's values */
	    if (!first && emcmotDebug->oldInputValid[axis]) {
		emcmotDebug->oldInput[axis] = emcmotStatus->input[axis];
	    }
	    /* set input, scaled according to input = (raw - offset) / scale */
	    emcmotStatus->input[axis] =
		(emcmotDebug->rawInput[axis] -
		emcmotStatus->inputOffset[axis]) *
		emcmotDebug->inverseInputScale[axis] -
		emcmotDebug->bcompincr[axis];

#ifdef COMPING
	    /* adjust feedback using compensation tables */
	    if (GET_AXIS_HOMED_FLAG(axis)) {
		emcmotStatus->input[axis] =
		    axisComp(axis, dir[axis], emcmotStatus->input[axis]);
	    }
#endif
	    if (first || !emcmotDebug->oldInputValid[axis]) {
		emcmotDebug->oldInput[axis] = emcmotStatus->input[axis];
		emcmotDebug->oldInputValid[axis] = 1;
		first = 0;
	    }

	    /* debounce bad feedback */
#ifndef SIMULATED_MOTORS
	    if (fabs(emcmotStatus->input[axis] -
		    emcmotDebug->oldInput[axis]) /
		emcmotConfig->servoCycleTime > emcmotDebug->bigVel[axis]) {
		/* bad input value-- interpolate last value, up to max
		   debounces, then hold it */
		if (++positionInputDebounce[axis] > POSITION_INPUT_DEBOUNCE) {
		    /* we haven't exceeded max number of debounces allowed,
		       so interpolate off the velocity estimate */
		    emcmotStatus->input[axis] = emcmotDebug->oldInput[axis] +
			emcmotDebug->jointVel[axis] *
			emcmotConfig->servoCycleTime;
		} else {
		    /* we've exceeded the max number of debounces allowed, so
		       hold position. We should flag an error here, abort the
		       move, disable motion, etc. but for now we'll rely on
		       following error to do this */
		    emcmotStatus->input[axis] = emcmotDebug->oldInput[axis];
		}
		/* FIXME-- testing */
		debouncecount[axis]++;
	    } else {
		/* no debounce needed, so reset debounce counter */
		positionInputDebounce[axis] = 0;
	    }
#endif

	    /* read limit switches and amp fault from external interface, and 
	       set 'emcmotDebug->enabling' to zero if tripped to cause
	       immediate stop */

	    /* handle limits */
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		extMaxLimitSwitchRead(axis, &isLimit);
		if (isLimit == GET_AXIS_PHL_POLARITY(axis)) {
		    if (++emcmotDebug->maxLimitSwitchCount[axis] >
			LIMIT_SWITCH_DEBOUNCE) {
			SET_AXIS_PHL_FLAG(axis, 1);
			emcmotDebug->maxLimitSwitchCount[axis] =
			    LIMIT_SWITCH_DEBOUNCE;
			if (emcmotStatus->overrideLimits || isHoming()) {
			} else {
			    SET_AXIS_ERROR_FLAG(axis, 1);
			    emcmotDebug->enabling = 0;
			}
		    }
		} else {
		    SET_AXIS_PHL_FLAG(axis, 0);
		    emcmotDebug->maxLimitSwitchCount[axis] = 0;
		}
	    }
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		extMinLimitSwitchRead(axis, &isLimit);
		if (isLimit == GET_AXIS_NHL_POLARITY(axis)) {
		    if (++emcmotDebug->minLimitSwitchCount[axis] >
			LIMIT_SWITCH_DEBOUNCE) {
			SET_AXIS_NHL_FLAG(axis, 1);
			emcmotDebug->minLimitSwitchCount[axis] =
			    LIMIT_SWITCH_DEBOUNCE;
			if (emcmotStatus->overrideLimits || isHoming()) {
			} else {
			    SET_AXIS_ERROR_FLAG(axis, 1);
			    emcmotDebug->enabling = 0;
			}
		    }
		} else {
		    SET_AXIS_NHL_FLAG(axis, 0);
		    emcmotDebug->minLimitSwitchCount[axis] = 0;
		}
	    }

	    if (GET_AXIS_ACTIVE_FLAG(axis) && GET_AXIS_ENABLE_FLAG(axis)) {
		extAmpFault(axis, &fault);
		if (fault == GET_AXIS_FAULT_POLARITY(axis)) {
		    if (++emcmotDebug->ampFaultCount[axis] >
			AMP_FAULT_DEBOUNCE) {
			SET_AXIS_ERROR_FLAG(axis, 1);
			SET_AXIS_FAULT_FLAG(axis, 1);
			emcmotDebug->ampFaultCount[axis] = AMP_FAULT_DEBOUNCE;
			emcmotDebug->enabling = 0;
		    }
		} else {
		    SET_AXIS_FAULT_FLAG(axis, 0);
		    emcmotDebug->ampFaultCount[axis] = 0;
		}
	    }

	    /* read home switch and set flag if tripped. Handling of home
	       sequence is done later. */
	    if (GET_AXIS_ACTIVE_FLAG(axis)) {
		extHomeSwitchRead(axis, &homeFlag);
		if (homeFlag == GET_AXIS_HOME_SWITCH_POLARITY(axis)) {
		    SET_AXIS_HOME_SWITCH_FLAG(axis, 1);
		} else {
		    SET_AXIS_HOME_SWITCH_FLAG(axis, 0);
		}
	    }

	}			/* end of: loop on axes, for reading inputs,
				   setting limit and home switch flags */

	/* check to see if logging should be triggered */
	if (emcmotStatus->logOpen &&
	    !emcmotStatus->logStarted &&
	    loggingAxis >= 0 && loggingAxis < EMCMOT_MAX_AXIS) {
	    double val = 0.0;

	    switch (emcmotStatus->logTriggerVariable) {
	    case EMCLOG_TRIGGER_ON_FERROR:
		val = emcmotDebug->ferrorCurrent[loggingAxis];
		break;

	    case EMCLOG_TRIGGER_ON_VOLT:
		val = emcmotDebug->rawOutput[loggingAxis];
		break;

	    case EMCLOG_TRIGGER_ON_POS:
		val = emcmotDebug->jointPos[loggingAxis];
		break;

	    case EMCLOG_TRIGGER_ON_VEL:
		val =
		    emcmotDebug->jointPos[loggingAxis] -
		    emcmotDebug->oldJointPos[loggingAxis];
		break;

	    default:
		break;
	    }

	    switch (emcmotStatus->logTriggerType) {
	    case EMCLOG_MANUAL_TRIGGER:
		break;

	    case EMCLOG_DELTA_TRIGGER:
		if (emcmotStatus->logStartVal - val <
		    -emcmotStatus->logTriggerThreshold
		    || emcmotStatus->logStartVal - val >
		    emcmotStatus->logTriggerThreshold) {
		    emcmotStatus->logStarted = 1;
		}
		break;

	    case EMCLOG_OVER_TRIGGER:
		if (val > emcmotStatus->logTriggerThreshold) {
		    emcmotStatus->logStarted = 1;
		}
		break;

	    case EMCLOG_UNDER_TRIGGER:
		if (val < emcmotStatus->logTriggerThreshold) {
		    emcmotStatus->logStarted = 1;
		}
	    }
	}

	/* now we're outside the axis loop, having just read input, scaled
	   it, read the limit and home switches and amp faults. We need to
	   abort all motion if we're on limits, handle homing sequence, and
	   handle mode and state transitions. */

	/* RUN STATE LOGIC: */

	/* check for disabling */
	if (!emcmotDebug->enabling && GET_MOTION_ENABLE_FLAG()) {
	    /* clear out the motion emcmotDebug->queue and interpolators */
	    tpClear(&emcmotDebug->queue);
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpClear(&emcmotDebug->freeAxis[axis]);
		cubicDrain(&emcmotDebug->cubic[axis]);
		if (GET_AXIS_ACTIVE_FLAG(axis)) {
		    extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
		    SET_AXIS_ENABLE_FLAG(axis, 0);
		    SET_AXIS_HOMING_FLAG(axis, 0);
		    emcmotStatus->output[axis] = 0.0;
		}
		/* don't clear the axis error flag, since that may signify
		   why we just went into disabled state */
	    }
	    /* reset the trajectory interpolation counter, so that the
	       kinematics functions called during the disabled state run at
	       the nominal trajectory rate rather than the servo rate. It's
	       loaded with emcmotConfig->interpolationRate when it goes to
	       zero. */
	    interpolationCounter = 0;
	    SET_MOTION_ENABLE_FLAG(0);
	    /* don't clear the motion error flag, since that may signify why
	       we just went into disabled state */
	}

	/* check for emcmotDebug->enabling */
	if (emcmotDebug->enabling && !GET_MOTION_ENABLE_FLAG()) {
	    tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		emcmotDebug->freePose.tran.x = emcmotDebug->jointPos[axis];
		tpSetPos(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
		pidReset(&emcmotConfig->pid[axis]);
		if (GET_AXIS_ACTIVE_FLAG(axis)) {
		    extAmpEnable(axis, GET_AXIS_ENABLE_POLARITY(axis));
		    SET_AXIS_ENABLE_FLAG(axis, 1);
		    SET_AXIS_HOMING_FLAG(axis, 0);
		}
		/* clear any outstanding axis errors when going into enabled
		   state */
		SET_AXIS_ERROR_FLAG(axis, 0);
	    }
	    SET_MOTION_ENABLE_FLAG(1);
	    /* clear any outstanding motion errors when going into enabled
	       state */
	    SET_MOTION_ERROR_FLAG(0);

	    /* init min-max-avg stats */
	    mmxavgInit(&emcmotDebug->tMmxavg, emcmotDebug->tMmxavgSpace,
		MMXAVG_SIZE);
	    mmxavgInit(&emcmotDebug->sMmxavg, emcmotDebug->sMmxavgSpace,
		MMXAVG_SIZE);
	    mmxavgInit(&emcmotDebug->yMmxavg, emcmotDebug->yMmxavgSpace,
		MMXAVG_SIZE);
	    mmxavgInit(&emcmotDebug->fMmxavg, emcmotDebug->fMmxavgSpace,
		MMXAVG_SIZE);
	    mmxavgInit(&emcmotDebug->fyMmxavg, emcmotDebug->fyMmxavgSpace,
		MMXAVG_SIZE);
	    emcmotDebug->tMin = 0.0;
	    emcmotDebug->tMax = 0.0;
	    emcmotDebug->tAvg = 0.0;
	    emcmotDebug->sMin = 0.0;
	    emcmotDebug->sMax = 0.0;
	    emcmotDebug->sAvg = 0.0;
	    emcmotDebug->yMin = 0.0;
	    emcmotDebug->yMax = 0.0;
	    emcmotDebug->yAvg = 0.0;
	    emcmotDebug->fyMin = 0.0;
	    emcmotDebug->fyMax = 0.0;
	    emcmotDebug->fyAvg = 0.0;
	    emcmotDebug->fMin = 0.0;
	    emcmotDebug->fMax = 0.0;
	    emcmotDebug->fAvg = 0.0;
	}

	/* check for entering teleop mode */
	if (emcmotDebug->teleoperating && !GET_MOTION_TELEOP_FLAG()) {
	    if (GET_MOTION_INPOS_FLAG()) {

		/* update coordinated emcmotDebug->queue position */
		tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
		/* drain the cubics so they'll synch up */
		for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		    cubicDrain(&emcmotDebug->cubic[axis]);
		}
		/* Initialize things to do when starting teleop mode. */
		emcmotDebug->teleop_data.currentVel.tran.x = 0.0;
		emcmotDebug->teleop_data.currentVel.tran.y = 0.0;
		emcmotDebug->teleop_data.currentVel.tran.z = 0.0;
		emcmotDebug->teleop_data.currentVel.a = 0.0;
		emcmotDebug->teleop_data.currentVel.b = 0.0;
		emcmotDebug->teleop_data.currentVel.c = 0.0;
		emcmotDebug->teleop_data.desiredVel.tran.x = 0.0;
		emcmotDebug->teleop_data.desiredVel.tran.y = 0.0;
		emcmotDebug->teleop_data.desiredVel.tran.z = 0.0;
		emcmotDebug->teleop_data.desiredVel.a = 0.0;
		emcmotDebug->teleop_data.desiredVel.b = 0.0;
		emcmotDebug->teleop_data.desiredVel.c = 0.0;
		emcmotDebug->teleop_data.currentAccell.tran.x = 0.0;
		emcmotDebug->teleop_data.currentAccell.tran.y = 0.0;
		emcmotDebug->teleop_data.currentAccell.tran.z = 0.0;
		emcmotDebug->teleop_data.currentAccell.a = 0.0;
		emcmotDebug->teleop_data.currentAccell.b = 0.0;
		emcmotDebug->teleop_data.currentAccell.c = 0.0;
		emcmotDebug->teleop_data.desiredAccell.tran.x = 0.0;
		emcmotDebug->teleop_data.desiredAccell.tran.y = 0.0;
		emcmotDebug->teleop_data.desiredAccell.tran.z = 0.0;
		emcmotDebug->teleop_data.desiredAccell.a = 0.0;
		emcmotDebug->teleop_data.desiredAccell.b = 0.0;
		emcmotDebug->teleop_data.desiredAccell.c = 0.0;
		SET_MOTION_TELEOP_FLAG(1);
		SET_MOTION_ERROR_FLAG(0);
	    } else {
		/* not in position-- don't honor mode change */
		emcmotDebug->teleoperating = 0;
	    }
	} else {
	    if (GET_MOTION_INPOS_FLAG()) {
		if (!emcmotDebug->teleoperating && GET_MOTION_TELEOP_FLAG()) {
		    SET_MOTION_TELEOP_FLAG(0);
		    if (!emcmotDebug->coordinating) {
			for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			    /* update free planner positions */
			    emcmotDebug->freePose.tran.x =
				emcmotDebug->jointPos[axis];
			    tpSetPos(&emcmotDebug->freeAxis[axis],
				emcmotDebug->freePose);
			    /* drain the cubics so they'll synch up */
			    cubicDrain(&emcmotDebug->cubic[axis]);
			}
		    }
		}
	    }

	    /* check for entering coordinated mode */
	    if (emcmotDebug->coordinating && !GET_MOTION_COORD_FLAG()) {
		if (GET_MOTION_INPOS_FLAG()) {
		    /* update coordinated emcmotDebug->queue position */
		    tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
		    /* drain the cubics so they'll synch up */
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			cubicDrain(&emcmotDebug->cubic[axis]);
		    }
		    /* clear the override limits flags */
		    emcmotDebug->overriding = 0;
		    emcmotStatus->overrideLimits = 0;
		    SET_MOTION_COORD_FLAG(1);
		    SET_MOTION_TELEOP_FLAG(0);
		    SET_MOTION_ERROR_FLAG(0);
		} else {
		    /* not in position-- don't honor mode change */
		    emcmotDebug->coordinating = 0;
		}
	    }

	    /* check entering free space mode */
	    if (!emcmotDebug->coordinating && GET_MOTION_COORD_FLAG()) {
		if (GET_MOTION_INPOS_FLAG()) {
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			/* update free planner positions */
			emcmotDebug->freePose.tran.x =
			    emcmotDebug->jointPos[axis];
			tpSetPos(&emcmotDebug->freeAxis[axis],
			    emcmotDebug->freePose);
			/* drain the cubics so they'll synch up */
			cubicDrain(&emcmotDebug->cubic[axis]);
		    }
		    SET_MOTION_COORD_FLAG(0);
		    SET_MOTION_TELEOP_FLAG(0);
		    SET_MOTION_ERROR_FLAG(0);
		} else {
		    /* not in position-- don't honor mode change */
		    emcmotDebug->coordinating = 1;
		}
	    }

	}

	/* check for homing sequences */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    if (GET_AXIS_HOMING_FLAG(axis)) {
		if (tpIsDone(&emcmotDebug->freeAxis[axis])) {
		    /* check for decel or final move */
		    if (emcmotDebug->homingPhase[axis] == 5) {
			/* reset flag-- we're back at home */
			emcmotDebug->homingPhase[axis] = 0;

			/* rework the home kinematics values-- this could be
			   done after each message to set world or joint
			   kinematics, but we'll defer it to here so it
			   happens just prior to when it's needed. Note that
			   the nominal value of these may be changed, if the
			   kinematics need to. */
			kinematicsHome(&worldHome, emcmotDebug->jointHome,
			    &fflags, &iflags);

			/* clear flag that indicates all joints need rehoming 
			   if any joint is moved, for machines with no
			   forward kins */
			rehomeAll = 0;

			/* set input offset to value such that resulting
			   input is the emcmotDebug->jointHome[] value, with:
			   input = (raw - offset) / scale -> offset = raw -
			   input * scale -> offset = raw -
			   emcmotDebug->jointHome * scale */

			emcmotStatus->inputOffset[axis] =
			    emcmotDebug->rawInput[axis] -
			    emcmotDebug->jointHome[axis] *
			    emcmotStatus->inputScale[axis];

			/* recompute inputs to match so we don't have a
			   momentary jump */
			emcmotStatus->input[axis] =
			    emcmotDebug->jointHome[axis];
			emcmotDebug->oldInput[axis] =
			    emcmotStatus->input[axis];

			/* reset interpolator so that it doesn't jump */
			cubicOffset(&emcmotDebug->cubic[axis],
			    emcmotDebug->jointHome[axis] -
			    emcmotDebug->coarseJointPos[axis]);

			/* set axis position to emcmotDebug->jointHome upon
			   homing */
			emcmotDebug->jointPos[axis] =
			    emcmotDebug->jointHome[axis];
			emcmotDebug->coarseJointPos[axis] =
			    emcmotDebug->jointHome[axis];
			emcmotDebug->freePose.tran.x =
			    emcmotDebug->jointHome[axis];
			tpSetPos(&emcmotDebug->freeAxis[axis],
			    emcmotDebug->freePose);

			SET_AXIS_HOMING_FLAG(axis, 0);
			SET_AXIS_HOMED_FLAG(axis, 1);

			/* set emcmotDebug->allHomed flag if all active axes
			   are homed; this will signify that kinematics
			   functions can be called */
			emcmotDebug->allHomed = 1;
			for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
			    if (GET_AXIS_ACTIVE_FLAG(t) &&
				!GET_AXIS_HOMED_FLAG(t)) {
				/* one active axis is not homed */
				emcmotDebug->allHomed = 0;
				break;
			    }
			}
			/* FIXME-- this only works with no forward kins */
			if (emcmotDebug->allHomed) {
			    emcmotStatus->pos = worldHome;
			    emcmotStatus->actualPos = worldHome;
			}
		    }

		    if (emcmotDebug->homingPhase[axis] == 4) {
			/* just finished decel, now we'll do final home */
			/* add move back to latched position + backoff */
			emcmotDebug->freePose.tran.x =
			    (emcmotDebug->saveLatch[axis] -
			    emcmotStatus->inputOffset[axis]) *
			    emcmotDebug->inverseInputScale[axis] +
			    emcmotConfig->homeOffset[axis];
			/* Note that I put a multiplication factor of 2 in
			   front of the homing velocity below. The reason is
			   that, I think, if you found the index pulse you
			   know your exact position it is save to travel with
			   higher speeds. In addition to that, you actually
			   see that the machine has found its index pulse for
			   the specified axis */
			tpSetVmax(&emcmotDebug->freeAxis[axis],
			    2 * (emcmotConfig->homingVel[axis]));
			if ((retval =
				tpAddLine(&emcmotDebug->freeAxis[axis],
				    emcmotDebug->freePose)) == 0) {
			    /* Advance homing sequence only if motion is
			       added to the tp */
			    emcmotDebug->homingPhase[axis] = 5;
			}
		    }
		}		/* end of: if axis is done either decel or
				   final home */
	    }			/* end of: if axis is homing */
	}			/* end of: axis loop that checks for homing */

	/* RUN MOTION CALCULATIONS: */

	/* run axis interpolations and outputs, but only if we're enabled.
	   This section is "suppressed" if we're not enabled, although the
	   read/write of encoders/dacs is still done. */
	whichCycle = 0;
	if (GET_MOTION_ENABLE_FLAG()) {
	    /* set whichCycle to be at least a servo cycle, for calc time
	       logging */
	    whichCycle = 1;

	    /* check to see if the interpolators are empty */
	    while (cubicNeedNextPoint(&emcmotDebug->cubic[0])) {
		/* they're empty, so pull next point(s) off Cartesian or
		   joint planner, depending upon coord or free mode. */

		/* check to see whether we're in teleop, coordinated or free
		   mode, to decide which motion planner to call */
		if (GET_MOTION_TELEOP_FLAG()) {
		    double accell_mag;

		    emcmotDebug->teleop_data.desiredAccell.tran.x =
			(emcmotDebug->teleop_data.desiredVel.tran.x -
			emcmotDebug->teleop_data.currentVel.tran.x) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.desiredAccell.tran.y =
			(emcmotDebug->teleop_data.desiredVel.tran.y -
			emcmotDebug->teleop_data.currentVel.tran.y) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.desiredAccell.tran.z =
			(emcmotDebug->teleop_data.desiredVel.tran.z -
			emcmotDebug->teleop_data.currentVel.tran.z) /
			emcmotConfig->trajCycleTime;
		    pmCartMag(emcmotDebug->teleop_data.desiredAccell.tran,
			&accell_mag);
		    emcmotDebug->teleop_data.desiredAccell.a =
			(emcmotDebug->teleop_data.desiredVel.a -
			emcmotDebug->teleop_data.currentVel.a) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.desiredAccell.b =
			(emcmotDebug->teleop_data.desiredVel.b -
			emcmotDebug->teleop_data.currentVel.b) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->teleop_data.desiredAccell.c =
			(emcmotDebug->teleop_data.desiredVel.c -
			emcmotDebug->teleop_data.currentVel.c) /
			emcmotConfig->trajCycleTime;
		    if (emcmotDebug->teleop_data.desiredAccell.a > accell_mag) {
			accell_mag = emcmotDebug->teleop_data.desiredAccell.a;
		    }
		    if (emcmotDebug->teleop_data.desiredAccell.b > accell_mag) {
			accell_mag = emcmotDebug->teleop_data.desiredAccell.b;
		    }
		    if (emcmotDebug->teleop_data.desiredAccell.c > accell_mag) {
			accell_mag = emcmotDebug->teleop_data.desiredAccell.c;
		    }
		    if (accell_mag > emcmotStatus->acc) {
			pmCartScalMult(emcmotDebug->teleop_data.desiredAccell.
			    tran, emcmotStatus->acc / accell_mag,
			    &emcmotDebug->teleop_data.currentAccell.tran);
			emcmotDebug->teleop_data.currentAccell.a =
			    emcmotDebug->teleop_data.desiredAccell.a *
			    emcmotStatus->acc / accell_mag;
			emcmotDebug->teleop_data.currentAccell.b =
			    emcmotDebug->teleop_data.desiredAccell.b *
			    emcmotStatus->acc / accell_mag;
			emcmotDebug->teleop_data.currentAccell.c =
			    emcmotDebug->teleop_data.desiredAccell.c *
			    emcmotStatus->acc / accell_mag;
			emcmotDebug->teleop_data.currentVel.tran.x +=
			    emcmotDebug->teleop_data.currentAccell.tran.x *
			    emcmotConfig->trajCycleTime;
			emcmotDebug->teleop_data.currentVel.tran.y +=
			    emcmotDebug->teleop_data.currentAccell.tran.y *
			    emcmotConfig->trajCycleTime;
			emcmotDebug->teleop_data.currentVel.tran.z +=
			    emcmotDebug->teleop_data.currentAccell.tran.z *
			    emcmotConfig->trajCycleTime;
			emcmotDebug->teleop_data.currentVel.a +=
			    emcmotDebug->teleop_data.currentAccell.a *
			    emcmotConfig->trajCycleTime;
			emcmotDebug->teleop_data.currentVel.b +=
			    emcmotDebug->teleop_data.currentAccell.b *
			    emcmotConfig->trajCycleTime;
			emcmotDebug->teleop_data.currentVel.c +=
			    emcmotDebug->teleop_data.currentAccell.c *
			    emcmotConfig->trajCycleTime;
		    } else {
			emcmotDebug->teleop_data.currentAccell =
			    emcmotDebug->teleop_data.desiredAccell;
			emcmotDebug->teleop_data.currentVel =
			    emcmotDebug->teleop_data.desiredVel;
		    }

		    emcmotStatus->pos.tran.x +=
			emcmotDebug->teleop_data.currentVel.tran.x *
			emcmotConfig->trajCycleTime;
		    emcmotStatus->pos.tran.y +=
			emcmotDebug->teleop_data.currentVel.tran.y *
			emcmotConfig->trajCycleTime;
		    emcmotStatus->pos.tran.z +=
			emcmotDebug->teleop_data.currentVel.tran.z *
			emcmotConfig->trajCycleTime;
		    emcmotStatus->pos.a +=
			emcmotDebug->teleop_data.currentVel.a *
			emcmotConfig->trajCycleTime;
		    emcmotStatus->pos.b +=
			emcmotDebug->teleop_data.currentVel.b *
			emcmotConfig->trajCycleTime;
		    emcmotStatus->pos.c +=
			emcmotDebug->teleop_data.currentVel.c *
			emcmotConfig->trajCycleTime;

		    /* convert to joints */
		    kinematicsInverse(&emcmotStatus->pos,
			emcmotDebug->coarseJointPos, &iflags, &fflags);

		    /* spline joints up-- note that we may be adding points
		       that fail soft limits, but we'll abort at the end of
		       this cycle so it doesn't really matter */
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			cubicAddPoint(&emcmotDebug->cubic[axis],
			    emcmotDebug->coarseJointPos[axis]);
		    }

		    if (kinType == KINEMATICS_IDENTITY) {
			/* call forward kinematics on input points for actual 
			   pos, at trajectory rate to save bandwidth */
			kinematicsForward(emcmotStatus->input,
			    &emcmotStatus->actualPos, &fflags, &iflags);
		    } else {
			/* fake it by setting actual pos to commanded pos */
			emcmotStatus->actualPos = emcmotStatus->pos;
		    }
		} else {
		    if (GET_MOTION_COORD_FLAG()) {
			/* we're in coordinated mode-- pull a pose off the
			   Cartesian trajectory planner, run it through the
			   inverse kinematics, and spline up the joint points 
			   for interpolation in servo cycles. */

			/* set whichCycle to be a Cartesian trajectory cycle,
			   for calc time logging */
			whichCycle = 2;

			/* run coordinated trajectory planning cycle */
			tpRunCycle(&emcmotDebug->queue);

			/* set new commanded traj pos */
			emcmotStatus->pos = tpGetPos(&emcmotDebug->queue);

			/* convert to joints */
			kinematicsInverse(&emcmotStatus->pos,
			    emcmotDebug->coarseJointPos, &iflags, &fflags);

			/* spline joints up-- note that we may be adding
			   points that fail soft limits, but we'll abort at
			   the end of this cycle so it doesn't really matter */
			for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			    cubicAddPoint(&emcmotDebug->cubic[axis],
				emcmotDebug->coarseJointPos[axis]);
			}

			if (kinType == KINEMATICS_IDENTITY) {
			    /* call forward kinematics on input points for
			       actual pos, at trajectory rate to save
			       bandwidth */
			    kinematicsForward(emcmotStatus->input,
				&emcmotStatus->actualPos, &fflags, &iflags);
			} else {
			    /* fake it by setting actual pos to commanded pos 
			     */
			    emcmotStatus->actualPos = emcmotStatus->pos;
			}

			/* now emcmotStatus->actualPos, emcmotStatus->pos,
			   and emcmotDebug->coarseJointPos[] are set */

		    } /* end of: coord mode */
		    else {
			/* we're in free mode-- run joint planning cycles */
			for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			    /* set whichCycle to be a joint trajectory cycle,
			       for calc time logging */
			    /* note that this may include one or more joint
			       trajectory cycles, so calc time may be
			       inherently variable */
			    whichCycle = 2;

			    /* run joint trajectory planning cycle */
			    tpRunCycle(&emcmotDebug->freeAxis[axis]);

			    /* set new coarse joint position. FIXME-- this
			       uses only the tran.x field of the TP_STRUCT,
			       which is overkill. We need a TP_STRUCT with a
			       single scalar element. */
			    emcmotDebug->coarseJointPos[axis] =
				tpGetPos(&emcmotDebug->freeAxis[axis]).tran.x;

			    /* spline joint up-- note that we may be adding a 
			       point that fails soft limit, but we'll abort
			       at the end of this cycle so it doesn't really
			       matter */
			    cubicAddPoint(&emcmotDebug->cubic[axis],
				emcmotDebug->coarseJointPos[axis]);
			}	/* end of: axis for loop for joint planning
				   cycle */

			if (kinType == KINEMATICS_IDENTITY) {
			    /* set actualPos from actual inputs */
			    kinematicsForward(emcmotStatus->input,
				&emcmotStatus->actualPos, &fflags, &iflags);
			    /* set pos from nominal joints, since we're in
			       joint mode */
			    kinematicsForward(emcmotDebug->coarseJointPos,
				&emcmotStatus->pos, &fflags, &iflags);
			} else if (kinType != KINEMATICS_INVERSE_ONLY) {
			    /* here is where we call the forward kinematics
			       repeatedly, when we're in free mode, so that
			       the world coordinates are kept up to date when 
			       joints are moving. This is only done if we
			       have the kinematics. emcmotStatus->pos needs
			       to be set with an estimate for the kinematics
			       to converge, which is true when we enter free
			       mode from coordinated mode or after the
			       machine is homed. */
			    EmcPose temp = emcmotStatus->pos;
			    if (0 == kinematicsForward(emcmotStatus->input,
				    &temp, &fflags, &iflags)) {
				emcmotStatus->pos = temp;
				emcmotStatus->actualPos = temp;
			    }
			    /* else leave them alone */
			} else {
			    /* no foward kins, and we're in joint mode, so we 
			       have no estimate of world coords, and we have
			       to leave them alone */
			}

			/* now emcmotStatus->actualPos, emcmotStatus->pos,
			   and emcmotDebug->coarseJointPos[] are set */

		    }		/* end of: free mode trajectory planning */
		}		/* end of: not teleop mode */
	    }			/* end of: while (cubicNeedNextPoint(0)) */

	    /* we're still in motion enabled section. For coordinated mode,
	       the Cartesian trajectory cycle has been computed, if
	       necessary, run through the inverse kinematics, and the joints
	       have been splined up for interpolation. For free mode, the
	       joint trajectory cycles have been computed, if necessary, and
	       the joints have been splined up for interpolation. We still
	       need to push the actual input through the forward kinematics,
	       for actual pos.

	       Effects:

	       For coord mode, emcmotStatus->pos contains the commanded
	       Cartesian pose, emcmotDebug->coarseJointPos[] contains the
	       results of the inverse kinematics at the coarse (trajectory)
	       rate, and the interpolators are not empty.

	       For free mode, emcmotStatus->pos is unchanged, and needs to be 
	       updated via the forward kinematics. FIXME-- make sure this
	       happens, and note where in this comment.
	       emcmotDebug->coarseJointPos[] contains the results of the
	       joint trajectory calculations at the coarse (trajectory) rate, 
	       and the interpolators are not empty. */

	    /* check for soft joint limits. If so, abort all motion. The
	       interpolators will pick this up further down and begin
	       planning abort and stop. */
	    emcmotDebug->onLimit = 0;
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		SET_AXIS_PSL_FLAG(axis, 0);
		SET_AXIS_NSL_FLAG(axis, 0);
		if (GET_AXIS_HOMED_FLAG(axis)) {
		    if (emcmotDebug->coarseJointPos[axis] >
			emcmotConfig->maxLimit[axis]) {
			SET_AXIS_ERROR_FLAG(axis, 1);
			SET_AXIS_PSL_FLAG(axis, 1);
			emcmotDebug->onLimit = 1;
		    } else if (emcmotDebug->coarseJointPos[axis] <
			emcmotConfig->minLimit[axis]) {
			SET_AXIS_ERROR_FLAG(axis, 1);
			SET_AXIS_NSL_FLAG(axis, 1);
		    }
		}
	    }

	    /* reset emcmotDebug->wasOnLimit flag iff all joints are free of
	       soft limits, as seen in the flag bits set last cycle. No need
	       to do this for hard limits, since emcmotDebug->wasOnLimit only 
	       prevents flurry of aborts while on a soft limit and hard
	       limits don't abort, they disable. */
	    if (emcmotDebug->onLimit) {
		if (!emcmotDebug->wasOnLimit) {
		    /* abort everything, regardless of coord or free mode */
		    tpAbort(&emcmotDebug->queue);
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			tpAbort(&emcmotDebug->freeAxis[axis]);
			emcmotDebug->wasOnLimit = 1;
		    }
		}
		/* else we were on a limit, so inhibit firing of aborts */
	    } else {
		/* not on a limit, so clear emcmotDebug->wasOnLimit so aborts 
		   fire next time we are on a limit */
		emcmotDebug->wasOnLimit = 0;
	    }

	    if (whichCycle == 2) {
		/* we're on a trajectory cycle, either Cartesian or joint
		   planning, so log per-traj-cycle data if logging active */
		logIt = 0;

		if (emcmotStatus->logStarted && emcmotStatus->logSkip >= 0) {

		    /* calculate current vel, accel, for logging */
		    emcmotDebug->newVel.tran.x =
			(emcmotStatus->pos.tran.x -
			emcmotDebug->oldPos.tran.x) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->newVel.tran.y =
			(emcmotStatus->pos.tran.y -
			emcmotDebug->oldPos.tran.y) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->newVel.tran.z =
			(emcmotStatus->pos.tran.z -
			emcmotDebug->oldPos.tran.z) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->oldPos = emcmotStatus->pos;
		    emcmotDebug->newAcc.tran.x =
			(emcmotDebug->newVel.tran.x -
			emcmotDebug->oldVel.tran.x) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->newAcc.tran.y =
			(emcmotDebug->newVel.tran.y -
			emcmotDebug->oldVel.tran.y) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->newAcc.tran.z =
			(emcmotDebug->newVel.tran.z -
			emcmotDebug->oldVel.tran.z) /
			emcmotConfig->trajCycleTime;
		    emcmotDebug->oldVel = emcmotDebug->newVel;

		    /* save the type with the log item */
		    ls.type = emcmotStatus->logType;

		    /* now log type-specific data */
		    switch (emcmotStatus->logType) {

		    case EMCMOT_LOG_TYPE_TRAJ_POS:
			if (logSkip-- <= 0) {
			    ls.item.trajPos.time = start - logStartTime;
			    ls.item.trajPos.pos = emcmotStatus->pos.tran;
			    logSkip = emcmotStatus->logSkip;
			    logIt = 1;
			}
			break;

		    case EMCMOT_LOG_TYPE_TRAJ_VEL:
			if (logSkip-- <= 0) {
			    ls.item.trajVel.time = start - logStartTime;
			    ls.item.trajVel.vel = emcmotDebug->newVel.tran;
			    pmCartMag(emcmotDebug->newVel.tran,
				&ls.item.trajVel.mag);
			    logSkip = emcmotStatus->logSkip;
			    logIt = 1;
			}
			break;

		    case EMCMOT_LOG_TYPE_TRAJ_ACC:
			if (logSkip-- <= 0) {
			    ls.item.trajAcc.time = start - logStartTime;
			    ls.item.trajAcc.acc = emcmotDebug->newAcc.tran;
			    pmCartMag(emcmotDebug->newAcc.tran,
				&ls.item.trajAcc.mag);
			    logSkip = emcmotStatus->logSkip;
			    logIt = 1;
			}
			break;

		    default:
			break;
		    }		/* end of: switch on log type */

		    /* now log it */
		    if (logIt) {
			emcmotLogAdd(emcmotLog, ls);
			emcmotStatus->logPoints = emcmotLog->howmany;
			logIt = 0;
		    }
		}		/* end of: if (emcmotStatus->logStarted &&
				   emcmotStatus->logSkip >= 0) */
	    }

	    /* end of: if (whichCycle == 2), for trajectory cycle logging */
	    /* run interpolation and compensation */
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		/* interpolate */
		emcmotDebug->oldJointPos[axis] = emcmotDebug->jointPos[axis];
		emcmotDebug->jointPos[axis] =
		    cubicInterpolate(&emcmotDebug->cubic[axis], 0, 0, 0, 0);
		emcmotDebug->jointVel[axis] =
		    (emcmotDebug->jointPos[axis] -
		    emcmotDebug->oldJointPos[axis]) /
		    emcmotConfig->servoCycleTime;
#ifdef COMPING
		/* set direction flag iff there's a direction change,
		   otherwise leave it alone so that stops won't change dir */
		if (emcmotDebug->jointVel[axis] > 0.0 && dir[axis] < 0) {
		    dir[axis] = 1;
		} else if (emcmotDebug->jointVel[axis] < 0.0 && dir[axis] > 0) {
		    dir[axis] = -1;
		}
		/* else leave it alone */
#endif /* COMPING */

		/* run output calculations */
		if (GET_AXIS_ACTIVE_FLAG(axis)) {
		    /* BACKLASH COMPENSATION */
		    /* FIXME-- make backlash part of the EMC status proper,
		       not the PID structure */
		    if (emcmotDebug->jointPos[axis] -
			emcmotDebug->oldJointPos[axis] > 0.0) {
			oldbcomp = emcmotDebug->bcomp[axis];
			emcmotDebug->bcomp[axis] =
			    +emcmotConfig->pid[axis].backlash / 2.0;
			if (emcmotDebug->bcompdir[axis] != +1) {
			    emcmotDebug->bac_done[axis] = 0;
			    emcmotDebug->bac_di[axis] =
				emcmotDebug->bcompincr[axis];
			    emcmotDebug->bac_d[axis] = 0;
			    emcmotDebug->bac_D[axis] =
				emcmotDebug->bcomp[axis] - oldbcomp;
			    emcmotDebug->bac_halfD[axis] =
				0.5 * emcmotDebug->bac_D[axis];
			    emcmotDebug->bac_incrincr[axis] =
				emcmotStatus->acc *
				emcmotConfig->servoCycleTime *
				emcmotConfig->servoCycleTime;
			    emcmotDebug->bac_incr[axis] =
				-0.5 * emcmotDebug->bac_incrincr[axis];
			    emcmotDebug->bcompdir[axis] = +1;
			}
		    } else if (emcmotDebug->jointPos[axis] -
			emcmotDebug->oldJointPos[axis] < 0.0) {
			oldbcomp = emcmotDebug->bcomp[axis];
			emcmotDebug->bcomp[axis] =
			    -emcmotConfig->pid[axis].backlash / 2.0;
			if (emcmotDebug->bcompdir[axis] != -1) {
			    emcmotDebug->bac_done[axis] = 0;
			    emcmotDebug->bac_di[axis] =
				emcmotDebug->bcompincr[axis];
			    emcmotDebug->bac_d[axis] = 0;
			    emcmotDebug->bac_D[axis] =
				emcmotDebug->bcomp[axis] - oldbcomp;
			    emcmotDebug->bac_halfD[axis] =
				0.5 * emcmotDebug->bac_D[axis];
			    emcmotDebug->bac_incrincr[axis] =
				emcmotStatus->acc *
				emcmotConfig->servoCycleTime *
				emcmotConfig->servoCycleTime;
			    emcmotDebug->bac_incr[axis] =
				-0.5 * emcmotDebug->bac_incrincr[axis];
			    emcmotDebug->bcompdir[axis] = -1;
			}
		    }
		    /* else no motion, so leave emcmotDebug->bcomp what it
		       was */

		    /* mete out the backlash according to an acc/dec profile */
		    if (!emcmotDebug->bac_done[axis]) {
			if (emcmotDebug->bac_D[axis] > 0.0) {
			    emcmotDebug->bcompincr[axis] =
				emcmotDebug->bac_di[axis] +
				emcmotDebug->bac_d[axis];
			    if (emcmotDebug->bac_d[axis] <
				emcmotDebug->bac_halfD[axis]) {
				emcmotDebug->bac_incr[axis] +=
				    emcmotDebug->bac_incrincr[axis];
				emcmotDebug->bac_d[axis] +=
				    emcmotDebug->bac_incr[axis];
			    } else if (emcmotDebug->bac_d[axis] <
				emcmotDebug->bac_D[axis]) {
				emcmotDebug->bac_incr[axis] -=
				    emcmotDebug->bac_incrincr[axis];
				emcmotDebug->bac_d[axis] +=
				    emcmotDebug->bac_incr[axis];
			    }
			    if (emcmotDebug->bac_d[axis] >=
				emcmotDebug->bac_D[axis]
				|| emcmotDebug->bac_incr[axis] <= 0.0) {
				emcmotDebug->bac_done[axis] = 1;
			    }
			} else {
			    emcmotDebug->bcompincr[axis] =
				emcmotDebug->bac_di[axis] +
				emcmotDebug->bac_d[axis];
			    if (emcmotDebug->bac_d[axis] >
				emcmotDebug->bac_halfD[axis]) {
				emcmotDebug->bac_incr[axis] +=
				    emcmotDebug->bac_incrincr[axis];
				emcmotDebug->bac_d[axis] -=
				    emcmotDebug->bac_incr[axis];
			    } else if (emcmotDebug->bac_d[axis] >
				emcmotDebug->bac_D[axis]) {
				emcmotDebug->bac_incr[axis] -=
				    emcmotDebug->bac_incrincr[axis];
				emcmotDebug->bac_d[axis] -=
				    emcmotDebug->bac_incr[axis];
			    }
			    if (emcmotDebug->bac_d[axis] <=
				emcmotDebug->bac_D[axis]
				|| emcmotDebug->bac_incr[axis] <= 0.0) {
				emcmotDebug->bac_done[axis] = 1;
			    }
			}
		    }

		    /* end of: if not emcmotDebug->bac_done[axis] */
		    /* ADJUST OUTPUT: this computes outJointPos[] from
		       emcmotDebug->jointPos[], by adding backlash
		       compensation and rounding result to nearest input unit 
		     */
		    emcmotDebug->outJointPos[axis] =
			emcmotDebug->jointPos[axis] +
			emcmotDebug->bcompincr[axis];

#ifndef NO_ROUNDING
		    numres =
			emcmotDebug->outJointPos[axis] *
			emcmotStatus->inputScale[axis];
		    if (numres < 0.0) {
			emcmotDebug->outJointPos[axis] =
			    emcmotDebug->inverseInputScale[axis] *
			    ((int) (numres - 0.5));
		    } else {
			emcmotDebug->outJointPos[axis] =
			    emcmotDebug->inverseInputScale[axis] *
			    ((int) (numres + 0.5));
		    }
#endif

		    /* Here's where binfunc_trajupdate() was called. */

		    /* COMPENSATE: compensation means compute output
		       'emcmotStatus->output[]' based on desired position
		       'outJointPos[]' and input 'emcmotStatus->input[]'.

		       Currently the source calls for PID compensation.
		       FIXME-- add wrapper for compensator, with ptr to
		       emcmotStatus struct, with semantics that ->output[]
		       needs to be filled. */

		    /* here is PID compensation */
		    /* note that we have to compare adjusted output
		       'outJointPos' with the input, but the input has
		       already had backlash comp taken out, while the output
		       has just had it added in. So, we need to add it to the 
		       input for this calculation */
		    emcmotStatus->output[axis] =
			pidRunCycle(&emcmotConfig->pid[axis],
			emcmotStatus->input[axis] +
			emcmotDebug->bcompincr[axis],
			emcmotDebug->outJointPos[axis]);

		    /* COMPUTE FOLLOWING ERROR: */

		    /* compute signed following error and magnitude */
		    thisFerror[axis] =
			emcmotDebug->jointPos[axis] -
			emcmotStatus->input[axis];
		    magFerror = fabs(thisFerror[axis]);
		    emcmotDebug->ferrorCurrent[axis] = thisFerror[axis];

		    /* record the max ferror for this axis */
		    if (emcmotDebug->ferrorHighMark[axis] < magFerror) {
			emcmotDebug->ferrorHighMark[axis] = magFerror;
		    }

		    /* compute the scaled ferror for a move of this speed */
		    limitFerror = emcmotConfig->maxFerror[axis] /
			emcmotConfig->limitVel * emcmotDebug->jointVel[axis];
		    if (limitFerror < emcmotConfig->minFerror[axis]) {
			limitFerror = emcmotConfig->minFerror[axis];
		    }

		    /* abort if this ferror is greater than the scaled ferror 
		     */
		    if (magFerror > limitFerror) {
			/* abort! abort! following error exceeded */
			SET_AXIS_ERROR_FLAG(axis, 1);
			SET_AXIS_FERROR_FLAG(axis, 1);
			if (emcmotDebug->enabling) {
			    /* report the following error just this once */
			    reportError("axis %d following error", axis);
			}
			emcmotDebug->enabling = 0;
		    } else {
			SET_AXIS_FERROR_FLAG(axis, 0);
		    }
		} /* end of: if (GET_AXIS_ACTIVE_FLAG(axis)) */
		else {
		    /* axis is not active-- leave the pid output where it
		       is-- if axis is not active one can still write to the
		       dac */
		}

		/* CLAMP OUTPUT: */

		/* 
		   clamp output means take 'emcmotStatus->output[]' and limit 
		   to range 'emcmotConfig->minOutput[] ..
		   emcmotConfig->maxOutput[]' */
		if (emcmotStatus->output[axis] <
		    emcmotConfig->minOutput[axis]) {
		    emcmotStatus->output[axis] =
			emcmotConfig->minOutput[axis];
		} else if (emcmotStatus->output[axis] >
		    emcmotConfig->maxOutput[axis]) {
		    emcmotStatus->output[axis] =
			emcmotConfig->maxOutput[axis];
		}

		/* CHECK FOR LATCH CONDITION: */
		/* 
		   check for latch condition means if we're waiting for a
		   latched index pulse, and we see the pulse switch, we read
		   the raw input and abort. The offset is set above in the
		   homing section by noting that if we're homing, and
		   emcmotDebug->homingPhase[] is 3, we latched.

		   This presumes an encoder index pulse. FIXME-- remove
		   explicit calls to encoder index pulse, to allow for
		   open-loop control latching via switches only. Open-loop
		   control can be achieved, at least for STG boards, by
		   defining NO_INDEX_PULSE in extstgmot.c */
		if (emcmotDebug->homingPhase[axis] == 3) {
		    /* read encoder index pulse */
		    extEncoderReadLatch(axis, &emcmotDebug->latchFlag[axis]);
		    if (emcmotDebug->latchFlag[axis]) {
			/* code below is excuted once the index pulse is
			   found */
			/* call for an abort-- when it's finished, code above 
			   sets inputOffset[] to emcmotDebug->saveLatch[] */
			if (tpAbort(&emcmotDebug->freeAxis[axis]) == 0) {
			    /* Only advance the homing sequence if the motion 
			       is really aborted */
			    emcmotDebug->homingPhase[axis] = 4;
			    /* get latched position in RAW UNITS */
			    emcmotDebug->saveLatch[axis] =
				emcmotDebug->rawInput[axis];
			}
		    }		/* end of: if (emcmotDebug->latchFlag[axis]) */
		}

		/* end of: if (emcmotDebug->homingPhase[axis] == 3 */
		/* CHECK FOR HOMING PHASE 2, COMMAND THE MOVE TO THE INDEX
		   PULSE */
		if (emcmotDebug->homingPhase[axis] == 2) {
		    if (GET_AXIS_HOMING_POLARITY(axis)) {
			emcmotDebug->freePose.tran.x = -2.0 * AXRANGE(axis);
		    } else {
			emcmotDebug->freePose.tran.x = +2.0 * AXRANGE(axis);
		    }
		    if ((retval =
			    tpAddLine(&emcmotDebug->freeAxis[axis],
				emcmotDebug->freePose)) == 0) {
			/* Only advance homing sequence if the motion is
			   actually put in the traj. planner */
			extEncoderResetIndex(axis);
			emcmotDebug->homingPhase[axis] = 3;
		    }
		}

		/* END OF: PHASE 2 */
		/* CHECK FOR HOME SWITCH CONDITION AND then tpAbort: */
		/* check if any of the home switch, phl, nhl are tripped */
		if (emcmotDebug->homingPhase[axis] == 1) {
		    if (GET_AXIS_HOME_SWITCH_FLAG(axis) ||
			GET_AXIS_PHL_FLAG(axis) || GET_AXIS_NHL_FLAG(axis)) {
			if (tpAbort(&emcmotDebug->freeAxis[axis]) == 0) {
			    /* Advance homing sequence if motion is aborted */
			    emcmotDebug->homingPhase[axis] = 2;
			}
		    }
		}		/* end of:
				   if(emcmotDebug->homingPhase[axis]==1){ */
	    }			/* end of: for (axis = 0; ...) */
	} /* end of: if (GET_MOTION_ENABLE_FLAG()) */
	else {
	    /* 
	       we're not enabled, so no motion planning or interpolation has
	       been done. emcmotDebug->jointPos[] is set to
	       emcmotStatus->input[], and likewise with
	       emcmotDebug->coarseJointPos[], which is normally updated at
	       the traj rate but it's convenient to do them here at the same
	       time at the servo rate. emcmotStatus->pos, ->actualPos need to 
	       be run through forward kinematics.  Note that we are running
	       at the servo rate, so we need to slow down by the
	       interpolation factor to avoid soaking the CPU. If we were
	       enabled, ->pos was set by calcs (coord mode) or forward kins
	       (free mode), and ->actualPos was set by forward kins on
	       ->input[], all at the trajectory rate. */
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		emcmotDebug->coarseJointPos[axis] = emcmotStatus->input[axis];
		emcmotDebug->oldJointPos[axis] = emcmotDebug->jointPos[axis];
		emcmotDebug->jointPos[axis] =
		    emcmotDebug->coarseJointPos[axis];
		emcmotDebug->jointVel[axis] =
		    (emcmotDebug->jointPos[axis] -
		    emcmotDebug->oldJointPos[axis]) /
		    emcmotConfig->servoCycleTime;
	    }
	    /* synthesize the trajectory interpolation, via a counter that
	       decrements from the interpolation rate. This causes the
	       statements to execute at the trajectory rate instead of the
	       servo rate at which this enclosing code is called. */
	    if (--interpolationCounter <= 0) {
		if (kinType != KINEMATICS_INVERSE_ONLY) {
		    /* call the forward kinematics, at the effective
		       trajectory rate */
		    EmcPose temp = emcmotStatus->pos;
		    if (0 == kinematicsForward(emcmotStatus->input, &temp,
			    &fflags, &iflags)) {
			emcmotStatus->pos = temp;
			emcmotStatus->actualPos = temp;
		    }
		}
		/* else can't generate Cartesian position, so leave it alone */

		/* reload the interpolation counter that simulates the
		   interpolation done when enabled */
		interpolationCounter = emcmotConfig->interpolationRate;
	    }
	}

	extDioRead(emcmotConfig->probeIndex, &emcmotStatus->probeval);
	if (emcmotStatus->probing && emcmotStatus->probeTripped) {
	    tpClear(&emcmotDebug->queue);
	    emcmotStatus->probing = 0;
	} else if (emcmotStatus->probing && GET_MOTION_INPOS_FLAG() &&
	    tpQueueDepth(&emcmotDebug->queue) == 0) {
	    emcmotStatus->probing = 0;
	} else if (emcmotStatus->probing) {
	    if (emcmotStatus->probeval == emcmotConfig->probePolarity) {
		emcmotStatus->probeTripped = 1;
		emcmotStatus->probedPos = emcmotStatus->actualPos;
		if (GET_MOTION_COORD_FLAG()) {
		    tpAbort(&emcmotDebug->queue);
		    SET_MOTION_ERROR_FLAG(0);
		} else {
		    /* check axis range */
		    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
			tpAbort(&emcmotDebug->freeAxis[axis]);
			SET_AXIS_HOMING_FLAG(axis, 0);
			SET_AXIS_ERROR_FLAG(axis, 0);
		    }
		}
	    }
	}

	/* SCALE OUTPUTS: */

	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    emcmotDebug->rawOutput[axis] =
		(emcmotStatus->output[axis] -
		emcmotStatus->outputOffset[axis]) *
		emcmotDebug->inverseOutputScale[axis];
	}

	/* WRITE OUTPUTS: */

	/* write DACs-- note that this is done even when not enabled,
	   although in this case the pidOutputs are all zero unless manually
	   overridden. They will not be set by any calculations if we're not
	   enabled. */
	extDacWriteAll(EMCMOT_MAX_AXIS, emcmotDebug->rawOutput);

	/* UPDATE THE REST OF THE DYNAMIC STATUS: */

	/* copy computed axis positions to status. Note that if motion is
	   disabled, this is the same as ->input[]. */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    emcmotStatus->axisPos[axis] = emcmotDebug->jointPos[axis];
	}

	/* health heartbeat */
	emcmotStatus->heartbeat++;

	/* motion emcmotDebug->queue status */
	emcmotStatus->depth = tpQueueDepth(&emcmotDebug->queue);
	emcmotStatus->activeDepth = tpActiveDepth(&emcmotDebug->queue);
	emcmotStatus->id = tpGetExecId(&emcmotDebug->queue);
	emcmotStatus->queueFull = tcqFull(&emcmotDebug->queue.queue);
	SET_MOTION_INPOS_FLAG(0);
	if (tpIsDone(&emcmotDebug->queue)) {
	    SET_MOTION_INPOS_FLAG(1);
	}

	/* axis status */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    SET_AXIS_INPOS_FLAG(axis, 0);
	    if (tpIsDone(&emcmotDebug->freeAxis[axis])) {
		SET_AXIS_INPOS_FLAG(axis, 1);
	    } else {
		/* this axis, at least, is moving, so set
		   emcmotDebug->overriding flag */
		if (emcmotStatus->overrideLimits) {
		    emcmotDebug->overriding = 1;
		}
	    }
	}

	/* reset overrideLimits flag if we have started a move and now are in 
	   position */
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    if (!GET_AXIS_INPOS_FLAG(axis)) {
		break;
	    }
	}
	if (axis == EMCMOT_MAX_AXIS) {
	    /* ran through all axes, and all are in position */
	    if (emcmotDebug->overriding) {
		emcmotDebug->overriding = 0;
		emcmotStatus->overrideLimits = 0;
	    }
	}

	/* check to see if we should pause in order to implement single
	   emcmotDebug->stepping */
	if (emcmotDebug->stepping
	    && emcmotDebug->idForStep != emcmotStatus->id) {
	    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
		tpPause(&emcmotDebug->freeAxis[axis]);
	    }
	    tpPause(&emcmotDebug->queue);
	    emcmotDebug->stepping = 0;
	    emcmotStatus->paused = 1;
	}

	/* calculate elapsed time and min/max/avg */
	end = etime();
	delta = end - start;

	emcmotDebug->last_time = emcmotDebug->cur_time;
	emcmotDebug->cur_time = end;

	if (emcmotDebug->last_time != 0.0) {
	    if (!IGNORE_REALTIME_ERRORS && GET_MOTION_ENABLE_FLAG()) {
		if (emcmotDebug->cur_time - emcmotDebug->last_time >
		    10 * emcmotConfig->servoCycleTime) {
		    reportError("controller missed realtime deadline.");
		    IGNORE_REALTIME_ERRORS = 1;
		}
	    }
	    if (DEBUG_EMCMOT) {
		mmxavgAdd(&emcmotDebug->yMmxavg,
		    (emcmotDebug->cur_time - emcmotDebug->last_time));
		emcmotDebug->yMin = mmxavgMin(&emcmotDebug->yMmxavg);
		emcmotDebug->yMax = mmxavgMax(&emcmotDebug->yMmxavg);
		emcmotDebug->yAvg = mmxavgAvg(&emcmotDebug->yMmxavg);
	    }
	}

	emcmotStatus->computeTime = delta;
	if (DEBUG_EMCMOT) {
	    if (2 == whichCycle) {
		/* traj calcs done this cycle-- use tMin,Max,Avg */
		mmxavgAdd(&emcmotDebug->tMmxavg, delta);
		emcmotDebug->tMin = mmxavgMin(&emcmotDebug->tMmxavg);
		emcmotDebug->tMax = mmxavgMax(&emcmotDebug->tMmxavg);
		emcmotDebug->tAvg = mmxavgAvg(&emcmotDebug->tMmxavg);
	    } else if (1 == whichCycle) {
		/* servo calcs only this cycle-- use sMin,Max,Avg */
		mmxavgAdd(&emcmotDebug->sMmxavg, delta);
		emcmotDebug->sMin = mmxavgMin(&emcmotDebug->sMmxavg);
		emcmotDebug->sMax = mmxavgMax(&emcmotDebug->sMmxavg);
		emcmotDebug->sAvg = mmxavgAvg(&emcmotDebug->sMmxavg);
	    } else {
		/* calcs disabled this cycle-- use nMin,Max,Avg */
		mmxavgAdd(&emcmotDebug->nMmxavg, delta);
		emcmotDebug->nMin = mmxavgMin(&emcmotDebug->nMmxavg);
		emcmotDebug->nMax = mmxavgMax(&emcmotDebug->nMmxavg);
		emcmotDebug->nAvg = mmxavgAvg(&emcmotDebug->nMmxavg);
	    }
	}

	/* log per-servo-cycle data if logging active */
	logIt = 0;
	if (emcmotStatus->logStarted && emcmotStatus->logSkip >= 0) {

	    /* record type here, since all will set this */
	    ls.type = emcmotStatus->logType;

	    /* now log type-specific data */
	    switch (ls.type) {

	    case EMCMOT_LOG_TYPE_AXIS_POS:
		if (logSkip-- <= 0) {
		    ls.item.axisPos.time = end - logStartTime;
		    ls.item.axisPos.input = emcmotStatus->input[loggingAxis];
		    ls.item.axisPos.output =
			emcmotDebug->jointPos[loggingAxis];
		    logSkip = emcmotStatus->logSkip;
		    logIt = 1;
		}
		break;

	    case EMCMOT_LOG_TYPE_ALL_INPOS:
		if (logSkip-- <= 0) {
		    ls.item.allInpos.time = end - logStartTime;
		    for (axis = 0;
			axis < EMCMOT_LOG_NUM_AXES &&
			axis < EMCMOT_MAX_AXIS; axis++) {
			ls.item.allInpos.input[axis] =
			    emcmotStatus->input[axis];
		    }
		    logSkip = emcmotStatus->logSkip;
		    logIt = 1;
		}
		break;

	    case EMCMOT_LOG_TYPE_ALL_OUTPOS:
		if (logSkip-- <= 0) {
		    ls.item.allOutpos.time = end - logStartTime;
		    for (axis = 0;
			axis < EMCMOT_LOG_NUM_AXES &&
			axis < EMCMOT_MAX_AXIS; axis++) {
			ls.item.allOutpos.output[axis] =
			    emcmotDebug->jointPos[axis];
		    }
		    logSkip = emcmotStatus->logSkip;
		    logIt = 1;
		}
		break;

	    case EMCMOT_LOG_TYPE_AXIS_VEL:
		if (logSkip-- <= 0) {
		    ls.item.axisVel.time = end - logStartTime;
		    ls.item.axisVel.cmdVel =
			emcmotDebug->jointPos[loggingAxis] -
			emcmotDebug->oldJointPos[loggingAxis];
		    ls.item.axisVel.actVel =
			emcmotStatus->input[loggingAxis] -
			emcmotDebug->oldInput[loggingAxis];
		    logSkip = emcmotStatus->logSkip;
		    logIt = 1;
		}
		break;

	    case EMCMOT_LOG_TYPE_ALL_FERROR:
		if (logSkip-- <= 0) {
		    ls.item.allFerror.time = end - logStartTime;
		    for (axis = 0;
			axis < EMCMOT_LOG_NUM_AXES &&
			axis < EMCMOT_MAX_AXIS; axis++) {
			ls.item.allFerror.ferror[axis] =
			    emcmotDebug->ferrorCurrent[axis];
		    }
		    logSkip = emcmotStatus->logSkip;
		    logIt = 1;
		}
		break;

	    case EMCMOT_LOG_TYPE_POS_VOLTAGE:
		/* don't do a circular log-- suppress if full */
		if (emcmotLog->howmany >= emcmotStatus->logSize) {
		    emcmotStatus->output[loggingAxis] = 0.0;	/* drop the
								   DAC to
								   zero */
		    emcmotStatus->logStarted = 0;	/* stop logging */
		    logIt = 0;	/* should still be zero, reset anyway */
		    break;
		}
		if (logSkip-- <= 0) {
		    ls.item.posVoltage.time = end - logStartTime;
		    for (axis = 0;
			axis < EMCMOT_LOG_NUM_AXES &&
			axis < EMCMOT_MAX_AXIS; axis++) {
			ls.item.posVoltage.pos =
			    emcmotStatus->input[loggingAxis];
			ls.item.posVoltage.voltage =
			    emcmotDebug->rawOutput[loggingAxis];
		    }
		    logSkip = emcmotStatus->logSkip;
		    logIt = 1;
		}
		break;

	    default:
		break;
	    }			/* end of: switch on log type */

	    /* now log it */
	    if (logIt) {
		emcmotLogAdd(emcmotLog, ls);
		emcmotStatus->logPoints = emcmotLog->howmany;
		logIt = 0;
	    }
	}
	/* end of: if logging */
	emcmotDebug->running_time = etime() - emcmotDebug->start_time;

	/* set tail to head, which has already been incremented */
	emcmotStatus->tail = emcmotStatus->head;
	emcmotDebug->tail = emcmotDebug->head;
	emcmotConfig->tail = emcmotConfig->head;

	/* wait for next cycle */
	rtapi_wait();
	while (0 == emcmotStruct) {
	    rtapi_wait();
	}
#ifdef RTAPI
    }				/* end of: forever loop for RT task */
#endif
}				/* end of: emcmotController() function */

#ifdef RTAPI

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,2,0)
/* register symbols to be modified by insmod
   see "Linux Device Drivers", Alessandro Rubini, p. 385
   (p.42-44 in 2nd edition) */
MODULE_PARM(PERIOD, "i");
MODULE_PARM(IGNORE_REALTIME_ERRORS, "i");
MODULE_PARM(DEBUG_EMCMOT, "i");
MODULE_PARM(SHMEM_BASE_ADDRESS, "l");
MODULE_PARM(SHMEM_KEY, "i");
MODULE_PARM(MOTION_IO_ADDRESS, "i");

MODULE_PARM(IO_BASE_ADDRESS, "l");
MODULE_PARM(FIND_IO_BASE_ADDRESS, "i");

MODULE_PARM(EMCMOT_TASK_PRIORITY, "i");
MODULE_PARM(EMCMOT_TASK_STACK_SIZE, "i");
MODULE_PARM(EMCMOT_NO_FORWARD_KINEMATICS, "i");

#ifdef MODULE_LICENSE
// The additional rights are you can do anything you want. -- meaning the code is public domain.
MODULE_LICENSE("GPL and additional rights");
#endif

#endif /* if LINUX_VERSION_CODE > KERNEL_VERSION(2,2,0) */

#endif /* ifdef RTAPI */

/* rtapi stuff */
static int module;
static int emcmot_task;		/* the task ID */
static int emcmot_prio;
#define EMCMOT_TASK_STACKSIZE 8192

typedef void (*RTAPI_FUNC) (void *);

int init_module(void)
{
    int axis;
    int t;
    PID_STRUCT pid;
    int retval;
    long period;

    module = rtapi_init("EMCMOT");
    if (module < 0) {
	rtapi_print("emcmot init: rtapi_init returned %d\n", module);
	return -1;
    }

    emcmotStruct = 0;
    emcmotDebug = 0;
    emcmotStatus = 0;
    emcmotCommand = 0;
    emcmotIo = 0;
    emcmotConfig = 0;

    /* record the kinematics type of the machine */
    kinType = kinematicsType();
    if (EMCMOT_NO_FORWARD_KINEMATICS) {
	kinType = KINEMATICS_INVERSE_ONLY;
    }

    /* allocate and initialize the shared memory structure */
    shmem_mem = rtapi_shmem_new(key, module, sizeof(EMCMOT_STRUCT));
    if (shmem_mem < 0) {
	rtapi_print("emcmot init: rtapi_shmem_new returned %d\n", shmem_mem);
	rtapi_exit(module);
	return -1;
    }
    retval = rtapi_shmem_getptr(shmem_mem, (void **) &emcmotStruct);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot init: rtapi_shmem_getptr returned %d\n", retval);
	rtapi_exit(module);
	return -1;
    }

    /* is timer started? if so, what period? */
    PERIOD_NSEC = PERIOD * 1000;	/* convert from msec to nsec */
    period = rtapi_clock_set_period(0);
    if (period == 0) {
	/* not running, start it */
	rtapi_print("emcmot init: starting timer with period %ld\n",
	    PERIOD_NSEC);
	period = rtapi_clock_set_period(PERIOD_NSEC);
	if (period < 0) {
	    rtapi_print
		("emcmot init: rtapi_clock_set_period failed with %ld\n",
		period);
	    rtapi_exit(module);
	    return -1;
	}
    }
    /* make sure period <= desired period (allow 1% roundoff error) */
    if (period > (PERIOD_NSEC + (PERIOD_NSEC / 100))) {
	/* timer period too long */
	rtapi_print("emcmot init: clock period too long: %ld\n", period);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("emcmot init: desired clock %ld, actual %ld\n", PERIOD_NSEC,
	period);

    /* we'll reference emcmotStruct directly */
    emcmotCommand = (EMCMOT_COMMAND *) & emcmotStruct->command;
    emcmotStatus = (EMCMOT_STATUS *) & emcmotStruct->status;
    emcmotConfig = (EMCMOT_CONFIG *) & emcmotStruct->config;
    if (DEBUG_EMCMOT) {
	emcmotDebug = (EMCMOT_DEBUG *) & emcmotStruct->debug;
    } else {
	emcmotDebug = &localEmcmotDebug;
    }

    emcmotError = (EMCMOT_ERROR *) & emcmotStruct->error;
    emcmotIo = (EMCMOT_IO *) & emcmotStruct->io;	/* set address of
							   struct JE
							   8/21/2001 */
    emcmotLog = (EMCMOT_LOG *) & emcmotStruct->log;
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis] = (EMCMOT_COMP *) & emcmotStruct->comp[axis];

	emcmotDebug->bcomp[axis] = 0;	/* backlash comp value */
	emcmotDebug->bcompdir[axis] = 0;	/* 0=none, 1=pos, -1=neg */
	emcmotDebug->bcompincr[axis] = 0;	/* incremental backlash comp */
	emcmotDebug->bac_done[axis] = 0;
	emcmotDebug->bac_d[axis] = 0;
	emcmotDebug->bac_di[axis] = 0;
	emcmotDebug->bac_D[axis] = 0;
	emcmotDebug->bac_halfD[axis] = 0;
	emcmotDebug->bac_incrincr[axis] = 0;
	emcmotDebug->bac_incr[axis] = 0;
    }

    /* zero shared memory */
    for (t = 0; t < sizeof(EMCMOT_STRUCT); t++) {
	((char *) emcmotStruct)[t] = 0;
    }

    /* init locals */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotDebug->maxLimitSwitchCount[axis] = 0;
	emcmotDebug->minLimitSwitchCount[axis] = 0;
	emcmotDebug->ampFaultCount[axis] = 0;
    }

    /* init compensation struct */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis]->total = 0;
	emcmotComp[axis]->alter = 0.0;
	/* leave out the avgint, nominal, forward, and reverse, since these
	   can't be zero and the total flag prevents their use anyway */
    }

    /* init error struct */
    emcmotErrorInit(emcmotError);

    /* init command struct */
    emcmotCommand->head = 0;
    emcmotCommand->command = 0;
    emcmotCommand->commandNum = 0;
    emcmotCommand->tail = 0;

    /* init status struct */

    emcmotStatus->head = 0;
    emcmotDebug->head = 0;
    emcmotConfig->head = 0;

    emcmotStatus->motionFlag = 0;
    SET_MOTION_ERROR_FLAG(0);
    SET_MOTION_COORD_FLAG(0);
    SET_MOTION_TELEOP_FLAG(0);
    emcmotDebug->split = 0;
    emcmotStatus->commandEcho = 0;
    emcmotStatus->commandNumEcho = 0;
    emcmotStatus->commandStatus = 0;
    emcmotStatus->heartbeat = 0;
    emcmotStatus->computeTime = 0.0;
    emcmotConfig->numAxes = EMCMOT_MAX_AXIS;
    emcmotConfig->trajCycleTime = TRAJ_CYCLE_TIME;
    emcmotConfig->servoCycleTime = SERVO_CYCLE_TIME;
    emcmotStatus->pos.tran.x = 0.0;
    emcmotStatus->pos.tran.y = 0.0;
    emcmotStatus->pos.tran.z = 0.0;
    emcmotStatus->actualPos.tran.x = 0.0;
    emcmotStatus->actualPos.tran.y = 0.0;
    emcmotStatus->actualPos.tran.z = 0.0;
    emcmotStatus->vel = VELOCITY;
    emcmotConfig->limitVel = VELOCITY;
    emcmotStatus->acc = ACCELERATION;
    emcmotStatus->qVscale = 1.0;
    emcmotStatus->id = 0;
    emcmotStatus->depth = 0;
    emcmotStatus->activeDepth = 0;
    emcmotStatus->paused = 0;
    emcmotStatus->overrideLimits = 0;
    SET_MOTION_INPOS_FLAG(1);
    emcmotStatus->logOpen = 0;
    emcmotStatus->logStarted = 0;
    emcmotStatus->logSize = 0;
    emcmotStatus->logSkip = 0;
    emcmotStatus->logPoints = 0;
    SET_MOTION_ENABLE_FLAG(0);
    emcmotConfig->kinematics_type = kinType;

    emcmotDebug->oldPos = emcmotStatus->pos;
    emcmotDebug->oldVel.tran.x = 0.0;
    emcmotDebug->oldVel.tran.y = 0.0;
    emcmotDebug->oldVel.tran.z = 0.0;

    MARK_EMCMOT_CONFIG_CHANGE();

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotConfig->homingVel[axis] = VELOCITY;
	emcmotConfig->homeOffset[axis] = 0.0;
	emcmotStatus->axisFlag[axis] = 0;
	emcmotConfig->maxLimit[axis] = MAX_LIMIT;
	emcmotConfig->minLimit[axis] = MIN_LIMIT;
	emcmotConfig->maxOutput[axis] = MAX_OUTPUT;
	emcmotConfig->minOutput[axis] = MIN_OUTPUT;
	emcmotConfig->minFerror[axis] = 0.0;	/* gives a true linear ferror 
						 */
	emcmotConfig->maxFerror[axis] = MAX_FERROR;
	emcmotDebug->ferrorCurrent[axis] = 0.0;
	emcmotDebug->ferrorHighMark[axis] = 0.0;
	emcmotStatus->outputScale[axis] = OUTPUT_SCALE;
	emcmotStatus->outputOffset[axis] = OUTPUT_OFFSET;
	emcmotStatus->inputScale[axis] = INPUT_SCALE;
	emcmotDebug->inverseInputScale[axis] = 1.0 / INPUT_SCALE;
	emcmotStatus->inputOffset[axis] = INPUT_OFFSET;
	emcmotDebug->inverseOutputScale[axis] = 1.0 / OUTPUT_SCALE;
	emcmotStatus->axVscale[axis] = 1.0;
	emcmotConfig->axisLimitVel[axis] = 1.0;
	emcmotDebug->bigVel[axis] = 1.0;
	SET_AXIS_ENABLE_FLAG(axis, 0);
	SET_AXIS_ACTIVE_FLAG(axis, 0);	/* default is not to use it; need an
					   explicit activate */
	SET_AXIS_NSL_FLAG(axis, 0);
	SET_AXIS_PSL_FLAG(axis, 0);
	SET_AXIS_NHL_FLAG(axis, 0);
	SET_AXIS_PHL_FLAG(axis, 0);
	SET_AXIS_INPOS_FLAG(axis, 1);
	SET_AXIS_HOMING_FLAG(axis, 0);
	SET_AXIS_HOMED_FLAG(axis, 0);
	SET_AXIS_FERROR_FLAG(axis, 0);
	SET_AXIS_FAULT_FLAG(axis, 0);
	SET_AXIS_ERROR_FLAG(axis, 0);
	emcmotConfig->axisPolarity[axis] = (EMCMOT_AXIS_FLAG) 0xFFFFFFFF;
	/* will read encoders directly, so don't set them here */
    }

    /* FIXME-- add emcmotError */

    /* init min-max-avg stats */
    mmxavgInit(&emcmotDebug->tMmxavg, emcmotDebug->tMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->sMmxavg, emcmotDebug->sMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->nMmxavg, emcmotDebug->nMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->yMmxavg, emcmotDebug->yMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fMmxavg, emcmotDebug->fMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fyMmxavg, emcmotDebug->fyMmxavgSpace,
	MMXAVG_SIZE);
    emcmotDebug->tMin = 0.0;
    emcmotDebug->tMax = 0.0;
    emcmotDebug->tAvg = 0.0;
    emcmotDebug->sMin = 0.0;
    emcmotDebug->sMax = 0.0;
    emcmotDebug->sAvg = 0.0;
    emcmotDebug->nMin = 0.0;
    emcmotDebug->nMax = 0.0;
    emcmotDebug->nAvg = 0.0;
    emcmotDebug->yMin = 0.0;
    emcmotDebug->yMax = 0.0;
    emcmotDebug->yAvg = 0.0;
    emcmotDebug->fyMin = 0.0;
    emcmotDebug->fyMax = 0.0;
    emcmotDebug->fyAvg = 0.0;
    emcmotDebug->fMin = 0.0;
    emcmotDebug->fMax = 0.0;
    emcmotDebug->fAvg = 0.0;

    emcmotDebug->cur_time = emcmotDebug->last_time = 0.0;
    emcmotDebug->start_time = etime();
    emcmotDebug->running_time = 0.0;

    /* init motion emcmotDebug->queue */
    if (-1 == tpCreate(&emcmotDebug->queue, DEFAULT_TC_QUEUE_SIZE,
	    emcmotDebug->queueTcSpace)) {
	rtapi_print("can't create motion emcmotDebug->queue\n");
	return -1;
    }
    tpInit(&emcmotDebug->queue);
    tpSetCycleTime(&emcmotDebug->queue, emcmotConfig->trajCycleTime);
    tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
    tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel);
    tpSetAmax(&emcmotDebug->queue, emcmotStatus->acc);

    /* init the axis components */
    pid.p = P_GAIN;
    pid.i = I_GAIN;
    pid.d = D_GAIN;
    pid.ff0 = FF0_GAIN;
    pid.ff1 = FF1_GAIN;
    pid.ff2 = FF2_GAIN;
    pid.backlash = BACKLASH;
    pid.bias = BIAS;
    pid.maxError = MAX_ERROR;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (-1 == tpCreate(&emcmotDebug->freeAxis[axis], FREE_AXIS_QUEUE_SIZE,
		emcmotDebug->freeAxisTcSpace[axis])) {
	    rtapi_print("can't create axis emcmotDebug->queue %d\n", axis);
	    return -1;
	}
	tpInit(&emcmotDebug->freeAxis[axis]);
	tpSetCycleTime(&emcmotDebug->freeAxis[axis],
	    emcmotConfig->trajCycleTime);
	/* emcmotDebug->freePose is inited to 0's in decl */
	tpSetPos(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	tpSetVmax(&emcmotDebug->freeAxis[axis], emcmotStatus->vel);
	tpSetAmax(&emcmotDebug->freeAxis[axis], emcmotStatus->acc);
	pidInit(&emcmotConfig->pid[axis]);
	pidSetGains(&emcmotConfig->pid[axis], pid);
	cubicInit(&emcmotDebug->cubic[axis]);
    }

    /* init the time and rate using functions to affect traj, the pids, and
       the cubics properly, since they're coupled */
    setTrajCycleTime(TRAJ_CYCLE_TIME);
    setServoCycleTime(SERVO_CYCLE_TIME);

    extEncoderSetIndexModel(EXT_ENCODER_INDEX_MODEL_MANUAL);
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotDebug->rawInput[axis] = 0.0;
	emcmotDebug->rawOutput[axis] = 0.0;
	emcmotDebug->coarseJointPos[axis] = 0.0;
	emcmotDebug->jointPos[axis] = 0.0;
	emcmotDebug->jointVel[axis] = 0.0;
	emcmotStatus->axisPos[axis] = 0.0;
	emcmotDebug->oldJointPos[axis] = 0.0;
	emcmotDebug->outJointPos[axis] = 0.0;

	emcmotDebug->homingPhase[axis] = 0;
	emcmotDebug->latchFlag[axis] = 0;
	emcmotDebug->saveLatch[axis] = 0.0;

	emcmotStatus->input[axis] = 0.0;
	emcmotDebug->oldInput[axis] = 0.0;
	emcmotDebug->oldInputValid[axis] = 0;
	emcmotStatus->output[axis] = 0.0;

	emcmotDebug->jointHome[axis] = 0.0;

	extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
    }

    emcmotStatus->tail = 0;

    rtapi_print("emcmot: initializing emcmotTask\n");

    /* set the task priority to second lowest, since we only have one task */
    emcmot_prio = rtapi_prio_next_higher(rtapi_prio_lowest());

    /* create the timer task */
    /* the second arg is an abitrary int that is passed to the timer task on
       the first iterration */
    emcmot_task =
	rtapi_task_new(emcmotController, (void *) 0, emcmot_prio, module,
	EMCMOT_TASK_STACKSIZE, RTAPI_USES_FP);
    if (emcmot_task < 0) {
	/* See rtapi.h for the error codes returned */
	rtapi_print("emcmot init: rtapi_task_new returned %d\n", emcmot_task);
	rtapi_exit(module);
	return -1;
    }
    /* start the task running */
    retval = rtapi_task_start(emcmot_task, (int) (SERVO_CYCLE_TIME * 1.0e9));
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot init: rtapi_task_start returned %d\n", retval);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("emcmot init: started timer task\n");
    rtapi_print("emcmot: init_module finished\n");

    return 0;
}

void cleanup_module(void)
{
    int axis;
    int retval;

    rtapi_print("emcmot: cleanup started.\n");

    retval = rtapi_task_pause(emcmot_task);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot exit: rtapi_task_pause returned %d\n", retval);
    }
    /* Remove the task from the list */
    retval = rtapi_task_delete(emcmot_task);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot exit: rtapi_task_delete returned %d\n", retval);
    }

    /* WPS these were moved from above to avoid a possible mutex problem. */
    /* There is no point in clearing the trajectory queue since the planner
       should be dead by now anyway. */
    if (emcmotStruct != 0 && emcmotDebug != 0 && emcmotConfig != 0) {
	rtapi_print("emcmot: disabling amps\n");
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
	}
    }

    /* free shared memory */
    retval = rtapi_shmem_delete(shmem_mem, module);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot exit: rtapi_shmem_delete returned %d\n", retval);
    }

    rtapi_print("emcmot: cleanup finished.\n");

    /* Clean up and exit */
    rtapi_exit(module);
}
