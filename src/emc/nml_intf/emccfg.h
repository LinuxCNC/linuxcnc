#ifndef EMCCFG_H
#define EMCCFG_H

/*
  emccfg.h

  Compile-time defaults for EMC application. Defaults are used to initialize
  globals in emcglb.c. Include emcglb.h to access these globals.

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

#ifdef __cplusplus
extern "C" {
#endif

/* default name of EMC ini file */
#define DEFAULT_EMC_INIFILE "emc.ini"

/* default name of EMC NML file */
#define DEFAULT_EMC_NMLFILE "emc.nml"

/* cycle time for emctask, in seconds */
#define DEFAULT_EMC_TASK_CYCLE_TIME 0.100

/* cycle time for emctio, in seconds */
#define DEFAULT_EMC_IO_CYCLE_TIME 0.100

/* default name of EMC_TOOL tool table file */
#define DEFAULT_TOOL_TABLE_FILE "tool.tbl"

/* default feed rate, in user units per second */
#define DEFAULT_TRAJ_DEFAULT_VELOCITY 1.0

/* default traverse rate, in user units per second */
#define DEFAULT_TRAJ_MAX_VELOCITY 10.0

/* default axis traverse rate, in user units per second */
#define DEFAULT_AXIS_MAX_VELOCITY 1.0

/* seconds after speed off to apply brake */
#define DEFAULT_SPINDLE_OFF_WAIT 2.0

/* seconds after brake off for spindle on */
#define DEFAULT_SPINDLE_ON_WAIT 2.0

/* bit locations for digital inputs */
#define DEFAULT_ESTOP_SENSE_INDEX          0
#define DEFAULT_LUBE_SENSE_INDEX           1

/* sense of digital inputs */
#define DEFAULT_ESTOP_SENSE_POLARITY       1
#define DEFAULT_LUBE_SENSE_POLARITY        0

/* point locations for analog outputs */
#define DEFAULT_SPINDLE_ON_INDEX           0
#define DEFAULT_MIN_VOLTS_PER_RPM         -0.01
#define DEFAULT_MAX_VOLTS_PER_RPM          0.01

/* bit locations for digital outputs */

#define DEFAULT_SPINDLE_FORWARD_INDEX      0
#define DEFAULT_SPINDLE_REVERSE_INDEX      1
#define DEFAULT_SPINDLE_BRAKE_INDEX        2
#define DEFAULT_SPINDLE_DECREASE_INDEX     3
#define DEFAULT_SPINDLE_INCREASE_INDEX     4
#define DEFAULT_MIST_COOLANT_INDEX         5
#define DEFAULT_FLOOD_COOLANT_INDEX        6
#define DEFAULT_ESTOP_WRITE_INDEX          7
#define DEFAULT_SPINDLE_ENABLE_INDEX       8
#define DEFAULT_LUBE_WRITE_INDEX           9

/* sense of digital outputs */
#define DEFAULT_SPINDLE_FORWARD_POLARITY   1
#define DEFAULT_SPINDLE_REVERSE_POLARITY   1
#define DEFAULT_MIST_COOLANT_POLARITY      1
#define DEFAULT_FLOOD_COOLANT_POLARITY     1
#define DEFAULT_SPINDLE_DECREASE_POLARITY  1
#define DEFAULT_SPINDLE_INCREASE_POLARITY  1
#define DEFAULT_ESTOP_WRITE_POLARITY       1
#define DEFAULT_SPINDLE_BRAKE_POLARITY     1
#define DEFAULT_SPINDLE_ENABLE_POLARITY    1
#define DEFAULT_LUBE_WRITE_POLARITY        1

#ifdef __cplusplus
}				/* matches extern "C" at top */
#endif
#endif
