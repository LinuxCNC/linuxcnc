/* Unit tests for the tool frame helpers in kins_util.c.
 *
 * The property worth pinning down is that relating one tool axis convention
 * to the other is a rotation and not a change of sign: negating the third
 * column on its own leaves a reflection, which is not a frame any machine can
 * hold, and it silently loses tool x as well.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "emcpos.h"
#include "kinematics.h"

static int failures;

static void check(int ok, const char *what)
{
    if (!ok) { printf("FAIL: %s\n", what); failures++; }
}

static PmRotationMatrix mat(double xx, double yx, double zx,
                            double xy, double yy, double zy,
                            double xz, double yz, double zz)
{
    /* written out in the layout it prints in, so the literal below reads as
       the matrix it is: columns are tool x, tool y, tool axis */
    PmRotationMatrix m;
    m.x.x = xx; m.y.x = yx; m.z.x = zx;
    m.x.y = xy; m.y.y = yy; m.z.y = zy;
    m.x.z = xz; m.y.z = yz; m.z.z = zz;
    return m;
}

static int same(const PmCartesian *a, double x, double y, double z)
{
    return fabs(a->x - x) < 1e-12
        && fabs(a->y - y) < 1e-12
        && fabs(a->z - z) < 1e-12;
}

/* rotation by 40 degrees about z then 25 about y, an arbitrary proper
   rotation with no zeros to hide a transposition */
static PmRotationMatrix arbitrary(void)
{
    const double a = 40.0 * M_PI / 180.0, b = 25.0 * M_PI / 180.0;
    const double ca = cos(a), sa = sin(a), cb = cos(b), sb = sin(b);
    return mat( ca*cb, -sa,  ca*sb,
                sa*cb,  ca,  sa*sb,
                  -sb,   0,     cb);
}

int main(void)
{
    PmRotationMatrix m, r;

    /* the supplied constants are usable as declarations */
    check(toolFrameIsProper(&TOOL_FRAME_SPINDLE), "TOOL_FRAME_SPINDLE is proper");
    check(toolFrameIsProper(&TOOL_FRAME_FLANGE),  "TOOL_FRAME_FLANGE is proper");

    /* TOOL_FRAME_FLANGE is a half turn about tool x */
    check(same(&TOOL_FRAME_FLANGE.x,  1,  0,  0), "flange keeps tool x");
    check(same(&TOOL_FRAME_FLANGE.y,  0, -1,  0), "flange reverses tool y");
    check(same(&TOOL_FRAME_FLANGE.z,  0,  0, -1), "flange reverses the tool axis");

    /* the mistake this exists to prevent: negating the tool axis alone is a
       reflection, and toolFrameIsProper has to reject it */
    m = TOOL_FRAME_SPINDLE;
    m.z.x = -m.z.x; m.z.y = -m.z.y; m.z.z = -m.z.z;
    check(!toolFrameIsProper(&m), "a negated third column is rejected");

    /* and so are the other ways of not being a rotation */
    m = TOOL_FRAME_SPINDLE; m.x.x = 2.0;
    check(!toolFrameIsProper(&m), "a scaled column is rejected");
    m = TOOL_FRAME_SPINDLE; m.y.x = 0.5;
    check(!toolFrameIsProper(&m), "non-orthogonal columns are rejected");

    /* applying a declared rotation */
    r = arbitrary();
    m = r;
    check(toolFrameApplyNative(&m, &TOOL_FRAME_SPINDLE) == 0, "identity applies");
    check(memcmp(&m, &r, sizeof m) == 0, "identity changes nothing");

    m = r;
    check(toolFrameApplyNative(&m, &TOOL_FRAME_FLANGE) == 0, "flange applies");
    check(toolFrameIsProper(&m), "the result is still a proper rotation");
    check(same(&m.x, r.x.x, r.x.y, r.x.z), "tool x survives the half turn");
    check(same(&m.z, -r.z.x, -r.z.y, -r.z.z), "the tool axis is reversed");
    check(same(&m.y, -r.y.x, -r.y.y, -r.y.z), "tool y is reversed with it");

    /* the half turn is its own inverse */
    check(toolFrameApplyNative(&m, &TOOL_FRAME_FLANGE) == 0, "flange applies again");
    check(memcmp(&m, &r, sizeof m) == 0, "twice is the identity");

    /* the declared rotation is in the module's frame, so it post-multiplies.
       pre-multiplying would give a different answer for a non-commuting pair,
       which is what this catches. */
    m = r;
    toolFrameApplyNative(&m, &TOOL_FRAME_FLANGE);
    check(fabs(m.y.x - (-r.y.x)) < 1e-12, "post-multiplied, not pre-multiplied");

    /* an improper declaration is refused rather than applied */
    m = r;
    r.z.x = -r.z.x; r.z.y = -r.z.y; r.z.z = -r.z.z;   /* reuse r as a bad native */
    check(toolFrameApplyNative(&m, &r) == -1, "an improper declaration is refused");

    /* pumakins' own frame at every joint zero is a half turn about x, so the
       declaration it makes turns it into the identity: the same answer a
       vertical mill gives, which is right, because both point at the work */
    m = mat(1, 0,  0,
            0, -1, 0,
            0, 0, -1);
    check(toolFrameIsProper(&m), "the puma zero pose frame is proper");
    check(toolFrameApplyNative(&m, &TOOL_FRAME_FLANGE) == 0, "puma declaration applies");
    check(same(&m.x, 1, 0, 0) && same(&m.y, 0, 1, 0) && same(&m.z, 0, 0, 1),
          "a puma at zero reports the same frame as a vertical mill");

    /* the two frames are reported against the machine and composed by the
       caller; the product is what a tilted work plane wants, and it is the
       thing that cannot be taken apart again, which is why it is not what
       the module returns */
    {
        PmRotationMatrix work, tool, in_work, back;

        /* nothing turns the work: the tool in work coordinates is the tool */
        work = TOOL_FRAME_SPINDLE;
        tool = arbitrary();
        toolFrameInWork(&work, &tool, &in_work);
        check(memcmp(&in_work, &tool, sizeof in_work) == 0,
              "identity work frame leaves the tool frame alone");

        /* nothing turns the tool: the tool in work coordinates is the inverse
           of the work rotation, so composing it back gives the identity */
        work = arbitrary();
        tool = TOOL_FRAME_SPINDLE;
        toolFrameInWork(&work, &tool, &in_work);
        check(toolFrameIsProper(&in_work), "the composition is a proper rotation");
        toolFrameInWork(&in_work, &TOOL_FRAME_SPINDLE, &back);
        toolFrameInWork(&work, &back, &in_work);
        check(same(&in_work.x, 1, 0, 0) && same(&in_work.y, 0, 1, 0)
              && same(&in_work.z, 0, 0, 1),
              "work composed with its own inverse is the identity");

        /* both turn, which is the case the split exists for: the work frame
           must be transposed, not just multiplied in */
        work = arbitrary();
        tool = TOOL_FRAME_FLANGE;
        toolFrameInWork(&work, &tool, &in_work);
        check(toolFrameIsProper(&in_work), "a mixed rotation composes properly");
        check(fabs(in_work.z.x - (work.x.x*tool.z.x + work.x.y*tool.z.y
                                + work.x.z*tool.z.z)) < 1e-12,
              "the work frame is transposed, not applied directly");
    }

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all tool frame checks passed\n");
    return 0;
}
