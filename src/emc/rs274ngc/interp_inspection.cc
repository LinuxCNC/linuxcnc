#include "interp_inspection.hh"
#include "interp_internal.hh"


double &currentX(setup_pointer s)
{
  return s->current_x;
}

double &currentY(setup_pointer s)
{
  return s->current_y;
}

double &currentZ(setup_pointer s)
{
  return s->current_z;
}

double & currentA(setup_pointer s)
{
  return s->AA_current;
}

double & currentB(setup_pointer s)
{
  return s->BB_current;
}

double & currentC(setup_pointer s)
{
  return s->CC_current;
}

double & currentWorkOffsetX(setup_pointer s)
{
  return s->origin_offset_x;
}

double & currentWorkOffsetY(setup_pointer s)
{
  return s->origin_offset_y;
}

double &currentWorkOffsetZ(setup_pointer s)
{
  return s->origin_offset_z;
}

double & currentWorkOffsetA(setup_pointer s)
{
  return s->AA_origin_offset;
}

double & currentWorkOffsetB(setup_pointer s)
{
  return s->BB_origin_offset;
}

double & currentWorkOffsetC(setup_pointer s)
{
  return s->CC_origin_offset;
}

double & currentAxisOffsetX(setup_pointer s)
{
  return s->axis_offset_x;
}

double & currentAxisOffsetY(setup_pointer s)
{
  return s->axis_offset_y;
}

double &currentAxisOffsetZ(setup_pointer s)
{
  return s->axis_offset_z;
}

double & currentAxisOffsetA(setup_pointer s)
{
  return s->AA_axis_offset;
}

double & currentAxisOffsetB(setup_pointer s)
{
  return s->BB_axis_offset;
}

double & currentAxisOffsetC(setup_pointer s)
{
  return s->CC_axis_offset;
}

