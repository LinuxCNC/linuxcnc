/********************************************************************
* Description: segmentqueue.c
*   Trajectory planner based on linking segments together
*
*   Derived from a work by Rogier Blom
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2010 All rights reserved.
********************************************************************/

#if defined(rtlinux) || defined(rtai)
// realtime headers..
#ifndef MODULE
#define MODULE
#endif
#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/version.h>
# include <linux/kernel.h>

# else
# include <stdio.h>
#endif

#include "segmentqueue.h"


#include <float.h>              /* for DBL_MAX */

#ifndef max
#define max(A,B)  ( (A) > (B) ? (A) : (B))
#endif

#ifndef min
#define min(A,B)  ( (A) < (B) ? (A) : (B))
#endif

/* sq->full will be set 1 when the number of segments exceeds the size
   of the queue minus the safety margin. This safety margin is created
   because it might happen that the task will send another motion (or more)
   before it gets the signal that the queue is full. Without a safety margin,
   this would result in a queue overflow
*/
#define SQ_SAFETY_MARGIN 10

/* if linking is possible, it will generally do it. With certain program
   this could result in a completely linked segment queue. When the chain is
   done, it will be deleted from the queue, that will be completely empty then.
   This variable can be used to set an upper limit for the number of linked
   segments */
#define SQ_MAX_NUM_LINKED_SEGS 500

#define SQ_LOW_LINKING_PRIORITY 1
#define SQ_HIGH_LINKING_PRIORITY 2

#define SQ_LINKING_NEEDED 2
#define SQ_LINKING_NOT_NEEDED 1

/* the next define can be used to turn off printing of individual messages */
#if 0
#define diagnostics printf
#else
#define diagnostics(a, b...)
#endif

/* some static variables for debugging */
static EmcPose oldPos, oldVel, newVel, newAcc;
static double oldDist;

/* functions for 'internal' use */
/* ---------------------------- */

static int sqDumpQueue(SEGMENTQUEUE * sq)
{
    SEGMENT *cursor;
    int i;

    cursor = sq->queue + sq->start;
    for (i = 0; i < sq->numSegments; i++) {
        if (cursor == 0) {
            diagnostics("Panic: NULL pointer in sqDumpQueue\n");
            return -1;
        }
        diagnostics("ID = %d, a=%d, Startpt: x=%g, y=%g, l=%g, tl=%g, ic=%g, fi=%g nls=%d ltps=%d\n",
                    cursor->ID, cursor->active,
                    cursor->start.tran.x,
                    cursor->start.tran.y,
                    cursor->length,
                    cursor->totLength,
                    cursor->initInc,
                    cursor->finalInc,
                    cursor->numLinkedSegs,
                    cursor->linkedToPrevSeg);

        cursor = cursor->nextSegment;

    }
    diagnostics("----------------------------------\n");
    return 0;
}


static double sqGiveMinAmaxTan(SEGMENT * s)
{
    /* give the minimum tangential acceleration for the chain of linked
       that starts with s. */
    SEGMENT *cursor;
    double minAmax;

    if (s == 0) {
        diagnostics("Error in sqGiveMinAmaxTan()\n");
        return -1;
    }

    if (s->numLinkedSegs == 0)
        return s->amaxTan;

    diagnostics("numLinkedSegs=%d\n", s->numLinkedSegs);

    minAmax = s->amaxTan;
    cursor = s;

    while (cursor->nextSegment != 0 && cursor->nextSegment->linkedToPrevSeg == 1) {
        cursor = cursor->nextSegment;
        if (cursor->amaxTan < minAmax) {
            minAmax = cursor->amaxTan;
        }
    }
    return minAmax;
}

static int sqLinkCriterion(SEGMENTQUEUE * sq, SEGMENT * s, double feedOverride)
{
    /* this function returns SQ_LINKING_NEEDED if the chain of segments starting
       with s is not long enough to accelerate or decelerate from the initial
       increment of the first segment in the chain to the final increment of the
       last segment in the chain
    */

    double amaxTan;
    double initInc, finalInc;
    int minNumSteps, i;
    SEGMENT *lastSeg;

    if (sq == 0 || s == 0) {
        diagnostics("Error in sqLinkCriterion\n");
        return -1;
    }

    if (s->linkedToPrevSeg == 1) {
        diagnostics("Segment s is linked to its preceding segment in sqLinkCriterion\n");
        return -1;
    }

    /* find the last segment of the chain */
    lastSeg = s;
    for (i = 0; i < s->numLinkedSegs; i++) {
        lastSeg = lastSeg->nextSegment;
        if (lastSeg == 0) {
            diagnostics("Panic: NULL pointer in sqLinkCriterion\n");
            return -1;
        }
    }

    /* find the minimum boundary for the acceleration in tangential
       direction */
    /* FIXME: this can be combined with the previous for statement that
       looks up the last segment of the chain */
    amaxTan = sqGiveMinAmaxTan(s);

    /* find the correct value for the initial increment: when s->plInitInc is
       non zero, this means that the segment has been processed before, and that
       value should be used */
    if (0 != s->plInitInc)
        initInc = s->plInitInc;
    else
        initInc = s->initInc;

    /* find the correct value of the final increment (with taking a feedoverride
       into account). */
    if (feedOverride >= 1.0) {
        finalInc = lastSeg->finalInc;
    } else {
        if (lastSeg->nextSegment != 0 && lastSeg->nextSegment->maxInc < lastSeg->maxInc)
            finalInc = min(lastSeg->finalInc, lastSeg->nextSegment->maxInc * feedOverride);
        else
            finalInc = min(lastSeg->finalInc, lastSeg->maxInc * feedOverride);
    }
    minNumSteps = ceil(3 * fabs(initInc - finalInc) / (2 * amaxTan * sq->ctPow2));
    if ((minNumSteps + 1) * (initInc + finalInc) / 2 - 
        initInc > s->totLength - 
        2 * initInc - finalInc - 5 * max(initInc, finalInc)) {
        diagnostics("Chain starting at %d is too short - linking needed\n", s->ID);
        return SQ_LINKING_NEEDED;
    } else {
        diagnostics("Chain starting at %d is NOT too short - linking NOT needed\n", s->ID);
        return SQ_LINKING_NOT_NEEDED;
    }
}

static int sqTestQueueIntegrity(SEGMENTQUEUE * sq)
{
    SEGMENT *cursor;

    cursor = sq->queue + sq->start;

    while (cursor != 0) {
        if (cursor->linkedToPrevSeg == 0) {
            diagnostics("ID = %d, nls = %d ", cursor->ID, cursor->numLinkedSegs);
            if (SQ_LINKING_NEEDED == sqLinkCriterion(sq, cursor, 1.0))
                diagnostics(" not ok\n");
            else
                diagnostics(" ok\n");

        }
        cursor = cursor->nextSegment;
    }

    diagnostics("----------------------------------\n");
    return 0;

}

static double sqGiveLength(EmcPose p1, EmcPose p2)
{
    PmCartesian disp;
    double mag;

    pmCartCartSub(p1.tran, p2.tran, &disp);
    pmCartMag(disp, &mag);

    return mag;
};

static double sqGiveCornerVelocity(SEGMENT * s1, SEGMENT * s2, double amax,
                                   double cycleTime)
{
    /* s1 and s2 must have been initialized correctly and must have a nonzero
       lenght in order to get a valid corner */

    PmCartesian v1;
    PmCartesian v2;
    PmCartesian me;
    PmCartesian diff;
    PmCartesian helix;

    double maxdiff;

    if (s1 == 0 || s2 == 0) {
        diagnostics("Error in sqGiveCornerVelocity()\n");
        return -1;
    }

    /* check if s1 or s2 has a zero length or amax or cycleTime is zero */
    if (s1->length == 0 || s2->length == 0 || amax == 0 || cycleTime == 0) {
        /* of course this shouldn't happen */
        diagnostics("Error in sqGiveCornerVelocity()\n");
        return -1;
    }

    if (s1->type == SQ_LINEAR) {
        v1 = s1->line.uVec;
    } else {
        pmCartCartSub(s1->end.tran, s1->circle.center, &me);
        pmCartCartCross(s1->circle.normal, me, &v1);
        pmCartScalDiv(s1->circle.rHelix, s1->circle.angle, &helix);
        pmCartCartAdd(v1, helix, &v1);
        pmCartUnit(v1, &v1);

    }

    if (s2->type == SQ_LINEAR) {
        v2 = s2->line.uVec;
    } else {
        pmCartScalDiv(s2->circle.rHelix, s2->circle.angle, &helix);
        pmCartCartAdd(s2->circle.rPerp, helix, &v2);
        pmCartUnit(v2, &v2);
    }

    /* calculate the difference between v1 and v2 */
    pmCartCartSub(v2, v1, &diff);

    /* select the largest element in diff */
    maxdiff = max(fabs(diff.x), fabs(diff.y));
    maxdiff = max(maxdiff, fabs(diff.z));

    /* return cornerVelocity */
    if (maxdiff <= 0.0)
        return DBL_MAX;
    else
        return (amax * cycleTime / maxdiff);

};


static double sqGiveMaxInc(SEGMENTQUEUE * sq, SEGMENT * s)
/* give the right maxInc from the chain of linked segments that start
   with s. */
{
    int done = 0;
    int minNumSteps;
    double startInc, finalInc;
    SEGMENT *cursor;
    double l;
    double maxAccTan;

    if (sq == 0 || s == 0) {
        diagnostics("Error 1 in sqGiveMaxInc()\n");
        return -1;
    }

    if (s->numLinkedSegs == 0)
        return s->maxInc;

    maxAccTan = sqGiveMinAmaxTan(s);

    startInc = s->plInitInc;
    cursor = s;
    l = s->length;
    while (!done) {
        if (cursor->nextSegment == 0) {
            return cursor->maxInc;
        }
        if (cursor->nextSegment != 0 && cursor->nextSegment->maxInc < cursor->maxInc)
            finalInc = min(cursor->finalInc, cursor->nextSegment->maxInc * sq->feedOverrideFactor);
        else
            finalInc = min(cursor->finalInc, cursor->maxInc * sq->feedOverrideFactor);
        minNumSteps = ceil(3 * fabs(finalInc - startInc) / (2 * maxAccTan * sq->ctPow2));

        if ((minNumSteps + 1) * (startInc + finalInc) / 2 - startInc > l - 5 * max(finalInc, startInc)) {
            /* this segment was too short and therefore linked to the next one */
            cursor = cursor->nextSegment;
            l += cursor->length;
        } else
            done = 1;
    }

    diagnostics("max increment for chain starting at %d is %g\n", s->ID, cursor->maxInc);
    return cursor->maxInc;
}


static int sqPlanSegment(SEGMENTQUEUE * sq, SEGMENT * s)
{
    /* variable declaration */
    double length;
    double maxInc;
    double amaxTan;

    SEGMENT *cursor;

    int validSolutionFound;

    if (sq == 0) {
        diagnostics("Error in sqPlanSegment()\n");
        return -1;
    }

    /* check is s is a valid pointer */
    if (s == 0) {
        diagnostics("Error in sqPlanSegment()\n");
        return -1;
    }

    /* check if this segment is linked to its predecessor (which should never
       happen!) */
    if (s->linkedToPrevSeg == 1) {
        diagnostics("Error in sqPlanSegment()\n");
        return -1;
    }

    length = s->length;
    cursor = s;

    /* check if there are any segments linked to s, find the total length of the
       linked segments and find the finalInc of the last segment in the chain */
    while (cursor->nextSegment != 0 && cursor->nextSegment->linkedToPrevSeg) {
        cursor = cursor->nextSegment;
    }

    /*  if (SQ_LINKING_NEEDED == sqLinkCriterion(sq, s, 1.0)) {
        diagnostics("Error in sqPlanSegment(), chain too short\n");
        if (s->initInc > cursor->finalInc)
        {
        diagnostics("InitInc > finalInc \n");
        diagnostics("Diff = %e\n", s->initInc - cursor->finalInc);
        }

        else
        {
        diagnostics("InitInc < finalInc \n");
        diagnostics("Diff = %e\n", s->initInc - cursor->finalInc);
        }

        diagnostics("ID = %d\n", s->ID);
        diagnostics("Startpt: x=%d, y=%d, z=%d\n",
        (int) (10000.0 * s->start.tran.x),
        (int) (10000.0 * s->start.tran.y),
        (int) (10000.0 * s->start.tran.z));
        sqDumpQueue(sq);
        sq->done=1;
        }

    */

    /* Check if the subsequent segment is long enough to decelerate to zero
     */
    /*
      if (cursor->finalInc !=0 && SQ_LINKING_NEEDED == sqLinkCriterion(sq,cursor->nextSegment,1.0))
      {
      diagnostics("Warning: the next segment is currently not long enough!!!\n");
      }

    */

    maxInc = sqGiveMaxInc(sq, s);
    diagnostics("maxinc %g\n", maxInc);
    maxInc *= sq->feedOverrideFactor;
    diagnostics("scaled maxinc %g\n", maxInc);

    /* maxInc should never exceed the the system's maximum increment (maxV*cycleTime) */
    maxInc = min(maxInc, sq->maxV * sq->cycleTime);
    diagnostics("capped maxinc %g\n", maxInc);

    /* find the minimum value for the maximum tangential acceleration for this
       chain of segments beginning with s */
    amaxTan = sqGiveMinAmaxTan(s);
    diagnostics("AmaxTan in sqplansegment  = %g, maxInc %g \n", amaxTan, maxInc);
    /* finalInc ( = initInc of nextSegment) should never be larger than
       the maxInc * feedOveride of this segment and the next segment */

    if (cursor->finalInc != 0) {        /* if true: this is not the last segment */
        if (cursor->nextSegment->maxInc < cursor->maxInc) {
            s->plFinalInc = min(cursor->finalInc,
                                cursor->nextSegment->maxInc * sq->feedOverrideFactor);
        } else {
            s->plFinalInc = min(cursor->finalInc, maxInc);
        }
        cursor->nextSegment->plInitInc = s->plFinalInc;
    } else {
        s->plFinalInc = 0;
    }

    /* the first two steps and the last step are already defined */
    length = s->totLength - 2 * s->plInitInc - s->plFinalInc;
    if (length < 0) {
        diagnostics("Panic: length negative in sqPlanSegment\n");
        diagnostics("plInitInc = %e plFinalInc = %e\n", s->plInitInc, s->plFinalInc);
        sq->done = 1;
        return -1;
    }

    /* calculate minimal number of steps for the first phase (acc) */
    if (s->plInitInc == maxInc)
        s->m = 0;
    else
        /* m should be always be two steps or more */
        s->m =  max(2, ceil(3 * fabs(maxInc - s->plInitInc) / (2 * amaxTan * sq->ctPow2)));

    /* calulate minimal number of steps for the third phase (dec) */
    if (s->plFinalInc == maxInc)
        s->q = 0;
    else
        s->q = max(2, ceil(3 * fabs(maxInc - s->plFinalInc) / (2 * amaxTan * sq->ctPow2)));

    /* calculate the minimal number of steps needed for the cruising phase */
    if (length - ((s->m + 1) * (s->plInitInc + maxInc) / 2 - s->plInitInc) * (s->m != 0) -
        ((s->q + 1) * (maxInc + s->plFinalInc) / 2 - maxInc) * (s->q != 0) < 0) {
        /* true: there is no cruising phase */
        s->p = 3;
        validSolutionFound = 0;
        if (maxInc > s->plInitInc && maxInc > s->plFinalInc) {
            maxInc = -s->p * amaxTan * sq->ctPow2 / 3 +
                sqrt(4 * s->p * s->p * amaxTan * amaxTan * sq->ctPow2 * sq->ctPow2 +
                     18 * (s->plInitInc * s->plInitInc + s->plFinalInc * s->plFinalInc) +
                     12 * amaxTan * sq->ctPow2 * (s->plInitInc - s->plFinalInc + 2 * length)) / 6;
            diagnostics("1: maxInc = %d\n", (int) (100000.0 * maxInc));
            if (maxInc > s->plInitInc && maxInc > s->plFinalInc) {
                validSolutionFound = 1;
            }
        }

        if ((s->plInitInc >= maxInc && maxInc > s->plFinalInc) || !validSolutionFound) {
            maxInc = 3 * ((s->plFinalInc * s->plFinalInc - s->plInitInc * s->plInitInc) +
                          2 * amaxTan * sq->ctPow2 * (s->plInitInc - s->plFinalInc + 2 * length)) /
                (4 * s->p * amaxTan * sq->ctPow2);
            diagnostics("2: maxInc = %d\n", (int) (100000.0 * maxInc));
            if (s->plInitInc >= maxInc && maxInc > s->plFinalInc) {
                validSolutionFound = 1;
            }
        }
        if ((s->plInitInc < maxInc && maxInc <= s->plFinalInc) || !validSolutionFound) {
            maxInc = (3 * (s->plInitInc * s->plInitInc - s->plFinalInc * s->plFinalInc) +
                      2 * amaxTan * sq->ctPow2 * (s->plInitInc - s->plFinalInc + 2 * length)) /
                (4 * s->p * amaxTan * sq->ctPow2);
            diagnostics("3: maxInc = %d\n", (int) (100000.0 * maxInc));
            if (s->plInitInc < maxInc && maxInc <= s->plFinalInc) {
                validSolutionFound = 1;
            }
        }
        if ((maxInc <= s->plInitInc && maxInc <= s->plFinalInc) || !validSolutionFound) {
            maxInc = s->p * amaxTan * sq->ctPow2 /
                3 + sqrt(4 * s->p * s->p * amaxTan * amaxTan * sq->ctPow2 * sq->ctPow2 +
                         18 * (s->plInitInc * s->plInitInc + s->plFinalInc * s->plFinalInc) +
                         12 * amaxTan * sq->ctPow2 * (-s->plInitInc + s->plFinalInc - 2 * length)) / 6;
            diagnostics("4: maxInc = %d\n", (int) (100000.0 * maxInc));
            if (maxInc <= s->plInitInc && maxInc <= s->plFinalInc) {
                validSolutionFound = 1;
            }
        }
        if (!validSolutionFound) {
            diagnostics("No solution found in sqPlanSegment\n");
            return -1;
        }

        /* recalculate m and q */
        s->m = ceil(3 * fabs(maxInc - s->plInitInc) / (2 * amaxTan * sq->ctPow2));
        s->q = ceil(3 * fabs(maxInc - s->plFinalInc) / (2 * amaxTan * sq->ctPow2));
        s->p = ceil((length - (s->m != 0) * ((s->m + 1) * (s->plInitInc + maxInc) / 2 - s->plInitInc) -
                     (s->q != 0) * ((s->q + 1) * (s->plFinalInc + maxInc) / 2 - maxInc)) / maxInc);
    } else
        s->p = ceil((length - (s->m != 0) * ((s->m + 1) * (s->plInitInc + maxInc) / 2 - s->plInitInc) -
                     (s->q != 0) * ((s->q + 1) * (s->plFinalInc + maxInc) / 2 - maxInc)) / maxInc);

    /* adjust maxInc a little */
    maxInc = (2 * length - (s->m != 0) * (s->m - 1) * s->plInitInc -
              (s->q != 0) * (s->q + 1) * s->plFinalInc) /
        ((s->m != 0) * (s->m + 1) + 2 * s->p + (s->q != 0) * (s->q - 1));

    s->totNumPoints = s->m + s->p + s->q + 3;
    s->cruiseInc = maxInc;

    /* ok, we've found (sub)-optimal values for m, p and q. Let's go on
       with the actual planning */

    if (s->m != 0) {
        s->b1 = 3 * (maxInc - s->plInitInc) / ((s->m * s->m * sq->ctPow2));
        s->a1 = -2 * s->b1 / (3 * s->m * sq->cycleTime);
        s->c1 = 0;
        s->d1 = s->plInitInc;
    } else
        s->a1 = s->b1 = s->c1 = s->d1 = 0;

    if (s->q != 0) {
        s->b3 = 3 * (s->plFinalInc - maxInc) / ((s->q * s->q * sq->ctPow2));
        s->a3 = -2 * s->b3 / (3 * s->q * sq->cycleTime);
        s->c3 = 0;
        s->d3 = maxInc;
    } else
        s->a3 = s->b3 = s->c3 = s->d3 = 0;

    s->planningDone = 1;

    return 0;
};

/* function to link s2 to s1
   Here it is assumed that s1 and s2 are indeed successive segments */
static int sqLinkSegments(SEGMENT * s1, SEGMENT * s2, int priority)
{
    diagnostics("Linking %d to %d\n", s1->ID, s2->ID);

    if (s1 == 0) {
        diagnostics("Error 1a in sqLinkSegments() \n");
        return -1;
    }
    if (s2 == 0) {
        diagnostics("Error 1b in sqLinkSegments() \n");
        return -1;
    }

    /* if s2 is not the segment that immediately follows s1, then linking
       can not be done */

    if (s1->nextSegment != s2) {
        diagnostics("Error 2 in sqLinkSegments()\n");
        return -1;
    }

    /* if s1 is active, linking should not be done */
    if (s1->active == 1) {
        /*        diagnostics("Error in sqLinkSegments()\n"); */
        /*        return -1; */
        return 0;
    }

    /* find the first element in the series of linked segments of which s1 is
       the last one */
    while (s1->linkedToPrevSeg) {
        if (s1 == 0) {
            diagnostics("Panic: NULL pointer in sqLinkSegments\n");
            return -1;
        }
        s1 = s1->prevSegment;
    }

    if (s1->numLinkedSegs > SQ_MAX_NUM_LINKED_SEGS && priority == SQ_LOW_LINKING_PRIORITY)
        return 0;

    s1->numLinkedSegs += s2->numLinkedSegs + 1;
    s1->totLength += s2->totLength;
    s2->linkedToPrevSeg = 1;
    s2->numLinkedSegs = 0;

    return 0;
}

static int sqForwardLinkSegment(SEGMENTQUEUE * sq, SEGMENT * s, double feedOverride)
{
    /* function that checks whether segment s needs to be linked to its
       successor(s) based upon its initial increment. It is possible that
       this requires linking of a set of succeding segments to make sure that
       the final increment can be reached. */

    SEGMENT *cursor;
    int done, counter;
    int linkcrit;

    diagnostics("Checking whether segment %d needs to be linked forward...\n", s->ID);

    if (sq == 0 || s == 0) {
        diagnostics("Error in sqForwardLinkSegment\n");
        return -1;
    }

    if (s->initInc - s->finalInc > 1e-10) {
        diagnostics("sqForwardlinkSegment called while initInc is larger than finalInc\n");
        /* not critical, so we don't return -1, but it's not good.... */
        return 0;
    }

    cursor = s;
    done = 0;
    counter = 0;

    while (!done) {
        counter++;
        if (counter > sq->size) {
            /* we have browsed through the whole queue and can't
               get out of the loop.... */
            diagnostics("Can't get out of the loop in sqForwardLinkSegment\n");
            return -1;
        }
        if (cursor->nextSegment == 0) {
            /* then this is the last segment in the queue, which always has
               a final velocity of zero that can always be reached. Ergo: we're
               done */
            done = 1;
        } else if (cursor->nextSegment->linkedToPrevSeg == 1) {
            /* the next segment is already linked to cursor, so let's go on */

            cursor = cursor->nextSegment;
            if (cursor == 0) {
                /* this is VERY unlikely to happen, since we already checked
                   that a few lines before, but just in case...... */
                diagnostics("Panic: cursor = NULL in sqForwardLinkSegment\n");
                return -1;
            }
        } else if (s->initInc > cursor->finalInc) {
            /* then we're done. This is always true as long as the set of
               segments that succeed s is correctly processed: if the
               succeeding segments are correctly processed, then
               cursor->finalInc can be reached from s->finalInc
               (= s->nextSegment->initInc) over the distance of the segments
               s->nextSegment to cursor. The final increment of s is larger
               than the initial increment (otherwise this function wouldn't
               have been called), so the distance of the segments s to cursor is
               of course large enough too to decelerate from s->initInc to
               cursor->finalInc */
            done = 1;
        } else {
            if (-1 == (linkcrit = sqLinkCriterion(sq, s, feedOverride))) {
                diagnostics("Error in sqForwardLinkSegment\n");
                return -1;
            } else if (linkcrit == SQ_LINKING_NEEDED) {
                cursor = cursor->nextSegment;
                if (cursor == 0) {
                    /* again, this is very unlikely to happen */
                    diagnostics("Panic: cursor = NULL in sqForwardLinkSegment\n");
                    return -1;
                }
                if (-1 == sqLinkSegments(cursor->prevSegment, cursor, SQ_HIGH_LINKING_PRIORITY)) {
                    diagnostics("Error in sqForwardLinkSegment\n");
                    return -1;
                }
            } else
                /* linkcrit==SQ_NO_LINKING_NEEDED */
                done = 1;               /* no further linking needed */
        }
        if (cursor == sq->queue + sq->end) {
            /* sq->end points to the first free position in the ring buffer
               so there's no segment at that position. So if cursor points to
               that position, something is very wrong */
            diagnostics("End of queue reached, no further linking possible\n");
            done = 1;
        }
    }
    diagnostics("... done linking forward\n");
    return 0;
}

static int sqBackwardLinkSegment(SEGMENTQUEUE * sq, SEGMENT * s, double feedOverride)
{
    /* function that checks whether segment s needs to be linked to its
       predecessor(s) based upon the finalInc. It is possible that
       this requires linking of a set of preceding segments to make sure that
       the final increment can be reached. */

    SEGMENT *cursor;
    int done, counter;
    int linkcrit;

    diagnostics("Checking whether segment %d needs to be linked backward...\n", s->ID);

    if (sq == 0 || s == 0) {
        diagnostics("Error in sqBackwardLinkSegment\n");
        return -1;
    }

    if (s->initInc - s->finalInc < -1e-10) {
        diagnostics("sqBackwardLinkSegment s=%d called but initInc is smaller than finalInc\n",
                    s->ID);
        sqDumpQueue(sq);
        /* not critical, so we don't return -1, but it's not good.... */
        return 0;
    }

    cursor = s;
    done = 0;
    counter = 0;

    while (!done) {
        counter++;
        if (counter > sq->size) {
            /* we have browsed through the whole queue and can't
               get out of the loop.... */
            diagnostics("Can't get out of the loop in sqBackwardLinkSegment\n");
            return -1;
        }

        if (cursor->linkedToPrevSeg == 1) {
            /* it is already linked */
            cursor = cursor->prevSegment;
            if (cursor == 0) {
                diagnostics("Panic: cursor = NULL in sqBackwardLinkSegment\n");
                return -1;
            }
        } else if (cursor->initInc < s->finalInc) {
            /* then we're done. This is always true as long as the set of
               segments that precede s is correctly processed, since the
               total length of the segments from cursor to the last segment
               before s is large enough to reach s's initial increment. This
               length is of course large enough too to reach s->finalInc
               because that is smaller than s->initInc (otherwise this
               function wouldn't have called). */
            done = 1;
        } else {
            if (-1 == (linkcrit = sqLinkCriterion(sq, cursor, feedOverride))) {
                diagnostics("Error in sqBackWardLinkSegment\n");
                return -1;
            } else if (linkcrit == SQ_LINKING_NEEDED) {
                cursor = cursor->prevSegment;
                if (cursor == 0) {
                    diagnostics("Panic: cursor = NULL in sqBackwardLinkSegment\n");
                    return -1;
                }
                if (-1 == sqLinkSegments(cursor, cursor->nextSegment, SQ_HIGH_LINKING_PRIORITY)) {
                    diagnostics("Error in sqBackwardLinkSegment\n");
                    return -1;
                }
            } else
                done = 1;               /* no further linking needed */
        }
        if (cursor == sq->queue + sq->start) {
            diagnostics("Beginning of queue reached, no further linking possible\n");
            done = 1;
        }
    }
    diagnostics("... done linking backward\n");
    return 0;
}

static int sqPreprocessSegment(SEGMENTQUEUE * sq, SEGMENT * newseg)
{

    /* this is the stuff that's needs to be done for both linear and
       circular motions. Only sqAddLine and sqAddCircle should call this
       function.
    */

    double cornerInc;

    SEGMENT *prevseg;
    SEGMENT *cursor;

    diagnostics("Preprocess Segment %d\n", newseg->ID);

    /* check if segment queue has been initialized and if newseg is valid */
    if (sq == 0 || sq->queue == 0 || newseg == 0) {
        diagnostics("Error 1 in sqPreprocessSegment()\n");
        return -1;
    }

    /* if this is the first segment.... */
    if (sq->numSegments == 1) {
        diagnostics("this is the first segment\n");
        newseg->initInc = 0;
        newseg->start = sq->lastPoint;
        newseg->prevSegment = 0;
        if (sq->paused != 1 && sq->stepping != 1) {
            diagnostics("we are NOT done\n");
            sq->done = 0;
        }
    } else {
        /* if not ... */
        prevseg = sq->queue + ((sq->end + sq->size - 2) % sq->size);
        newseg->start = prevseg->end;
        prevseg->nextSegment = newseg;
        newseg->prevSegment = prevseg;

        if (prevseg->active == 1) {
            /* we can't change anything of the active segment */
            newseg->initInc = prevseg->finalInc;
        } else {

            /* calculate the corner velocity for the corner between prevSegment
               and newseg */

            if (-1 == (cornerInc = sqGiveCornerVelocity(prevseg, newseg, sq->maxAcc, sq->cycleTime) *
                       sq->cycleTime)) {
                diagnostics("Error 2 in sqPreprocessSegment()\n");
                return -1;
            }

            /* if the maximum speeds of the new and the previous segment
               are the same and the corner speed is larger than this
               maximum speed, the segments can be linked */
            if ((prevseg->maxInc - newseg->maxInc < 1e-8) && (cornerInc > newseg->maxInc)) {
                diagnostics("Linking ID = %d for conv \n", newseg->ID);
                if (-1 == sqLinkSegments(prevseg, newseg, SQ_LOW_LINKING_PRIORITY)) {
                    diagnostics("Error 3 in sqPreprocessSegment()\n");
                    return -1;
                }
                cornerInc = prevseg->maxInc;
                newseg->initInc = cornerInc;
                prevseg->finalInc = cornerInc;
            } else {
                /* the corner velocity shouldn't exceed the maximum
                   speeds of both segments */

                if ((cornerInc > prevseg->maxInc) || (cornerInc > newseg->maxInc))
                    cornerInc = min(prevseg->maxInc, newseg->maxInc);

                newseg->initInc = cornerInc;
                prevseg->finalInc = cornerInc;

                /* is prevseg long enough to acc/dec from initInc to finalInc
                   (=cornerInc)? */
                cursor = prevseg;
                /* check if prevseg is linked to its predecessor and
                   if so, find the first segment in the linked
                   series of segments */

                if (prevseg->linkedToPrevSeg == 1) {
                    while (cursor->linkedToPrevSeg) {
                        cursor = cursor->prevSegment;
                        if (cursor == 0) {
                            diagnostics("Panic: cursor = NULL in sqPreprocessSegment\n");
                            return -1;
                        }
                    }
                }
                if (SQ_LINKING_NEEDED ==  sqLinkCriterion(sq, cursor, sq->maxFeedOverrideFactor)) {
                    if (cursor->initInc <= cornerInc) {
                        /* check if prevseg needs to be linked to its next
                           segment(s), which in this case only the newseg
                           can be. The feedoverride factor is set to the
                           maximum feedoverride to ensure correct linking
                           for every possible feed */
                        diagnostics("Calling FLS for ID = %d\n", cursor->ID);
                        if (-1 == sqForwardLinkSegment(sq, cursor, sq->maxFeedOverrideFactor)) {
                            diagnostics("Error 4 in sqPreprocessSegment()\n");
                            return -1;
                        }
                    } else {
                        /* check if the prevseg needs to be linked to its
                           predecessor(s). The feedoverride factor is set to
                           the maximum feedoverride to ensure correct linking
                           for every possible feed */
                        diagnostics("Calling BLS for ID = %d\n", cursor->ID);
                        if (-1 == sqBackwardLinkSegment(sq, cursor, sq->maxFeedOverrideFactor)) {
                            diagnostics("Error 5 in sqPreprocessSegment()\n");
                            return -1;
                        }
                    }
                }
                if (SQ_LINKING_NEEDED == sqLinkCriterion(sq, cursor, sq->maxFeedOverrideFactor)) {
                    /* adjust the maxInc in a smart way */
                    diagnostics("Prevseg too short, adjusting feeds\n");
                    newseg->initInc = cursor->totLength / 12.0;
                    prevseg->finalInc = newseg->initInc;
                    cursor->initInc = newseg->initInc;
                    cursor->prevSegment->finalInc = newseg->initInc;
                    /* FIXME: this might cause an issue with the segment preceding cursor */
                }
                if (!newseg->linkedToPrevSeg &&
                    SQ_LINKING_NEEDED == sqLinkCriterion(sq, newseg, sq->maxFeedOverrideFactor)) {
                    diagnostics("New segment too short, linking.\n");
                    sqBackwardLinkSegment(sq, newseg, sq->maxFeedOverrideFactor);
                }
            }
        }
    }
    //  sqTestQueueIntegrity(sq);
    return 0;
}

/* interface functions */
/* ------------------- */

int sqCreate(SEGMENTQUEUE * sq, SEGMENT * first, int size)
{
    if (size <= 0 || sq == 0 || first == 0) {
        diagnostics("Error in sqInitQueue()\n");
        return -1;
    }

    sq->queue = first;
    sq->size = size;

    sq->start = sq->end = 0;
    sq->full = 0;
    sq->numSegments = 0;

    sq->initXYZ.tran.x = 0;
    sq->initXYZ.tran.y = 0;
    sq->initXYZ.tran.z = 0;
    sq->lastPoint = sq->initXYZ;
    sq->n = 0;
    sq->maxFeedOverrideFactor = 1.0;    /* this has to be set at start-up using
                                           sqSetMaxFeedOverride() */
    sq->feedOverrideFactor = 1.0;
    sq->cycleTime = 0;
    sq->maxAcc = 0;
    sq->maxV = 0;
    sq->currentID = 0;

    sq->done = 1;                       /* the queue is empty, so by definition we're done */
    sq->paused = 0;
    sq->stepping = 0;
    sq->feedAdjusting = 0;
    sq->aborting = 0;

    /* initializing debug variables */
    oldPos = sq->lastPoint;
    oldVel.tran.x = 0;
    oldVel.tran.y = 0;
    oldVel.tran.z = 0;
    oldDist = 0;
    return 0;
};

int sqSetAmax(SEGMENTQUEUE * sq, double amax)
{
    if (sq == 0 || amax <= 0) {
        diagnostics("Error in SetMaxAcc!!!\n");
        return -1;
    }
    sq->maxAcc = amax / 2;
    return 0;
};

int sqSetVmax(SEGMENTQUEUE * sq, double vmax)
{
    if (sq == 0 || vmax <= 0) {
        diagnostics("Error in SetVmax!!! sq %p vmax %g\n", sq, vmax);
        return -1;
    }
    diagnostics("SetVmax sq %p vmax %g\n", sq, vmax);
    sq->maxV = vmax;
    return 0;
};

int sqSetCycleTime(SEGMENTQUEUE * sq, double secs)
{
    if (sq == 0 || secs == 0) {
        diagnostics("Cycletime is zero!!! sq %p secs %g\n", sq, secs);
        return -1;
    }
    diagnostics("Cycletime sq %p secs %g\n", sq, secs);
    sq->cycleTime = secs;
    sq->ctPow2 = secs * secs;
    sq->ctPow3 = sq->ctPow2 * secs;
    return 0;
};

int sqSetMaxFeedOverride(SEGMENTQUEUE * sq, double mfo)
{
    if (sq == 0 || mfo <= 0) {
        diagnostics("Error in sqSetMaxFeedOverride()\n");
        return -1;
    }
    sq->maxFeedOverrideFactor = mfo;
    diagnostics("maxFeedOverrideFactor set to: %e\n", mfo);
    return 0;
}

int sqSetPos(SEGMENTQUEUE * sq, EmcPose pos)
{
    if (sq == 0) {
        diagnostics("Error in sqSetPos()\n");
        return -1;
    }
    sq->initXYZ = pos;
    sq->lastPoint = sq->initXYZ;
    return 0;
}

int sqClear(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqClearQueue(): sq == 0 \n");
        return -1;
    }
    sq->numSegments = 0;
    sq->start = sq->end = 0;
    sq->full = 0;
    sq->n = 0;
    sq->feedOverrideFactor = 1.0;
    sq->currentID = 0;

    sq->done = 1;                       /* the queue is empty, so by definition we're done */
    sq->paused = 0;
    sq->stepping = 0;
    sq->feedAdjusting = 0;
    sq->aborting = 0;

    sq->currentVel = 0;
    oldDist = 0;
    oldPos = sq->lastPoint;
    oldVel.tran.x = 0;
    oldVel.tran.y = 0;
    oldVel.tran.z = 0;

    return 0;
}

int sqTrashQueue(SEGMENTQUEUE * sq)
{
    if (sq == 0 || sq->queue == 0) {
        diagnostics("Error in sqTrashQueue()\n");
        return -1;
    }
    sqClear(sq);
    sq->queue = 0;
    return 0;
};

/* function to set the feed rate for the motions appended after this command */
int sqSetFeed(SEGMENTQUEUE * sq, double feed)
{
    if (sq == 0) {
        diagnostics("Error in sqSetFeed: null pointer\n");
        return -1;
    }
    diagnostics("sqSetFeed: %g\n", feed);

    if (feed <= 0.0) {
        /* tp.c ignores this and returns an error that's never checked */
        diagnostics("Error in sqSetFeed: non-positive feed %d/1000, using old feed %d/1000\n", (int) (feed*1000.0), (int) (sq->feed*1000.0));
        feed = sq->feed;
    }

    if (feed > sq->maxV)
        sq->feed = sq->maxV;
    else
        sq->feed = feed;
    return 0;
}

static void emcpose_to_pmpose(EmcPose e, PmPose * p)
{
    p->tran = e.tran;
    p->rot.s = 1;
    p->rot.x = e.a;
    p->rot.y = e.b;
    p->rot.z = e.c;

    return;
}

static void pmpose_to_emcpose(PmPose p, EmcPose * e)
{
    e->tran = p.tran;
    e->a = p.rot.x;
    e->b = p.rot.y;
    e->c = p.rot.z;

    return;
}

int sqAddLine(SEGMENTQUEUE * sq, EmcPose end, int ID)
{
    double length, maxUVec;
    SEGMENT *newseg;
    EmcPose start;
    PmPose start_pose, end_pose;

    /* check if segment queue has been initialized */
    if (sq == 0 || sq->queue == 0) {
        diagnostics("Error in sqAddLine()\n");
        return -1;
    }

    /* check for full */
    if (sq->numSegments == sq->size) {
        diagnostics("Panic!!!!, segmentqueue overflows!!!\n");
        return -1;
    }

    /* check if new motion has zero length */
    if (sq->numSegments == 0)
        start = sq->lastPoint;
    else
        start = sq->queue[(sq->end + sq->size - 1) % sq->size].end;

    length = sqGiveLength(start, end);
    diagnostics("addline length %g\n", length);

    if (length == 0) {
        /* only set ID of last appended motion */
        sq->lastAppMotionID = ID;
        return 0;
    }

    /* let newseg point to the first empty place in the ring buffer... */
    newseg = sq->queue + sq->end;
    /* ...and update the ring buffer properties */
    sq->end = (sq->end + 1) % sq->size;
    sq->numSegments++;
    if (sq->numSegments >= sq->size - SQ_SAFETY_MARGIN) {
        sq->full = 1;
    }

    /* fill segment parameter fields */
    newseg->ID = ID;
    newseg->type = SQ_LINEAR;
    newseg->length = length;
    newseg->totLength = length;
    newseg->start = start;
    newseg->end = end;
    newseg->maxInc = sq->feed * sq->cycleTime;
    if (newseg->maxInc > length) {
        newseg->maxInc = length;
    }
    diagnostics("newseg feed %g cycletime %g length %g\n", sq->feed, sq->cycleTime, length);
    newseg->finalInc = 0;
    newseg->plInitInc = 0;
    newseg->plFinalInc = 0;
    newseg->cruiseInc = 0;
    newseg->planningDone = 0;
    newseg->active = 0;
    newseg->numLinkedSegs = 0;
    newseg->linkedToPrevSeg = 0;
    newseg->totNumPoints = 0;
    newseg->nextSegment = 0;

    /* initialize line */
    emcpose_to_pmpose(newseg->start, &start_pose);
    emcpose_to_pmpose(newseg->end, &end_pose);
    pmLineInit(&newseg->line, start_pose, end_pose);

    /* set the maximum tangential acceleration for this line */
    maxUVec = max(fabs(newseg->line.uVec.x), fabs(newseg->line.uVec.y));
    maxUVec = max(fabs(newseg->line.uVec.z), maxUVec);
    newseg->amaxTan = sq->maxAcc / maxUVec;

    diagnostics("Amax tan = %g\n", newseg->amaxTan);

    if (-1 == sqPreprocessSegment(sq, newseg)) {
        diagnostics("Error in sqAddLine()\n");
        return -1;
    }

    /* set last Appended Motion ID */
    sq->lastAppMotionID = ID;

    return 0;
}

int sqAddCircle(SEGMENTQUEUE * sq, EmcPose end, PmCartesian center,
                PmCartesian normal, int turn, int ID)
{
    SEGMENT *newseg;
    PmCircle circle;
    EmcPose start;
    PmCartesian helix;
    double absHelix;
    PmPose start_pose, end_pose;

    /* used to calculate the maximum tangential acceleration */
    double rpow2, A, topIncPow2;

    /* check if segment queue has been initialized */
    if (sq == 0 || sq->queue == 0) {
        diagnostics("Error in sqAddCircle()\n");
        return -1;
    }

    /* check for full */
    if (sq->numSegments == sq->size) {
        diagnostics("Panic!!!!, segmentqueue overflows!!!\n");
        return -1;
    }

    if (sq->numSegments == 0)
        start = sq->lastPoint;
    else
        start = sq->queue[(sq->end + sq->size - 1) % sq->size].end;

    emcpose_to_pmpose(start, &start_pose);
    emcpose_to_pmpose(end, &end_pose);
    pmCircleInit(&circle, start_pose, end_pose, center, normal, turn);

    if (circle.angle == 0) {
        /* only set ID of last appended motion */
        sq->lastAppMotionID = ID;
        return 0;
    }

    /* Calculate the helix gradient in normal direction */
    pmCartScalDiv(circle.rHelix, circle.angle, &helix);
    pmCartMag(helix, &absHelix);

    /* let newseg point to the first empty place in the ring buffer... */
    newseg = sq->queue + sq->end;
    /* ...and update the ring buffer properties */
    sq->end = (sq->end + 1) % sq->size;
    sq->numSegments++;
    if (sq->numSegments >= sq->size - SQ_SAFETY_MARGIN) {
        sq->full = 1;
    }

    /* fill segment parameter fields */
    newseg->ID = ID;
    newseg->type = SQ_CIRCULAR;
    newseg->circle = circle;

    newseg->helixRadius = sqrt(circle.radius * circle.radius
                               + absHelix * absHelix);
    newseg->length = circle.angle * newseg->helixRadius;

    newseg->totLength = newseg->length;
    newseg->start = start;
    newseg->end = end;
    newseg->maxInc = min(sq->feed * sq->cycleTime,
                         sqrt(sq->maxAcc * circle.radius) * sq->cycleTime);
    if (absHelix != 0)
        newseg->maxInc = min(newseg->maxInc, sq->feed * sq->cycleTime / absHelix);

    newseg->finalInc = 0;
    newseg->plInitInc = 0;
    newseg->plFinalInc = 0;
    newseg->cruiseInc = 0;
    newseg->planningDone = 0;
    newseg->active = 0;
    newseg->numLinkedSegs = 0;
    newseg->linkedToPrevSeg = 0;
    newseg->totNumPoints = 0;
    newseg->nextSegment = 0;

    /* calculate the maximum tangential acceleration for this circle */
    rpow2 = circle.radius * circle.radius;
    topIncPow2 = newseg->maxInc * newseg->maxInc;
    A = max(sq->maxAcc * sq->maxAcc * sq->ctPow2 * sq->ctPow2 * rpow2 -
            topIncPow2 * topIncPow2, 3.0 / 4.0 * topIncPow2 * topIncPow2);

    newseg->amaxTan = sqrt(A / (rpow2 * (rpow2 * +topIncPow2))) / sq->ctPow2;

    if (absHelix != 0) {
        newseg->amaxTan = min(newseg->amaxTan, sq->maxAcc * newseg->helixRadius / absHelix);
    }

    if (-1 == sqPreprocessSegment(sq, newseg)) {
        diagnostics("Error in sqAddCircle()\n");
        return -1;
    }

    /* set last Appended Motion ID */
    sq->lastAppMotionID = ID;

    return 0;
}

EmcPose sqGetPos(SEGMENTQUEUE * sq)
{
    EmcPose retval;
    if (sq == 0) {
        diagnostics("Error in sqGetPosition()\n");
        ZERO_EMC_POSE(retval);
        return retval;
    }

    return sq->lastPoint;
};

int sqRunCycle(SEGMENTQUEUE * sq, long period)
{
    SEGMENT *as;                        /* to use instead of sq->queue[sq->start],
                                           stands for Active Segment */
    int i;
    SEGMENT *cursor;
    double finalInc;
    int done;
    int minNumSteps;
    double angleToGo;
    double amaxTan;
    int turn;
    PmCartesian normal, center;
    PmPose last_point_pose;
    PmPose start_pose, end_pose;

    double npow1, npow2, npow3;    /* to speed up cubic calculations */

    if (sq == 0) {
        diagnostics("Error in sqRunCycle(): Segmentqueue doesn't exist!\n");
        return -1;
    }

    if (sq->full == 1 && sq->numSegments < sq->size - SQ_SAFETY_MARGIN)
        diagnostics("sq->full == 1 although queue's not full!!!!\n");

    if (sq->done == 1) {
        /* do nothing */
        return 0;
    }
    /* if buffer is empty give last point (i.e. do nothing) */
    if (sq->numSegments == 0) {
        /* set the 'done' flag ..... */
        sq->done = 1;

        /* check if currentID is the same as the ID of the last appended motion.
           The only case in which this will not be true is when the last motion
           has a zero length */
        if (sq->currentID != sq->lastAppMotionID)
            sq->currentID = sq->lastAppMotionID;
        return 0;
    }
    as = sq->queue + sq->start; /* much shorter ..... */
    if (as == 0) {
        diagnostics("Panic: as = NULL in sqRunCycle\n");
        return -1;
    }

    if (as->active == 0) {
        /* we're about to start with a new segment */
        diagnostics("starting with new segment\n");
        if (-1 == sqPlanSegment(sq, as)) {
            diagnostics("Error 1 in sqRunCycle\n");
            return -1;
        }
        /* mark the whole chain as active */
        cursor = as;
        for (i = 0; i <= as->numLinkedSegs; i++) {
            if (cursor == 0) {
                diagnostics("Panic: cursor 1 = NULL in sqRunCycle\n");
                return -1;
            }
            cursor->active = 1;
            cursor = cursor->nextSegment;
        }
        oldDist = 0;
        sq->dist = 0;

        /* reset base, cursor, offset and cumlength and n */
        sq->base = as->start;
        sq->cursor = as;
        sq->offset = 0;
        sq->cumLength = as->length;
        sq->n = 1;
        sq->currentID = as->ID;

    }
    /* else: planning has been done before and the parameters are correctly
       initialized, unless someone deliberately changed planningDone to 1.
       Let's trust our user and not check for that here. */

    /* depending of in what phase of the motion we are, determine the new
       distance */
    if (sq->n == 1 || sq->n == 2) {
        sq->dist += as->plInitInc;
    } else if (sq->n <= as->m + 2) {
        /* acceleration phase */
        npow1 = sq->n - 2;
        npow2 = npow1 * npow1;
        npow3 = npow2 * npow1;
        sq->dist += as->a1 * npow3 * sq->ctPow3
            + as->b1 * npow2 * sq->ctPow2 + as->c1 * npow1 * sq->cycleTime + as->d1;
    } else if (sq->n <= as->m + as->p + 2) {
        /* cruising phase */
        sq->dist += as->cruiseInc;
    } else if (sq->n <= as->m + as->p + as->q + 2) {
        /* deceleration phase */
        npow1 = sq->n - as->m - as->p - 2;
        npow2 = npow1 * npow1;
        npow3 = npow2 * npow1;
        sq->dist += as->a3 * npow3 * sq->ctPow3
            + as->b3 * npow2 * sq->ctPow2 + as->c3 * npow1 * sq->cycleTime + as->d3;

    } else if (sq->n == as->m + as->p + as->q + 3) {
        /* last step */
        sq->dist += as->plFinalInc;
    } else {
        /* we have a problem, because this should never happen */
        diagnostics("Error 2 in sqRunCycle\n");
        return -1;
    }

    /* transform the dist scalar into a XYZ triplet */
    if (as->nextSegment != 0 && (as->nextSegment->linkedToPrevSeg == 1 || sq->paused == 1)) {
        /* the active segment is the first segment of a chain */
        /* the sq->paused == 1 test is added to make sure that if a pause
           command is given just before the end of the segment, that the
           following segment is used to finish decelerating to zero */
        while ((sq->dist > sq->cumLength) && (sq->cursor->nextSegment != 0) &&
               ((sq->cursor->nextSegment->linkedToPrevSeg == 1) || sq->paused == 1)) {
            sq->offset = sq->cumLength;
            sq->base = sq->cursor->end;
            sq->cursor = sq->cursor->nextSegment;
            sq->cumLength += sq->cursor->length;

        }
        /* set currentID */
        sq->currentID = sq->cursor->ID;

        if (sq->cursor->type == SQ_LINEAR) {
            pmLinePoint(&sq->cursor->line, sq->dist - sq->offset, &last_point_pose);
        } else {
            pmCirclePoint(&sq->cursor->circle,
                          (sq->dist - sq->offset) / sq->cursor->helixRadius,
                          &last_point_pose);
        }
        pmpose_to_emcpose(last_point_pose, &sq->lastPoint);
    } else {
        /* the active segment has no other segments linked to it, which makes
           things much easier... */
        if (sq->cursor->type == SQ_LINEAR) {
            pmLinePoint(&as->line, sq->dist, &last_point_pose);
        } else {
            pmCirclePoint(&as->circle, sq->dist / as->helixRadius,
                          &last_point_pose);
        }
        pmpose_to_emcpose(last_point_pose, &sq->lastPoint);
    }

    sq->n++;

    if (sq->n > as->totNumPoints) {
        if (sq->aborting == 1) {
            if (-1 == sqClear(sq)) {
                diagnostics("Error 3 in sqRunCycle\n");
                return -1;
            }
        } else if (sq->paused == 1 || sq->feedAdjusting == 1) {
            if (sq->paused == 1) {
                sq->done = 1;
                finalInc = 0;
            } else {
                sq->feedAdjusting = 0;
                finalInc = as->plFinalInc;
            }

            /* remove all segments preceding the current segment */
            cursor = as;
            while (cursor != sq->cursor) {
                cursor = cursor->nextSegment;
                if (cursor == 0) {
                    diagnostics("Panic: cursor 2 = NULL in sqRunCycle\n");
                    return -1;
                }
                sq->numSegments--;
            }
            if (sq->numSegments < sq->size - SQ_SAFETY_MARGIN)
                sq->full = 0;
            if (sq->numSegments < 0) {
                diagnostics("Panic: sq->numSegments <0  in sqRunCycle\n");
                return -1;
            }

            cursor->prevSegment = 0;
            cursor->linkedToPrevSeg = 0;
            sq->start = sq->cursor - sq->queue;
            as = cursor;

            as->planningDone = 0;
            as->plInitInc = finalInc;
            as->start = sq->lastPoint;

            if (as->type == SQ_LINEAR) {
                as->length = sqGiveLength(as->start, as->end);
                as->totLength = as->length;
                emcpose_to_pmpose(as->start, &start_pose);
                emcpose_to_pmpose(as->end, &end_pose);
                pmLineInit(&as->line, start_pose, end_pose);
            } else {
                angleToGo = as->circle.angle - (sq->dist - sq->offset) / as->helixRadius;
                turn = floor(angleToGo / (2 * PM_PI));
                normal = as->circle.normal;
                center = as->circle.center;
                emcpose_to_pmpose(as->start, &start_pose);
                emcpose_to_pmpose(as->end, &end_pose);
                pmCircleInit(&as->circle, start_pose, end_pose, center, normal, turn);
                as->length = as->circle.angle * as->helixRadius;
                as->totLength = as->length;
            }

            as->active = 0;             /* mark the first segment of the chain
                                           as not active */

            /* determine how many segments are linked to as */
            as->numLinkedSegs = 0;
            while (cursor->nextSegment != 0 && cursor->nextSegment->linkedToPrevSeg == 1) {
                cursor = cursor->nextSegment;
                as->numLinkedSegs++;
                as->totLength += cursor->length;
            }
            /* find the minimum of the amax's of the segments in the chain */
            amaxTan = sqGiveMinAmaxTan(as);

            done = 0;
            /* keep linking segments until the chain is long enough */
            while (!done) {
                if (cursor->nextSegment != 0 && cursor->nextSegment->maxInc < cursor->maxInc)
                    finalInc = min(cursor->finalInc,
                                   cursor->nextSegment->maxInc * sq->feedOverrideFactor);

                else
                    finalInc = min(cursor->finalInc, cursor->maxInc * sq->feedOverrideFactor);

                amaxTan = min(amaxTan, cursor->amaxTan);
                minNumSteps = ceil(3 * fabs(finalInc - as->plInitInc) / (2 * amaxTan * sq->ctPow2));
                if ((minNumSteps + 1) * (as->plInitInc + finalInc) /
                    2 - as->plInitInc > as->totLength - 5 * max(as->plInitInc, finalInc)) {
                    if (-1 == sqLinkSegments(cursor, cursor->nextSegment, SQ_HIGH_LINKING_PRIORITY)) {
                        diagnostics("Error 3 in sqRunCycle\n");
                        return -1;
                    }
                    cursor = cursor->nextSegment;
                    if (cursor == 0) {
                        diagnostics("Panic: cursor 3 = NULL in sqRunCycle\n");
                        return -1;
                    }

                } else
                    done = 1;
            }

        } else if (sq->stepping == 1) {
            /* we are at the end of the segment, but shouldn't go on
               with the next one */
            sq->numSegments--;
            sq->start = (sq->start + 1) % sq->size;
            if (sq->numSegments < sq->size - SQ_SAFETY_MARGIN)
                sq->full = 0;
            if (sq->numSegments < 0) {
                diagnostics("Panic: sq->numSegments < 0  in sqRunCycle\n");
                return -1;
            }

            as = sq->queue + sq->start;
            as->plInitInc = 0;
            as->active = 0;
            sq->done = 1;
        } else {
            /* end of segment reached */
            sq->numSegments -= 1 + as->numLinkedSegs;
            sq->start = (sq->start + as->numLinkedSegs + 1) % sq->size;
            if (sq->numSegments < sq->size - SQ_SAFETY_MARGIN)
                sq->full = 0;           /* we just removed some segments ... */
            if (sq->numSegments < 0) {
                diagnostics("Panic: sq->numSegments <0  in sqRunCycle\n");
                return -1;
            }
        }
    }

    /* for debugging */

    oldVel = newVel;
    newVel.tran.x = sq->lastPoint.tran.x - oldPos.tran.x;
    newVel.tran.y = sq->lastPoint.tran.y - oldPos.tran.y;
    newVel.tran.z = sq->lastPoint.tran.z - oldPos.tran.z;

    oldPos = sq->lastPoint;

    newAcc.tran.x = newVel.tran.x - oldVel.tran.x;
    newAcc.tran.y = newVel.tran.y - oldVel.tran.y;
    newAcc.tran.z = newVel.tran.z - oldVel.tran.z;

    if (fabs(newAcc.tran.x) > sq->maxAcc * sq->ctPow2 ||
        fabs(newAcc.tran.y) > sq->maxAcc * sq->ctPow2 ||
        fabs(newAcc.tran.z) > sq->maxAcc * sq->ctPow2) {
        diagnostics("MaxAcc limited violated on motion %d\n", sq->currentID);
        diagnostics("ddx=%d ddy=%d ddz=%d\n",
                       (int) (newAcc.tran.x * 1000000.0),
                       (int) (newAcc.tran.y * 1000000.0),
                       (int) (newAcc.tran.z * 1000000.0));
    }

    sq->currentVel = sq->dist - oldDist;
    oldDist = sq->dist;

    return 0;
}

/* function to change the feed overide */
int sqSetFeedOverride(SEGMENTQUEUE * sq, double fo)
{

    SEGMENT *as, *cursor;               /* as = Active Segment */
    double startInc, finalInc, startAcc = 0;
    double prevInc;
    int npow1, npow2, npow3;
    double angleToGo;
    int turn;
    PmCartesian normal, center;
    PmPose start_pose, end_pose;

    if (sq == 0 || sq->queue == 0) {
        diagnostics("Error in sqSetFeedOverride\n");
        return -1;
    }

    /* if fo is out of the valid range: clamp it */
    if (fo < 0)
        fo = 0;
    else if (fo > sq->maxFeedOverrideFactor)
        fo = sq->maxFeedOverrideFactor;

    if (sq->feedOverrideFactor == fo)
        /* don't do anything, just return */
        return 0;

    if (fo == 0) {
        /* then this is actually a pause action */
        if (-1 == sqPause(sq)) {
            diagnostics("Error in sqSetFeedOverride\n");
            return -1;
        } else {
            sq->feedOverrideFactor = 0;
            return 0;
        }
    }

    if (sq->numSegments == 0) {
        /* the queue is empty */
        sq->feedOverrideFactor = fo;
        return 0;
    }

    as = sq->queue + sq->start;

    if (sq->paused == 1) {
        if (sq->feedOverrideFactor == 0) {
            /* If the previous feed override factor equals zero, then the
               pause action was actually done by setting the feed
               override to zero */
            sq->feedOverrideFactor = fo;
            if (-1 == sqResume(sq)) {
                diagnostics("Error in sqSetFeedOverride\n");
                return -1;
            } else
                return 0;
        } else {
            /* else: don't do anything else */
            sq->feedOverrideFactor = fo;
            return 0;
        }
    }

    sq->feedOverrideFactor = fo;
    if (as->active == 0) {
        /* the previous segment has just been finished. as still needs to
           be planned. So: don't do anything */
    } else if (sq->n < as->m + 2 || sq->feedAdjusting == 1) {
        /* we are accelerating to (macInc * 'previous fo'). We need to adjust
           this phase in order get to the desired cruising feed */

        if (sq->feedAdjusting != 1 && as->p == 0
            && as->maxInc * fo > as->cruiseInc)
            /* the the segment is too short to reach the current maxInc and it
               will be too short too to reach the new feed */
            return 0;

        if (sq->feedAdjusting == 1) {
            /* copy the phase 3 params to the phase 1 params, since these will
               be used to find estimates for the current velocity and
               acceleration */
            as->a1 = as->a3;
            as->b1 = as->b3;
            as->c1 = as->c3;
            as->d1 = as->d3;
        }

        finalInc = as->maxInc * fo;

        /* recalculate the last two dist values to find startInc and to use
           for making an estimation of startAcc */
        if (sq->n == 1 || sq->n == 2 || sq->n == 3) {
            startInc = as->plInitInc;
            startAcc = 0;
        } else if (sq->n == 3) {
            npow1 = 1;
            npow2 = 1;
            npow3 = 1;
            startInc = as->a1 * npow3 * sq->ctPow3 +
                as->b1 * npow2 * sq->ctPow2 +
                as->c1 * npow1 * sq->cycleTime + as->d1;
            startAcc = (startInc - as->plInitInc);
        } else {
            npow1 = sq->n - 3;  /* sq->n-2-1 */
            npow2 = npow1 * npow1;
            npow3 = npow2 * npow1;
            startInc = as->a1 * npow3 * sq->ctPow3 +
                as->b1 * npow2 * sq->ctPow2 +
                as->c1 * npow1 * sq->cycleTime + as->d1;
            npow1 = sq->n - 4;  /* sq->n-2-2 */
            npow2 = npow1 * npow1;
            npow3 = npow2 * npow1;
            prevInc = as->a1 * npow3 * sq->ctPow3 +
                as->b1 * npow2 * sq->ctPow2 +
                as->c1 * npow1 * sq->cycleTime + as->d1;
            startAcc = startInc - prevInc;
        }
        as->q = ceil(1.7393877 * fabs(startInc - finalInc) / (sq->maxAcc * sq->ctPow2) +
                     0.5967755904 * fabs(startInc - finalInc) /
                     (sq->maxAcc * sq->maxAcc * sq->ctPow2 * sq->ctPow2) * startAcc -
                     (sq->maxAcc * sq->ctPow2) / 2);
        as->m = 0;
        as->p = 0;
        as->totNumPoints = as->q + 1;
        sq->n = 3;                      /* start all over, but skip the first initInc steps */

        as->b3 = -(3 * (startInc - finalInc) + 2 * as->q * startAcc) / (as->q * as->q * sq->ctPow2);
        as->a3 = (2 * (startInc - finalInc) + as->q * startAcc) / (as->q * as->q * as->q * sq->ctPow3);
        as->c3 = startAcc / sq->cycleTime;
        as->d3 = startInc;

        as->plFinalInc = finalInc;
        /* when the desired feed is reached, some things off this segment
           will have to be recalculated, which is done by runCycle. By setting
           this flag, runCycle will know that it needs to do this
        */
        sq->feedAdjusting = 1;

    } else if (sq->n < as->m + as->p + 2) {

        /* change the active segment such that it looks like we are starting
           with a new segment */

        startInc = as->cruiseInc;

        /* delete all previous segments */
        cursor = as;
        while (cursor != sq->cursor) {
            cursor = cursor->nextSegment;
            if (cursor == 0) {
                diagnostics("Panic 1: cursor = NULL in sqSetFeedOverride\n");
                return -1;
            }
            sq->numSegments--;
        }
        if (sq->numSegments < sq->size - SQ_SAFETY_MARGIN)
            sq->full = 0;
        if (sq->numSegments < 0) {
            diagnostics("Panic: sq->numSegments <0  in sqSetFeedOverride\n");
            return -1;
        }

        cursor->prevSegment = 0;
        cursor->linkedToPrevSeg = 0;
        sq->start = sq->cursor - sq->queue;
        as = cursor;

        as->planningDone = 0;
        as->plInitInc = startInc;
        as->start = sq->lastPoint;
        if (as->type == SQ_LINEAR) {
            as->length = sqGiveLength(as->start, as->end);
            as->totLength = as->length;
            emcpose_to_pmpose(as->start, &start_pose);
            emcpose_to_pmpose(as->end, &end_pose);
            pmLineInit(&as->line, start_pose, end_pose);
        } else {
            angleToGo = as->circle.angle - (sq->dist - sq->offset) / as->helixRadius;
            turn = floor(angleToGo / (2 * PM_PI));
            normal = as->circle.normal;
            center = as->circle.center;
            emcpose_to_pmpose(as->start, &start_pose);
            emcpose_to_pmpose(as->end, &end_pose);
            pmCircleInit(&as->circle, start_pose, end_pose, center, normal, turn);
            as->length = as->circle.angle * as->helixRadius;
            as->totLength = as->length;
        }

        as->active = 0;         /* mark the first segment of the chain
                                   as not active */

        /* determine how many segments are linked to as */
        as->numLinkedSegs = 0;
        while (cursor->nextSegment != 0 && cursor->nextSegment->linkedToPrevSeg == 1) {
            cursor = cursor->nextSegment;
            as->numLinkedSegs++;
            as->totLength += cursor->length;
        }

        /* keep linking successive segments to as until the chain is
           long enough */
        if (-1 == sqForwardLinkSegment(sq, as, sq->feedOverrideFactor)) {
            diagnostics("Error in sqSetFeedOverride\n");
            return -1;
        }
    } else {
        /* we are already decelerating, which is in most cases
           necessary.  Therefore, let's not interrupt this and finish
           the current segment first. The new segment will be planned
           with the new feedOverrideFactor. */
    }

    return 0;
}

/* function to pause the system (decelerate to zero velocity) */
int sqPause(SEGMENTQUEUE * sq)
{
    SEGMENT *as;                        /* as = Active Segment */
    double startInc;            /* the incrediment at the time the pause command
                                   was given */
    double startAcc;            /* the derivate of the inc at that time */
    int npow1, npow2, npow3;
    double amaxTan;

    if (sq == 0 || sq->queue == 0) {
        diagnostics("Error in sqPause\n");
        return -1;
    }

    if (sq->paused == 1)
        /* don't do anything, system is already paused */
        return 0;

    /* set paused flag */
    sq->paused = 1;

    if (sq->numSegments == 0) {
        /* the queue is empty */
        sq->done = 1;           /* propably redundant */
        return 0;
    }

    as = sq->queue + sq->start;

    if (as->active == 0 && as->initInc == 0) {
        /* then this is the very first segment and we have not started yet.
           So let's set the done flag and return */
        sq->done = 1;
        return 0;
    }

    sq->feedAdjusting = 0;      /* if we were adjusting the speed, ignore that too */

    amaxTan = sqGiveMinAmaxTan(as);

    if (as->active == 0 || sq->n <= 3) {
        startInc = as->plInitInc;
        startAcc = 0;
        as->active = 1;
        as->planningDone = 1;   /* the planning will be done in this function */

        as->q = ceil(3 * startInc / (2 * amaxTan * sq->ctPow2));
        as->m = 0;
        as->p = 0;
        as->totNumPoints = as->q + 3;
        sq->n = 3;                      /* start allover, but skip the first initInc steps */

    } else if (sq->n < as->m + 2) {
        /* recalculate the last two dist values */
        npow1 = sq->n - 3;              /* sq->n-2-1 */
        npow2 = npow1 * npow1;
        npow3 = npow2 * npow1;
        startInc = as->a1 * npow3 * sq->ctPow3 +
            as->b1 * npow2 * sq->ctPow2 + as->c1 * npow1 * sq->cycleTime + as->d1;
        npow1 = sq->n - 4;              /* sq->n-2-2 */
        npow2 = npow1 * npow1;
        npow3 = npow2 * npow1;
        startAcc = startInc - (as->a1 * npow3 * sq->ctPow3 + as->b1 * npow2 * sq->ctPow2 +
                               as->c1 * npow1 * sq->cycleTime + as->d1);
        as->q = ceil(2.12 * startInc / (amaxTan * sq->ctPow2));
        as->m = 0;
        as->p = 0;
        as->totNumPoints = as->q + 3;
        sq->n = 3;                      /* start allover, but skip the first initInc steps */
    } else if (sq->n < as->m + as->p + 2) {
        startInc = as->cruiseInc;
        startAcc = 0;
        as->q = ceil(3 * startInc / (2 * amaxTan * sq->ctPow2));
        as->m = 0;
        as->p = 0;
        as->totNumPoints = as->q + 3;
        sq->n = 3;                      /* start allover, but skip the first initInc steps */
    } else {
        /* we are already decelerating, so it would be best to finish that
           first. After we're done with that, we can decelerate to zero */

        /* before messing the whole thing up, let's see if we are already
           decelerating to zero */
        if (as->plFinalInc == 0) {
            /* Then we'll reach the end of the segment with a zero velocity.
               This means that we don't have to do anything but waiting at the
               end of the segment. This is basically the same thing that
               happens at the end of a step-motion. This is exactly what
               we will tell the system: that we're stepping, so that all of
               the extra things needed for a pause within a segment are skipped.
            */
            sq->paused = 0;
            sq->stepping = 1;
            return 0;
        } else {

            /* let's do a little trick: we copy the deceleration parameters
               of the current motion to the acceleration phase parameter
               fields in the current segment and calculate a new
               deceleration action from finalInc to 0. We'll decrease sq->n
               with a number such of steps, such that it will look like
               nothing has happened. sqRunCycle won't notice this, and will
               think it's (again) in phase 1. We know better.... */

            as->a1 = as->a3;
            as->b1 = as->b3;
            as->c1 = as->c3;
            as->d1 = as->d3;

            sq->n -= as->m + as->p;

            as->m = as->q;
            as->p = 1;          /* the cruising phase now becomes the
                                   usual final step of a segment */

            /* find the finalInc */
            startInc = as->plFinalInc;
            startAcc = 0;
            as->cruiseInc = startInc;

            as->q = ceil(3 * startInc / (2 * amaxTan * sq->ctPow2));
            as->totNumPoints = as->m + as->q + as->p + 2;

        }
    }
    as->b3 = -(3 * startInc + 2 * as->q * startAcc) / (as->q * as->q * sq->ctPow2);
    as->a3 = (2 * startInc + as->q * startAcc) / (as->q * as->q * as->q * sq->ctPow3);
    as->c3 = startAcc / sq->cycleTime;
    as->d3 = startInc;

    as->plFinalInc = 0;

    return 0;
}

/* function to resume with a paused segmentqueue */
int sqResume(SEGMENTQUEUE * sq)
{
    SEGMENT *as;                        /* as = Active Segment */
    SEGMENT *cursor;

    if (sq == 0 || sq->queue == 0) {
        diagnostics("Error in sqResume\n");
        return -1;
    }

    if (sq->done != 1) {

        /* we can't resume if the systems is not done yet with a step or pause
           action */
        diagnostics("Can't resume if not done with pause or step action\n");
        /* this is not a critical error, so we'll just ignore the command */
        return 0;
    }

    if (sq->feedOverrideFactor == 0) {
        /* we can't resume if the feed override factor is zero. To resume
           the user should set a new non-zero value for this factor first.
           This will immediately result in a resume action. */
        diagnostics("Can't resume if feed override is zero\n");
        /* not a critical error, so ignore it */
        return 0;
    }

    if (sq->numSegments == 0) {
        /* there's not much to do */
        sq->paused = 0;
        return 0;
    }

    /* let's see if the length of the segment(chain) is large enough to
       accelerate from zero to finalInc */
    as = sq->queue + sq->start;
    cursor = as;
    /* first find the last segment in the chain */
    while (cursor->nextSegment != 0 && cursor->nextSegment->linkedToPrevSeg == 1)
        cursor = cursor->nextSegment;

    /* keep linking successive segments to as until the chain is long
       enough */

    if (-1 == sqForwardLinkSegment(sq, as, sq->feedOverrideFactor)) {
        diagnostics("Error in sqResume \n ");
        return -1;
    }

    if (sq->paused == 1 && sq->stepping == 1)
        sq->stepping = 1;
    else
        sq->stepping = 0;

    sq->paused = 0;
    sq->done = 0;

    return 0;
}

/* function to abort */
int sqAbort(SEGMENTQUEUE * sq)
{
    if (1 == sq->aborting)
        /* we are already aborting, so let's just ignore it */
        return 0;

    if (sq == 0) {
        diagnostics("Error in sqAbort\n");
        return -1;
    }

    if (sq->paused == 1 || sq->done == 1) {
        if (-1 == sqClear(sq)) {
            diagnostics("Error in sqAbort\n");
            return -1;
        }
    } else {
        sq->aborting = 1;
        sqPause(sq);
    }
    return 0;
}

/* function to do execute one motion from a stop and stop again */
int sqStep(SEGMENTQUEUE * sq)
{
    SEGMENT *as;                        /* as = Active Segment */
    if (sq == 0) {
        diagnostics("Error in sqStep\n");
        return -1;
    }

    if (sq->done != 1 || sq->numSegments == 0) {
        /* step should only be used when system is paused and waiting... */
        diagnostics("Stepping can only be done when system is paused and waiting\n");
        /* not a critical error, let's ignore it */
        return 0;;
    }

    if (sq->feedOverrideFactor == 0) {
        /* we can't step if the feed override factor is zero. To step
           the user should set a new non-zero value for this factor first.
           This will immediately result in a resume action. */
        diagnostics("Can't resume if feed override is zero\n");
        /* not a critical error, so ignore it */
        return 0;
    }

    /* make finalInc of the current segment 0, set sq->stepping and resume.
       This means that after this is segment is done, it will
       wait for the next step or resume command */

    as = sq->queue + sq->start;
    as->finalInc = 0;

    /* if the next segment is linked to the current one, unlink it */
    if (as->nextSegment != 0 && as->nextSegment->linkedToPrevSeg == 1) {
        as->nextSegment->linkedToPrevSeg = 0;
        as->nextSegment->numLinkedSegs = as->numLinkedSegs - 1;
        as->nextSegment->totLength = as->totLength - as->length;
        as->numLinkedSegs = 0;
        as->totLength = as->length;
    }
    sq->done = 0;
    sq->stepping = 1;
    sq->paused = 0;
    return 0;
}

int sqIsStepping(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqIsStepping\n");
        return -1;
    }
    return sq->stepping;
}

int sqIsDone(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqIsStepping\n");
        return -1;
    }
    return sq->done;
}

int sqIsPaused(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqIsPaused\n");
        return -1;
    }
    return sq->paused;
}

double sqGetVel(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqGetVel\n");
        return -1;
    }

    return sq->currentVel;
}

double sqGetMaxAcc(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqGetMaxAcc\n");
        return -1;
    }
    return sq->maxAcc;
}

int sqQueueDepth(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqGetDepth\n");
        return -1;
    }
    return sq->numSegments;
}

int sqIsQueueFull(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqIsQueueFull\n");
        return -1;
    }
    return sq->full;
}

int sqGetExecId(SEGMENTQUEUE * sq)
{
    if (sq == 0) {
        diagnostics("Error in sqGetID\n");
        return -1;
    }
    return sq->currentID;
}
