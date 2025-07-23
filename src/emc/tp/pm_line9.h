#ifndef PM_LINE9_H
#define PM_LINE9_H

#include "pm_vector.h"

typedef struct {
    PmVector start;
    PmVector end;
    PmVector uVec;
    double tmag;
} PmLine9;

typedef enum {
    KEEP_START_PT,
    KEEP_END_PT,
} SegmentToKeepType;

int pmLine9Init(PmLine9 * const line9,
    PmVector const * const start,
    PmVector const * const end);

double pmLine9Length(const PmLine9 * const line9);

double pmLine9RotaryAxisLength(PmLine9 const * const line9);

double pmLine9LinearAxisLength(PmLine9 const * const line9);

int pmLine9Point(PmLine9 const * const line9, double progress, PmVector * const point);

int pmLine9Cut(PmLine9 * const line, double cut_pt_progress, SegmentToKeepType keep_pt);

int pmCartLineCut(PmCartLine * const line, double cut_pt_progress, SegmentToKeepType keep_pt);

double pmLine9VLimit(const PmLine9 * const line9, double v_target, double v_limit_linear, double v_limit_angular);

#endif // PM_LINE9_H
