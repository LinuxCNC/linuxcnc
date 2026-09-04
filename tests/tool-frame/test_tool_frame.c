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

#define DEG (M_PI/180.0)
#define NUTATION 45.0

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


/* ------------------------------------------------------------------
 * Machine models for the tool orientation inverse.
 *
 * These are the frame functions a module supplies, written out here so the
 * solver can be exercised without loading one.  Rotary joints are in degrees,
 * as every module in the tree takes them, except radMachine, which is in
 * radians to prove the solver does not assume.
 * ------------------------------------------------------------------ */

static PmRotationMatrix rows(const double m[3][3])
{
    return mat(m[0][0], m[0][1], m[0][2],
               m[1][0], m[1][1], m[1][2],
               m[2][0], m[2][1], m[2][2]);
}

static PmRotationMatrix rot_z(double rad)
{
    const double c = cos(rad), s = sin(rad);
    const double m[3][3] = {{c, -s, 0}, {s, c, 0}, {0, 0, 1}};
    return rows(m);
}

/* the nutating secondary joint of the trsrn heads */
static PmRotationMatrix rot_nutate(double rad)
{
    const double v = NUTATION*DEG, sv = sin(v), cv = cos(v);
    const double ss = sin(rad), cs = cos(rad);
    const double r = cs + sv*sv*(1 - cs);
    const double q = cs + cv*cv*(1 - cs);
    const double t = sv*cv*(1 - cs);
    const double m[3][3] = {{   cs, -cv*ss, sv*ss},
                            {cv*ss,      r,     t},
                            {-sv*ss,     t,     q}};
    return rows(m);
}

static PmRotationMatrix product(const PmRotationMatrix *a,
                                const PmRotationMatrix *b)
{
    const double x[3][3] = {{a->x.x, a->y.x, a->z.x},
                            {a->x.y, a->y.y, a->z.y},
                            {a->x.z, a->y.z, a->z.z}};
    const double y[3][3] = {{b->x.x, b->y.x, b->z.x},
                            {b->x.y, b->y.y, b->z.y},
                            {b->x.z, b->y.z, b->z.z}};
    double m[3][3];
    int i, j, k;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            m[i][j] = 0;
            for (k = 0; k < 3; k++) { m[i][j] += x[i][k]*y[k][j]; }
        }
    }
    return rows(m);
}

static int identityFrame(const double *j, PmRotationMatrix *rot,
                         const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)j; (void)fflags;
    *rot = TOOL_FRAME_SPINDLE;
    return 0;
}

/* xyzac: both rotaries carry the table, the tool stays square with the
   machine.  j[3] is A, j[4] is C. */
static int xyzacWork(const double *j, PmRotationMatrix *rot,
                     const KINEMATICS_FORWARD_FLAGS *fflags)
{
    const double a = j[3]*DEG, c = j[4]*DEG;
    const double m[3][3] = {{      cos(c),        sin(c),      0},
                            {-sin(c)*cos(a), cos(c)*cos(a), sin(a)},
                            { sin(c)*sin(a),-cos(c)*sin(a), cos(a)}};
    (void)fflags;
    *rot = rows(m);
    return 0;
}

/* a nutating spindle head with nothing turning the work.  j[3] is the
   nutating secondary joint, j[4] the primary about z. */
static int headTool(const double *j, PmRotationMatrix *rot,
                    const KINEMATICS_FORWARD_FLAGS *fflags)
{
    PmRotationMatrix p = rot_z(j[4]*DEG), s = rot_nutate(j[3]*DEG);
    (void)fflags;
    *rot = product(&p, &s);
    return 0;
}

/* the same head, in radians, to exercise the period discovery */
static int radTool(const double *j, PmRotationMatrix *rot,
                   const KINEMATICS_FORWARD_FLAGS *fflags)
{
    PmRotationMatrix p = rot_z(j[4]), s = rot_nutate(j[3]);
    (void)fflags;
    *rot = product(&p, &s);
    return 0;
}

/* a table rotary and a nutating head at once, so three joints turn the tool
   and a bare tool axis leaves one of them free.  j[3] is the table A, j[4]
   the nutating joint, j[5] the head primary. */
static int mixedWork(const double *j, PmRotationMatrix *rot,
                     const KINEMATICS_FORWARD_FLAGS *fflags)
{
    const double a = j[3]*DEG;
    const double m[3][3] = {{1,       0,      0},
                            {0,  cos(a), sin(a)},
                            {0, -sin(a), cos(a)}};
    (void)fflags;
    *rot = rows(m);
    return 0;
}

static int mixedTool(const double *j, PmRotationMatrix *rot,
                     const KINEMATICS_FORWARD_FLAGS *fflags)
{
    PmRotationMatrix p = rot_z(j[5]*DEG), s = rot_nutate(j[4]*DEG);
    (void)fflags;
    *rot = product(&p, &s);
    return 0;
}

/* what the module would report: the tool frame in workpiece coordinates */
static PmRotationMatrix in_work(kinsFrameFunc work, kinsFrameFunc tool,
                                const double *j)
{
    KINEMATICS_FORWARD_FLAGS f = 0;
    PmRotationMatrix w, t, out;

    work(j, &w, &f);
    tool(j, &t, &f);
    toolFrameInWork(&w, &t, &out);
    return out;
}

static int axis_matches(kinsFrameFunc work, kinsFrameFunc tool,
                        const double *j, const PmCartesian *want)
{
    PmRotationMatrix m = in_work(work, tool, j);
    return fabs(m.z.x - want->x) < 1e-9
        && fabs(m.z.y - want->y) < 1e-9
        && fabs(m.z.z - want->z) < 1e-9;
}

/* does the list hold a solution whose free joints are these, to a degree */
static int holds(const double *sols, int count, int njoints,
                 const int *which, const double *value, int n)
{
    int s, i, ok;

    for (s = 0; s < count; s++) {
        ok = 1;
        for (i = 0; i < n; i++) {
            if (fabs(sols[s*njoints + which[i]] - value[i]) > 1e-6) { ok = 0; }
        }
        if (ok) { return 1; }
    }
    return 0;
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


    /* ------------------------------------------------------------------
     * The tool orientation inverse.
     * ------------------------------------------------------------------ */
    {
        double seed[6]  = {10, 20, 30, 10, 5, 0};
        double truth[6] = {10, 20, 30, 34.4, 68.8, 0};
        double sols[TOOL_FRAME_MAX_SOLUTIONS*6];
        int free_dirs[TOOL_FRAME_MAX_SOLUTIONS];
        double spin[TOOL_FRAME_MAX_SOLUTIONS];
        PmRotationMatrix want;
        PmCartesian axis, xdir;
        int n, i;

        /* a table rotary machine, tool axis only: the two rotaries pin it
           down, and there are two ways to get there */
        want = in_work(xyzacWork, identityFrame, truth);
        axis = want.z;
        n = toolFrameSolve(xyzacWork, identityFrame, 5, &axis, NULL, seed, 0,
                           sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
        check(n == 2, "xyzac reports both ways to reach a tool axis");
        for (i = 0; i < n; i++) {
            check(axis_matches(xyzacWork, identityFrame, sols + i*5, &axis),
                  "every xyzac solution reaches the requested axis");
            check(free_dirs[i] == 0, "an xyzac solution is pinned down");
            check(sols[i*5 + 0] == seed[0] && sols[i*5 + 1] == seed[1]
                  && sols[i*5 + 2] == seed[2],
                  "the joints that do not turn the tool are copied from the seed");
        }
        {
            const int which[2] = {3, 4};
            const double value[2] = {34.4, 68.8};
            check(holds(sols, n, 5, which, value, 2),
                  "the pose the request was built from is one of them");
        }

        /* the singular pose: the tool axis is the axis the primary turns
           about, so the primary is free and the answer is a family */
        axis.x = 0; axis.y = 0; axis.z = 1;
        n = toolFrameSolve(xyzacWork, identityFrame, 5, &axis, NULL, seed, 0,
                           sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
        check(n == 1, "a singular pose reports one representative, not a sample");
        check(free_dirs[0] == 1, "and says one direction is free");
        check(fabs(sols[3]) < 1e-6, "the joint the request does pin down is set");
        check(fabs(sols[4] - seed[4]) < 1e-6,
              "the free joint is left where the machine already is");

        /* approaching the singularity: the two solutions stay two until the
           spin about the tool stops being worth anything, and then the answer
           becomes the family rather than a scatter of points that differ by
           more than the tool can tell apart */
        axis.x = sin(0.01*DEG); axis.y = 0; axis.z = cos(0.01*DEG);
        n = toolFrameSolve(xyzacWork, identityFrame, 5, &axis, NULL, seed, 0,
                           sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
        check(n == 2 && free_dirs[0] == 0 && free_dirs[1] == 0,
              "a hundredth of a degree off the pole still has two solutions");

        axis.x = sin(0.001*DEG); axis.y = 0; axis.z = cos(0.001*DEG);
        n = toolFrameSolve(xyzacWork, identityFrame, 5, &axis, NULL, seed, 0,
                           sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
        check(n == 1 && free_dirs[0] == 1,
              "a thousandth of a degree off it, the spin is free in practice");

        /* xyzac turns the work through a full sphere, so straight down is a
           pose and not a refusal: A at half a turn */
        axis.x = 0; axis.y = 0; axis.z = -1;
        n = toolFrameSolve(xyzacWork, identityFrame, 5, &axis, NULL, seed, 0,
                           sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
        check(n == 1 && free_dirs[0] == 1,
              "the other pole is reachable, and free about the tool as well");
        check(fabs(fabs(sols[3]) - 180.0) < 1e-6, "reached with A at half a turn");

        /* a machine where nothing turns the tool answers for the one pose it
           has, and refuses anything else */
        axis.x = 0; axis.y = 0; axis.z = 1;
        n = toolFrameSolve(identityFrame, identityFrame, 5, &axis, NULL, seed, 0,
                           sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
        check(n == 1 && free_dirs[0] == 0,
              "a machine with no orientation joints reports its one pose");
        axis.x = 0; axis.y = 1; axis.z = 0;
        n = toolFrameSolve(identityFrame, identityFrame, 5, &axis, NULL, seed, 0,
                           sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
        check(n == 0, "and cannot reach any other");

        /* out of reach: a nutating head sweeps a cone, and with a nutation
           of 45 degrees it cannot get the tool below the horizontal */
        {
            double head_seed[5] = {0, 0, 0, 10, 5};

            axis.x = 0; axis.y = 0; axis.z = -1;
            n = toolFrameSolve(identityFrame, headTool, 5, &axis, NULL,
                               head_seed, 0, sols, TOOL_FRAME_MAX_SOLUTIONS,
                               free_dirs, spin);
            check(n == 0, "an unreachable axis reports no solutions");
        }

        /* the nutating head, checked against the closed form the TWP remap
           uses: cos(secondary) = (Kzz - Cv^2)/(1 - Cv^2), which has the two
           roots +theta and -theta */
        {
            double head_seed[5]  = {0, 0, 0, 10, 5};
            double head_truth[5] = {0, 0, 0, 40.0, 25.0};
            const double cv = cos(NUTATION*DEG);
            double closed, s;

            want = in_work(identityFrame, headTool, head_truth);
            axis = want.z;
            closed = acos((axis.z - cv*cv)/(1 - cv*cv))/DEG;

            n = toolFrameSolve(identityFrame, headTool, 5, &axis, NULL,
                               head_seed, 0, sols, TOOL_FRAME_MAX_SOLUTIONS,
                               free_dirs, spin);
            check(n == 2, "the nutating head reports both secondary roots");
            for (i = 0; i < n; i++) {
                s = fabs(sols[i*5 + 3]);
                check(fabs(s - closed) < 1e-6,
                      "the search agrees with the closed form of the remap");
                check(axis_matches(identityFrame, headTool, sols + i*5, &axis),
                      "every nutating solution reaches the requested axis");
            }
            check(fabs(sols[0*5 + 3] + sols[1*5 + 3]) < 1e-6,
                  "the two roots are opposite, as acos gives them");

            /* the same machine written in radians: the joint unit is
               discovered, so the answer is the same shape */
            head_seed[3] = 10*DEG; head_seed[4] = 5*DEG;
            n = toolFrameSolve(identityFrame, radTool, 5, &axis, NULL,
                               head_seed, 0, sols, TOOL_FRAME_MAX_SOLUTIONS,
                               free_dirs, spin);
            check(n == 2, "a module taking radians is solved too");
            for (i = 0; i < n; i++) {
                check(fabs(fabs(sols[i*5 + 3])/DEG - closed) < 1e-6,
                      "and gives the same angles once the unit is accounted for");
            }
        }

        /* three joints turn the tool.  A bare tool axis leaves the spin about
           it free, and asking for tool x as well pins the machine down. */
        {
            double mix_seed[6]  = {0, 0, 0, 5, 10, 15};
            double mix_truth[6] = {0, 0, 0, 20, 45.8, 57.3};

            want = in_work(mixedWork, mixedTool, mix_truth);
            axis = want.z;
            xdir = want.x;

            n = toolFrameSolve(mixedWork, mixedTool, 6, &axis, NULL, mix_seed, 0,
                               sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
            check(n == 1, "a spare orientation joint gives a family, not a list");
            check(free_dirs[0] == 1, "and one free direction is reported");
            check(axis_matches(mixedWork, mixedTool, sols, &axis),
                  "the representative reaches the requested axis");

            for (i = 0; i < TOOL_FRAME_MAX_SOLUTIONS; i++) { spin[i] = 99; }
            n = toolFrameSolve(mixedWork, mixedTool, 6, &axis, &xdir, mix_seed, 0,
                               sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
            check(n == 2, "asking for tool x as well pins it down");
            for (i = 0; i < n; i++) {
                PmRotationMatrix got = in_work(mixedWork, mixedTool, sols + i*6);
                check(free_dirs[i] == 0, "with nothing left free");
                check(spin[i] == 0.0,
                      "and no turn about the tool left over, the joints did it");
                check(fabs(got.x.x - xdir.x) < 1e-9
                      && fabs(got.x.y - xdir.y) < 1e-9
                      && fabs(got.x.z - xdir.z) < 1e-9,
                      "and tool x where it was asked for");
            }
            {
                const int which[3] = {3, 4, 5};
                const double value[3] = {20, 45.8, 57.3};
                check(holds(sols, n, 6, which, value, 3),
                      "the pose the request was built from is one of them");
            }

            /* the same machine with the table held, which is what a tilted
               work plane that leaves the table alone asks: the head alone
               reaches the axis two ways, and tool x is then a turn about the
               tool rather than a table move */
            n = toolFrameSolve(mixedWork, mixedTool, 6, &axis, NULL, mix_seed,
                               1u << 3, sols, TOOL_FRAME_MAX_SOLUTIONS,
                               free_dirs, spin);
            check(n == 2, "holding the table leaves the two head solutions");
            for (i = 0; i < n; i++) {
                check(free_dirs[i] == 0, "with nothing left free");
                check(sols[i*6 + 3] == mix_seed[3], "and the table where it was");
                check(axis_matches(mixedWork, mixedTool, sols + i*6, &axis),
                      "every held-table solution reaches the requested axis");
            }

            n = toolFrameSolve(mixedWork, mixedTool, 6, &axis, &xdir, mix_seed,
                               1u << 3, sols, TOOL_FRAME_MAX_SOLUTIONS,
                               free_dirs, spin);
            check(n == 2, "tool x with the table held is still reached both ways");
            for (i = 0; i < n; i++) {
                PmRotationMatrix got = in_work(mixedWork, mixedTool, sols + i*6);
                double c_s = cos(spin[i]), s_s = sin(spin[i]);
                check(sols[i*6 + 3] == mix_seed[3], "the table is still where it was");
                check(spin[i] != 0.0, "so the turn about the tool is not zero");
                check(fabs(c_s*got.x.x + s_s*got.y.x - xdir.x) < 1e-9
                      && fabs(c_s*got.x.y + s_s*got.y.y - xdir.y) < 1e-9
                      && fabs(c_s*got.x.z + s_s*got.y.z - xdir.z) < 1e-9,
                      "and it places tool x");
            }

            /* holding every orientation joint leaves the one pose */
            n = toolFrameSolve(mixedWork, mixedTool, 6, &axis, NULL, mix_truth,
                               (1u << 3) | (1u << 4) | (1u << 5), sols,
                               TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
            check(n == 1 && free_dirs[0] == 0,
                  "with everything held, the seed answers if it reaches the axis");
            n = toolFrameSolve(mixedWork, mixedTool, 6, &axis, NULL, mix_seed,
                               (1u << 3) | (1u << 4) | (1u << 5), sols,
                               TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin);
            check(n == 0, "and refuses if it does not");
        }


        /* Asking a five axis machine for tool x as well.  Its two rotaries are
           spent on the tool axis and the turn about that axis is not a joint,
           so the answer is the poses that reach the axis plus the turn that
           places tool x, which is what the virtual rotation applies. */
        {
            double head_seed[5]  = {0, 0, 0, 10, 5};
            double head_truth[5] = {0, 0, 0, 40.0, 25.0};
            PmRotationMatrix got, want_frame;
            PmCartesian want_x;
            double c_s, s_s, dot;

            want_frame = in_work(identityFrame, headTool, head_truth);
            axis = want_frame.z;

            /* a tool x at right angles to that axis, but not the one this
               machine happens to produce: turn the achieved one by 30 degrees
               about the axis */
            c_s = cos(30*DEG); s_s = sin(30*DEG);
            want_x.x = c_s*want_frame.x.x + s_s*want_frame.y.x;
            want_x.y = c_s*want_frame.x.y + s_s*want_frame.y.y;
            want_x.z = c_s*want_frame.x.z + s_s*want_frame.y.z;

            n = toolFrameSolve(identityFrame, headTool, 5, &axis, &want_x,
                               head_seed, 0, sols, TOOL_FRAME_MAX_SOLUTIONS,
                               free_dirs, spin);
            check(n == 2, "the axis is still reached both ways");
            for (i = 0; i < n; i++) {
                check(axis_matches(identityFrame, headTool, sols + i*5, &axis),
                      "every solution reaches the requested axis");
                got = in_work(identityFrame, headTool, sols + i*5);
                /* turning the achieved frame by the reported spin has to land
                   tool x where it was asked for */
                c_s = cos(spin[i]); s_s = sin(spin[i]);
                check(fabs(c_s*got.x.x + s_s*got.y.x - want_x.x) < 1e-9
                      && fabs(c_s*got.x.y + s_s*got.y.y - want_x.y) < 1e-9
                      && fabs(c_s*got.x.z + s_s*got.y.z - want_x.z) < 1e-9,
                      "and the reported turn places tool x");
            }

            /* with nowhere to report the turn, the request cannot be answered
               rather than being answered wrongly */
            n = toolFrameSolve(identityFrame, headTool, 5, &axis, &want_x,
                               head_seed, 0, sols, TOOL_FRAME_MAX_SOLUTIONS,
                               free_dirs, NULL);
            check(n == 0, "and without somewhere to put it, no solutions");

            /* the two vectors are two axes of one frame */
            dot = 0.5;
            want_x.x = axis.x + dot; want_x.y = axis.y; want_x.z = axis.z;
            check(toolFrameSolve(identityFrame, headTool, 5, &axis, &want_x,
                                 head_seed, 0, sols, TOOL_FRAME_MAX_SOLUTIONS,
                                 free_dirs, spin) == -1,
                  "a tool x not at right angles to the axis is refused");
        }

        /* the arguments are checked rather than trusted */
        axis.x = 0; axis.y = 0; axis.z = 1;
        check(toolFrameSolve(NULL, identityFrame, 5, &axis, NULL, seed, 0, sols,
                             TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin) == -1,
              "a missing frame function is refused");
        check(toolFrameSolve(xyzacWork, identityFrame, 0, &axis, NULL, seed, 0,
                             sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin) == -1,
              "a bogus joint count is refused");
        check(toolFrameSolve(xyzacWork, identityFrame, 5, NULL, NULL, seed, 0,
                             sols, TOOL_FRAME_MAX_SOLUTIONS, free_dirs, spin) == -1,
              "a missing target is refused");
    }

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("all tool frame checks passed\n");
    return 0;
}
