#ifndef EMCTOOL_H
#define EMCTOOL_H

#include "emcpos.h"

/* Tools are numbered 1..CANON_TOOL_MAX, with tool 0 meaning no tool. */
#define CANON_POCKETS_MAX 56	// max size of carousel handled
#define CANON_TOOL_ENTRY_LEN 256	// how long each file line can be

struct CANON_TOOL_TABLE {
    int toolno;
    EmcPose offset;
    double diameter;
    double frontangle;
    double backangle;
    int orientation;
};

#endif
