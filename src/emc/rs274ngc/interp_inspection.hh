/**
 * @file interp_inspection.hh
 *
 * Shim functions to access interp internal data generically.
 *
 * This is mostly meant for unit testing (so that backend changes don't break
 * the tests as often), but it could be expanded into a proper API.
 * 
 * @author Robert W. Ellenberg <rwe24g@gmail.com>
 *
 * Copyright (c) 2019.
 *
 * License: LGPL-V2
 */

#ifndef INTERP_INSPECTION_HH
#define INTERP_INSPECTION_HH

#include <interp_fwd.hh>

double &currentX(setup_pointer s);
double &currentY(setup_pointer s);
double &currentZ(setup_pointer s);
double &currentA(setup_pointer s);
double &currentB(setup_pointer s);
double &currentC(setup_pointer s);

double &currentWorkOffsetX(setup_pointer s);
double &currentWorkOffsetY(setup_pointer s);
double &currentWorkOffsetZ(setup_pointer s);
double &currentWorkOffsetA(setup_pointer s);
double &currentWorkOffsetB(setup_pointer s);
double &currentWorkOffsetC(setup_pointer s);

double &currentAxisOffsetX(setup_pointer s);
double &currentAxisOffsetY(setup_pointer s);
double &currentAxisOffsetZ(setup_pointer s);
double &currentAxisOffsetA(setup_pointer s);
double &currentAxisOffsetB(setup_pointer s);
double &currentAxisOffsetC(setup_pointer s);
#endif // INTERP_INSPECTION_HH
