#ifndef TP_JSON5_PRINT_H
#define TP_JSON5_PRINT_H

#include "rtapi_json5.h"
#include "posemath.h"
#include "emcpos.h"
#include "tc_types.h"
#include "blendmath_types.h"

#define print_json5_PmCartesian(varname_) print_json5_PmCartesian_(#varname_, varname_)
#define print_json5_PmVector(varname_) print_json5_PmVector_(#varname_, varname_)
#define print_json5_EmcPose(varname_) print_json5_EmcPose_(#varname_, varname_)
#define print_json5_PmCartLine(varname_) print_json5_PmCartLine_( #varname_, varname_)
#define print_json5_PmCircle(varname_) print_json5_PmCircle_(#varname_, varname_)

#define print_json5_PmCartesian_field(base_, varname_) print_json5_PmCartesian_(#varname_, (base_)->varname_)
#define print_json5_PmVector_field(base_, varname_) print_json5_PmVector_(#varname_, (base_)->varname_)
#define print_json5_EmcPose_field(base_, varname_) print_json5_EmcPose_(#varname_, (base_)->varname_)
#define print_json5_PmCartLine_field(base_, varname_) print_json5_PmCartLine_( #varname_, (base_)->varname_)
#define print_json5_PmCircle_field(base_, varname_) print_json5_PmCircle_(#varname_, (base_)->varname_)


static inline void print_json5_tc_id_data_(TC_STRUCT const * const tc)
{
    print_json5_int_("src_line", tc->tag.fields[GM_FIELD_LINE_NUMBER]);
    print_json5_int_field(tc, id);
    print_json5_long_long_field(tc, unique_id);
}


static inline void print_json5_PmCartesian_(const char *varname, PmCartesian value)
{

    rtapi_print("%s: [%0.17g, %0.17g, %0.17g], ", varname ?: "NO_NAME", value.x, value.y, value.z);
}

static inline void print_json5_PmVector_(const char *varname, PmVector value)
{
    rtapi_print("%s: [%0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g], ",
                    varname,
                    value.ax[0],
                    value.ax[1],
                    value.ax[2],
                    value.ax[3],
                    value.ax[4],
                    value.ax[5],
                    value.ax[6],
                    value.ax[7],
                    value.ax[8]
                );
}

static inline void print_json5_EmcPose_(const char *varname, EmcPose value)
{
    rtapi_print("%s: [%0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g, %0.17g], ",
                    varname,
                    value.tran.x,
                    value.tran.y,
                    value.tran.z,
                    value.a,
                    value.b,
                    value.c,
                    value.u,
                    value.v,
                    value.w);
}

static inline void print_json5_PmCartLine_(const char *name, PmCartLine value)
{
    print_json5_object_start_(name);
    // Need manual field names here
    print_json5_PmCartesian_("start", value.start);
    print_json5_PmCartesian_("end", value.start);
    print_json5_PmCartesian_("uVec", value.uVec);
    print_json5_double_("tmag", value.tmag);
    print_json5_double_("tmag_zero", value.tmag_zero);
    print_json5_object_end_();
}

static inline void print_json5_PmCircle_(const char *name, PmCircle circle)
{
    print_json5_object_start_(name);
    print_json5_PmCartesian_("center", circle.center);
    print_json5_PmCartesian_("normal", circle.normal);
    print_json5_PmCartesian_("rTan", circle.rTan);
    print_json5_PmCartesian_("rPerp", circle.rPerp);
    print_json5_PmCartesian_("rHelix", circle.rHelix);
    print_json5_double_("radius", circle.radius);
    print_json5_double_("angle", circle.angle);
    print_json5_double_("spiral", circle.spiral);
    print_json5_object_end_();
}

static inline void print_json5_PmLine9_(const char *name, PmLine9 value)
{
    //TODO
}

static inline void print_json5_PmCircle9_(const char *name, PmCircle9 value)
{
    //TODO
}

static inline void print_json5_SpiralArcLengthFit_(const char *fname, SpiralArcLengthFit fit)
{
    print_json5_object_start_(fname);
    print_json5_double_("b0", fit.b0);
    print_json5_double_("b1", fit.b1);
    print_json5_double_("total_planar_length", fit.total_planar_length);
    print_json5_long_("spiral_in", fit.spiral_in);
    print_json5_object_end_();
}

static inline void print_json5_blend_boundary_t(const char *fname, blend_boundary_t const * const r)
{
    print_json5_object_start_(fname);
    print_json5_PmVector_field(r, P1);
    print_json5_PmVector_field(r, P2);
    print_json5_PmVector_field(r, u1);
    print_json5_PmVector_field(r, u2);
    print_json5_double_field(r, s1);
    print_json5_double_field(r, s2);
    print_json5_object_end_();
}

static inline void print_json5_biarc_control_points_t(const char *fname, biarc_control_points_t const * const c)
{
    print_json5_object_start_(fname);
    print_json5_PmVector_field(c, Pb1);
    print_json5_PmVector_field(c, Pb2);
    print_json5_PmVector_field(c, u_mid);
    print_json5_PmVector_field(c, P_mid);
    print_json5_double_field(c, d);
    print_json5_object_end_();
}

static inline void print_json5_biarc_solution_errors_t(const char *fname, biarc_solution_errors_t const * const r)
{
    print_json5_object_start_(fname);
    print_json5_double_field(r, deviation);
    print_json5_double_field(r, radius_rel);
    print_json5_double_field(r, radius_abs);
    print_json5_object_end_();
}

static inline void print_json5_biarc_solution_t(const char *fname, biarc_solution_t const * const r)
{
    print_json5_object_start_(fname);
    print_json5_double_field(r, Rb);
    print_json5_double_field(r, R_geom);
    print_json5_double_field(r, arc_len_est);
    print_json5_double_field(r, R_plan);
    print_json5_double_field(r, T_plan);
    print_json5_biarc_solution_errors_t("err", &r->err);
    print_json5_object_end_();
}

static inline void print_json5_SphericalArc9(const char *fname, SphericalArc9 const * const arc) {
    print_json5_object_start_(fname);
    print_json5_PmVector_field(arc, start);
    print_json5_PmVector_field(arc, end);
    print_json5_PmVector_field(arc, center);
    print_json5_PmVector_field(arc, rStart);
    print_json5_PmVector_field(arc, rEnd);
    print_json5_PmVector_field(arc, uTan);
    print_json5_double_field(arc, radius);
    print_json5_double_field(arc, spiral);
    print_json5_double_field(arc, angle);
    print_json5_double_field(arc, Sangle);
    print_json5_double_field(arc, line_length);
    print_json5_object_end_();
}

static inline void print_json5_TC_STRUCT_kinematics(const char *fname, TC_STRUCT const * const tc)
{
    print_json5_object_start_(fname);
    print_json5_double_("maxvel", tc->maxvel_geom);
    print_json5_double_field(tc, maxaccel);
    print_json5_double_field(tc, acc_normal_max);
    print_json5_double_field(tc, acc_ratio_tan);
    print_json5_double_field(tc, reqvel);
    print_json5_double_field(tc, target);
    print_json5_int_field(tc, motion_type);
    print_json5_object_end_();
}

static inline void print_json5_SpiralArcLengthFit(
    const char *fname,
    SpiralArcLengthFit const * const fit,
    double min_radius,
    double total_angle,
    double spiral_coef)
{
    print_json5_object_start_(fname);
    print_json5_double(min_radius);
    print_json5_double(total_angle);
    print_json5_double(spiral_coef);
    print_json5_double_("b0", fit->b0);
    print_json5_double_("b1", fit->b1);
    print_json5_double_("total_planar_length", fit->total_planar_length);
    print_json5_bool_("spiral_in", fit->spiral_in);
    print_json5_object_end_();
}


#endif // TP_JSON5_PRINT_H
