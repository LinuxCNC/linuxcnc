/**
 * @file pm_line9.c
 *
 * @author Robert W. Ellenberg <rwe24g@gmail.com>
 *
 * @copyright Copyright 2019, Robert W. Ellenberg
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License (V2) as published by the Free Software Foundation.
 */

#include "posemath.h"
#include "pm_line9.h"
#include "rtapi_bool.h"
#include "stddef.h"
#include "rtapi_math.h"

static const PmCartesian zero={0,0,0};

int pmLine9Init(
    PmLine9 * const line9,
    PmVector const * const start,
    PmVector const * const end)
{
    line9->start = *start;
    line9->end = *end;

    VecVecSub(&line9->end, &line9->start, &line9->uVec);
    line9->tmag = VecMag(&line9->uVec);
    VecUnitEq(&line9->uVec);
    return 0;
}

double pmLine9Length(PmLine9 const * const line9)
{
    return line9->tmag;
}

double pmLine9RotaryAxisLength(PmLine9 const * const line9)
{
    PmCartesian abc;
    VecToCart(&line9->end, NULL, &abc, NULL);
    PmCartesian abc_start;
    VecToCart(&line9->start, NULL, &abc_start, NULL);

    pmCartCartSubEq(&abc, &abc_start);
    double len=NAN;
    pmCartMag(&abc, &len);
    return len;
}

double pmLine9LinearAxisLength(const PmLine9 * const line9)
{
    PmVector no_abc;
    VecVecSub(&line9->end, &line9->start, &no_abc);
    VecSetABC(&zero, &no_abc);

    return VecMag(&no_abc);
}

int pmLine9Point(const PmLine9 * const line9, double progress, PmVector * const point)
{
    return
        VecScalMult(&line9->uVec, progress, point)
        ||    VecVecAddEq(point, &line9->start);
}

int pmLine9Cut(PmLine9 * const line9, double cut_pt_progress, SegmentToKeepType keep_pt)
{
    switch (keep_pt) {
    case KEEP_END_PT:
    {
        PmVector tmp;
        VecScalMult(&line9->uVec, cut_pt_progress, &tmp);
        VecVecAddEq(&line9->start, &tmp);
        line9->tmag -= cut_pt_progress;
        return 0;
    }
    case KEEP_START_PT:
        VecScalMult(&line9->uVec, cut_pt_progress, &line9->end);
        VecVecAddEq(&line9->end, &line9->start);
        line9->tmag = cut_pt_progress;
        return 0;
    }

    return -1;
}

static inline bool is_fuzz(double a) {
    return fabs(a) < (CART_FUZZ) ? true : false;
}

int pmCartLineCut(PmCartLine * const line, double cut_pt_progress, SegmentToKeepType keep_pt)
{
    switch (keep_pt) {
    case KEEP_END_PT:
    {
        PmCartesian tmp;
        pmCartScalMult(&line->uVec, cut_pt_progress, &tmp);
        pmCartCartAddEq(&line->start, &tmp);
        line->tmag -= cut_pt_progress;
        line->tmag_zero = is_fuzz(line->tmag);
        return 0;
    }
    case KEEP_START_PT:
        pmCartScalMult(&line->uVec, cut_pt_progress, &line->end);
        pmCartCartAddEq(&line->end, &line->start);
        line->tmag = cut_pt_progress;
        line->tmag_zero = is_fuzz(line->tmag);
        return 0;
    }

    return -1;
}


double pmLine9VLimit(PmLine9 const * const line9, double v_target, double v_limit_linear, double v_limit_angular)
{
    return VecVLimit(&line9->uVec, v_target, v_limit_linear, v_limit_angular);
}
