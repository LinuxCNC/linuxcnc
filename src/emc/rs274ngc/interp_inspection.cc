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
