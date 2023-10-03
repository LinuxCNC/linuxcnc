#ifndef SAICANON_HH
#define SAICANON_HH

#include <stdio.h>
#include <interp_fwd.hh>
#include <canon.hh>
#include <string>

struct StandaloneInterpInternals;
class InterpBase;

extern StandaloneInterpInternals _sai;
extern InterpBase *pinterp;
extern FILE *_outfile;
extern char _parameter_file_name[PARAMETER_FILE_NAME_LENGTH];

struct StandaloneInterpInternals
{
  StandaloneInterpInternals();
  /* Dummy world model */

  CANON_PLANE _active_plane;
  int _active_slot;
  int _feed_mode;
  double _feed_rate;
  int _flood;
  double _external_length_units;
  double _length_unit_factor;
  CANON_UNITS _length_unit_type;
  int _line_number;
  int _mist;
  CANON_MOTION_MODE _motion_mode;
  double _probe_position_a;
  double _probe_position_b;
  double _probe_position_c;
  double _probe_position_x;
  double _probe_position_y;
  double _probe_position_z;
  double _g5x_x;
  double _g5x_y;
  double _g5x_z;
  double _g5x_a;
  double _g5x_b;
  double _g5x_c;
  double _g92_x;
  double _g92_y;
  double _g92_z;
  double _g92_a;
  double _g92_b;
  double _g92_c;
  double _program_position_a;
  double _program_position_b;
  double _program_position_c;
  double _program_position_x;
  double _program_position_y;
  double _program_position_z;
  double _spindle_speed[EMCMOT_MAX_SPINDLES];
  CANON_DIRECTION _spindle_turning[EMCMOT_MAX_SPINDLES];
  int _pockets_max;
  CANON_TOOL_TABLE _tools[CANON_POCKETS_MAX];
  /* optional program stop */
  bool optional_program_stop;
  /* optional block delete */
  bool block_delete;
  double motion_tolerance;
  double naivecam_tolerance;
  /* Dummy status variables */
  double _traverse_rate;

  EmcPose _tool_offset;
  bool _toolchanger_fault;
  int  _toolchanger_reason ;
};

void reset_internals();
#endif // SAICANON_HH
