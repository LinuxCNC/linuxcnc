/********************************************************************
* Description: canon_interface.hh
*   C++ wrapper class around the canon callback table.
*
*   Hides the ctx pointer, handles type conversions between the
*   C++ types used by the interpreter (EmcPose, enums, StateTag,
*   std::vector<CONTROL_POINT>) and the flat C types in the
*   generated canon_api.h callback table.
*
*   Each interpreter instance holds its own CanonInterface,
*   making multi-instance operation natural.
*
* License: GPL Version 2
********************************************************************/
#ifndef CANON_INTERFACE_HH
#define CANON_INTERFACE_HH

#define CANON_API_CGO  // skip gomc_api.h registry helpers
#include "gomc/generated/gmi/canon/canon_api.h"
#undef CANON_API_CGO

#include "canon.hh"       // EmcPose, CANON_TOOL_TABLE, enums, StateTag, CONTROL_POINT
#include <cstdint>
#include <cstdio>          // snprintf
#include <vector>

class CanonInterface {
    const canon_callbacks_t *cb;

public:
    CanonInterface() : cb(nullptr) {}
    explicit CanonInterface(const canon_callbacks_t *callbacks) : cb(callbacks) {}

    bool valid() const { return cb != nullptr; }

    // ---- Simple pass-through methods ----
    // Generated from canon_api.h typedefs. Each method forwards to
    // cb->func(cb->ctx, ...) hiding the ctx plumbing.

    void init_canon() { cb->init_canon(cb->ctx); }
    void set_g5x_offset(int32_t origin, double x, double y, double z, double a, double b, double c, double u, double v, double w) { cb->set_g5x_offset(cb->ctx, origin, x, y, z, a, b, c, u, v, w); }
    void set_g92_offset(double x, double y, double z, double a, double b, double c, double u, double v, double w) { cb->set_g92_offset(cb->ctx, x, y, z, a, b, c, u, v, w); }
    void set_xy_rotation(double t) { cb->set_xy_rotation(cb->ctx, t); }
    void update_end_point(double x, double y, double z, double a, double b, double c, double u, double v, double w) { cb->update_end_point(cb->ctx, x, y, z, a, b, c, u, v, w); }
    void set_traverse_rate(double rate) { cb->set_traverse_rate(cb->ctx, rate); }
    void straight_traverse(int32_t lineno, double x, double y, double z, double a, double b, double c, double u, double v, double w) { cb->straight_traverse(cb->ctx, lineno, x, y, z, a, b, c, u, v, w); }
    void set_feed_rate(double rate) { cb->set_feed_rate(cb->ctx, rate); }
    void set_feed_mode(int32_t spindle, int32_t mode) { cb->set_feed_mode(cb->ctx, spindle, mode); }
    void set_naivecam_tolerance(double tolerance) { cb->set_naivecam_tolerance(cb->ctx, tolerance); }
    void set_cutter_radius_compensation(double radius) { cb->set_cutter_radius_compensation(cb->ctx, radius); }
    void stop_cutter_radius_compensation() { cb->stop_cutter_radius_compensation(cb->ctx); }
    void start_speed_feed_synch(int32_t spindle, double feed_per_revolution, int32_t velocity_mode) { cb->start_speed_feed_synch(cb->ctx, spindle, feed_per_revolution, velocity_mode); }
    void stop_speed_feed_synch() { cb->stop_speed_feed_synch(cb->ctx); }
    void arc_feed(int32_t lineno, double first_end, double second_end, double first_axis, double second_axis, int32_t rotation, double axis_end_point, double a, double b, double c, double u, double v, double w) { cb->arc_feed(cb->ctx, lineno, first_end, second_end, first_axis, second_axis, rotation, axis_end_point, a, b, c, u, v, w); }
    void straight_feed(int32_t lineno, double x, double y, double z, double a, double b, double c, double u, double v, double w) { cb->straight_feed(cb->ctx, lineno, x, y, z, a, b, c, u, v, w); }
    void rigid_tap(int32_t lineno, double x, double y, double z, double scale) { cb->rigid_tap(cb->ctx, lineno, x, y, z, scale); }
    void straight_probe(int32_t lineno, double x, double y, double z, double a, double b, double c, double u, double v, double w, uint8_t probe_type) { cb->straight_probe(cb->ctx, lineno, x, y, z, a, b, c, u, v, w, probe_type); }
    void stop() { cb->stop(cb->ctx); }
    void dwell(double seconds) { cb->dwell(cb->ctx, seconds); }
    void finish() { cb->finish(cb->ctx); }
    void set_spindle_mode(int32_t spindle, double mode) { cb->set_spindle_mode(cb->ctx, spindle, mode); }
    void set_spindle_speed(int32_t spindle, double rpm) { cb->set_spindle_speed(cb->ctx, spindle, rpm); }
    void stop_spindle_turning(int32_t spindle) { cb->stop_spindle_turning(cb->ctx, spindle); }
    void orient_spindle(int32_t spindle, double orientation, int32_t mode) { cb->orient_spindle(cb->ctx, spindle, orientation, mode); }
    void wait_spindle_orient_complete(int32_t spindle, double timeout) { cb->wait_spindle_orient_complete(cb->ctx, spindle, timeout); }
    void select_tool(int32_t tool) { cb->select_tool(cb->ctx, tool); }
    void start_change() { cb->start_change(cb->ctx); }
    void change_tool(int32_t slot) { cb->change_tool(cb->ctx, slot); }
    void change_tool_number(int32_t number) { cb->change_tool_number(cb->ctx, number); }
    void reload_tooldata() { cb->reload_tooldata(cb->ctx); }
    void flood_on() { cb->flood_on(cb->ctx); }
    void flood_off() { cb->flood_off(cb->ctx); }
    void mist_on() { cb->mist_on(cb->ctx); }
    void mist_off() { cb->mist_off(cb->ctx); }
    void enable_feed_override() { cb->enable_feed_override(cb->ctx); }
    void disable_feed_override() { cb->disable_feed_override(cb->ctx); }
    void enable_speed_override(int32_t spindle) { cb->enable_speed_override(cb->ctx, spindle); }
    void disable_speed_override(int32_t spindle) { cb->disable_speed_override(cb->ctx, spindle); }
    void enable_feed_hold() { cb->enable_feed_hold(cb->ctx); }
    void disable_feed_hold() { cb->disable_feed_hold(cb->ctx); }
    void enable_adaptive_feed() { cb->enable_adaptive_feed(cb->ctx); }
    void disable_adaptive_feed() { cb->disable_adaptive_feed(cb->ctx); }
    void set_motion_output_bit(int32_t index) { cb->set_motion_output_bit(cb->ctx, index); }
    void clear_motion_output_bit(int32_t index) { cb->clear_motion_output_bit(cb->ctx, index); }
    void set_aux_output_bit(int32_t index) { cb->set_aux_output_bit(cb->ctx, index); }
    void clear_aux_output_bit(int32_t index) { cb->clear_aux_output_bit(cb->ctx, index); }
    void set_motion_output_value(int32_t index, double value) { cb->set_motion_output_value(cb->ctx, index, value); }
    void set_aux_output_value(int32_t index, double value) { cb->set_aux_output_value(cb->ctx, index, value); }
    int32_t wait_input(int32_t index, int32_t input_type, int32_t wait_type, double timeout) { return cb->wait_input(cb->ctx, index, input_type, wait_type, timeout); }
    void clamp_axis(int32_t axis) { cb->clamp_axis(cb->ctx, axis); }
    void unclamp_axis(int32_t axis) { cb->unclamp_axis(cb->ctx, axis); }
    int32_t lock_rotary(int32_t lineno, int32_t joint) { return cb->lock_rotary(cb->ctx, lineno, joint); }
    int32_t unlock_rotary(int32_t lineno, int32_t joint) { return cb->unlock_rotary(cb->ctx, lineno, joint); }
    void program_stop() { cb->program_stop(cb->ctx); }
    void optional_program_stop() { cb->optional_program_stop(cb->ctx); }
    void program_end() { cb->program_end(cb->ctx); }
    void pallet_shuttle() { cb->pallet_shuttle(cb->ctx); }
    void comment(const char *s) { cb->comment(cb->ctx, s); }
    void message(const char *s) { cb->message(cb->ctx, s); }
    void log_msg(const char *s) { cb->log_msg(cb->ctx, s); }
    void logopen(const char *s) { cb->logopen(cb->ctx, s); }
    void logappend(const char *s) { cb->logappend(cb->ctx, s); }
    void logclose() { cb->logclose(cb->ctx); }
    void canon_error(const char *msg) { cb->canon_error(cb->ctx, msg); }
    void turn_probe_on() { cb->turn_probe_on(cb->ctx); }
    void turn_probe_off() { cb->turn_probe_off(cb->ctx); }
    void set_block_delete(int32_t enabled) { cb->set_block_delete(cb->ctx, enabled); }
    int32_t get_block_delete() { return cb->get_block_delete(cb->ctx); }
    void set_optional_program_stop_flag(int32_t enabled) { cb->set_optional_program_stop(cb->ctx, enabled); }
    int32_t get_optional_program_stop_flag() { return cb->get_optional_program_stop(cb->ctx); }
    void set_parameter_file_name(const char *name) { cb->set_parameter_file_name(cb->ctx, name); }
    void on_reset() { cb->on_reset(cb->ctx); }
    double get_user_defined_result() { return cb->get_user_defined_result(cb->ctx); }
    double get_external_feed_rate() { return cb->get_external_feed_rate(cb->ctx); }
    double get_external_traverse_rate() { return cb->get_external_traverse_rate(cb->ctx); }
    double get_external_length_units() { return cb->get_external_length_units(cb->ctx); }
    double get_external_angle_units() { return cb->get_external_angle_units(cb->ctx); }
    double get_external_motion_control_tolerance() { return cb->get_external_motion_control_tolerance(cb->ctx); }
    double get_external_motion_control_naivecam_tolerance() { return cb->get_external_motion_control_naivecam_tolerance(cb->ctx); }
    int32_t get_external_flood() { return cb->get_external_flood(cb->ctx); }
    int32_t get_external_mist() { return cb->get_external_mist(cb->ctx); }
    double get_external_position_x() { return cb->get_external_position_x(cb->ctx); }
    double get_external_position_y() { return cb->get_external_position_y(cb->ctx); }
    double get_external_position_z() { return cb->get_external_position_z(cb->ctx); }
    double get_external_position_a() { return cb->get_external_position_a(cb->ctx); }
    double get_external_position_b() { return cb->get_external_position_b(cb->ctx); }
    double get_external_position_c() { return cb->get_external_position_c(cb->ctx); }
    double get_external_position_u() { return cb->get_external_position_u(cb->ctx); }
    double get_external_position_v() { return cb->get_external_position_v(cb->ctx); }
    double get_external_position_w() { return cb->get_external_position_w(cb->ctx); }
    double get_external_probe_position_x() { return cb->get_external_probe_position_x(cb->ctx); }
    double get_external_probe_position_y() { return cb->get_external_probe_position_y(cb->ctx); }
    double get_external_probe_position_z() { return cb->get_external_probe_position_z(cb->ctx); }
    double get_external_probe_position_a() { return cb->get_external_probe_position_a(cb->ctx); }
    double get_external_probe_position_b() { return cb->get_external_probe_position_b(cb->ctx); }
    double get_external_probe_position_c() { return cb->get_external_probe_position_c(cb->ctx); }
    double get_external_probe_position_u() { return cb->get_external_probe_position_u(cb->ctx); }
    double get_external_probe_position_v() { return cb->get_external_probe_position_v(cb->ctx); }
    double get_external_probe_position_w() { return cb->get_external_probe_position_w(cb->ctx); }
    double get_external_probe_value() { return cb->get_external_probe_value(cb->ctx); }
    int32_t get_external_probe_tripped_value() { return cb->get_external_probe_tripped_value(cb->ctx); }
    double get_external_speed(int32_t spindle) { return cb->get_external_speed(cb->ctx, spindle); }
    double get_external_tool_length_xoffset() { return cb->get_external_tool_length_xoffset(cb->ctx); }
    double get_external_tool_length_yoffset() { return cb->get_external_tool_length_yoffset(cb->ctx); }
    double get_external_tool_length_zoffset() { return cb->get_external_tool_length_zoffset(cb->ctx); }
    double get_external_tool_length_aoffset() { return cb->get_external_tool_length_aoffset(cb->ctx); }
    double get_external_tool_length_boffset() { return cb->get_external_tool_length_boffset(cb->ctx); }
    double get_external_tool_length_coffset() { return cb->get_external_tool_length_coffset(cb->ctx); }
    double get_external_tool_length_uoffset() { return cb->get_external_tool_length_uoffset(cb->ctx); }
    double get_external_tool_length_voffset() { return cb->get_external_tool_length_voffset(cb->ctx); }
    double get_external_tool_length_woffset() { return cb->get_external_tool_length_woffset(cb->ctx); }
    int32_t get_external_tool_slot() { return cb->get_external_tool_slot(cb->ctx); }
    int32_t get_external_selected_tool_slot() { return cb->get_external_selected_tool_slot(cb->ctx); }
    int32_t get_external_tc_fault() { return cb->get_external_tc_fault(cb->ctx); }
    int32_t get_external_tc_reason() { return cb->get_external_tc_reason(cb->ctx); }
    int32_t get_external_queue_empty() { return cb->get_external_queue_empty(cb->ctx); }
    int32_t get_external_axis_mask() { return cb->get_external_axis_mask(cb->ctx); }
    int32_t get_external_digital_input(int32_t index, int32_t def) { return cb->get_external_digital_input(cb->ctx, index, def); }
    double get_external_analog_input(int32_t index, double def) { return cb->get_external_analog_input(cb->ctx, index, def); }
    int32_t get_external_feed_override_enable() { return cb->get_external_feed_override_enable(cb->ctx); }
    int32_t get_external_spindle_override_enable(int32_t spindle) { return cb->get_external_spindle_override_enable(cb->ctx, spindle); }
    int32_t get_external_adaptive_feed_enable() { return cb->get_external_adaptive_feed_enable(cb->ctx); }
    int32_t get_external_feed_hold_enable() { return cb->get_external_feed_hold_enable(cb->ctx); }
    int32_t get_external_offset_applied() { return cb->get_external_offset_applied(cb->ctx); }

    // ---- Methods with enum type conversions ----
    // The callback table uses int32_t; the C++ API uses enums.

    void use_length_units(CANON_UNITS units) {
        cb->use_length_units(cb->ctx, static_cast<int32_t>(units));
    }

    void select_plane(CANON_PLANE plane) {
        cb->select_plane(cb->ctx, static_cast<int32_t>(plane));
    }

    void set_motion_control_mode(CANON_MOTION_MODE mode, double tolerance) {
        cb->set_motion_control_mode(cb->ctx, static_cast<int32_t>(mode), tolerance);
    }

    void set_feed_reference(CANON_FEED_REFERENCE reference) {
        cb->set_feed_reference(cb->ctx, static_cast<int32_t>(reference));
    }

    void start_cutter_radius_compensation(int32_t direction) {
        cb->start_cutter_radius_compensation(cb->ctx, direction);
    }

    CANON_MOTION_MODE get_external_motion_control_mode() {
        return static_cast<CANON_MOTION_MODE>(cb->get_external_motion_control_mode(cb->ctx));
    }

    CANON_PLANE get_external_plane() {
        return static_cast<CANON_PLANE>(cb->get_external_plane(cb->ctx));
    }

    CANON_DIRECTION get_external_spindle(int32_t spindle) {
        return static_cast<CANON_DIRECTION>(cb->get_external_spindle(cb->ctx, spindle));
    }

    CANON_UNITS get_external_length_unit_type() {
        return static_cast<CANON_UNITS>(cb->get_external_length_unit_type(cb->ctx));
    }

    // ---- Methods with default arguments ----

    void start_spindle_clockwise(int32_t spindle, int32_t wait_for_atspeed = 1) {
        cb->start_spindle_clockwise(cb->ctx, spindle, wait_for_atspeed);
    }

    void start_spindle_counterclockwise(int32_t spindle, int32_t wait_for_atspeed = 1) {
        cb->start_spindle_counterclockwise(cb->ctx, spindle, wait_for_atspeed);
    }

    // ---- Methods with complex type conversions ----

    // StateTag → uint64_t opaque pointer
    void update_tag(StateTag tag) {
        cb->update_tag(cb->ctx, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&tag)));
    }

    // std::vector<CONTROL_POINT> → ptr + len
    void nurbs_feed(int32_t lineno, const std::vector<CONTROL_POINT> &pts, uint32_t k) {
        std::vector<canon_control_point_t> cpts(pts.size());
        for (size_t i = 0; i < pts.size(); i++) {
            cpts[i].x = pts[i].X;
            cpts[i].y = pts[i].Y;
            cpts[i].w = pts[i].W;
        }
        cb->nurbs_feed(cb->ctx, lineno, cpts.data(), cpts.size(), k);
    }

    // EmcPose offset → 9 flattened doubles
    void set_tool_table_entry(int32_t pocket, int32_t toolno, EmcPose offset,
                              double diameter, double frontangle,
                              double backangle, int32_t orient) {
        cb->set_tool_table_entry(cb->ctx, pocket, toolno,
            offset.tran.x, offset.tran.y, offset.tran.z,
            offset.a, offset.b, offset.c,
            offset.u, offset.v, offset.w,
            diameter, frontangle, backangle, orient);
    }

    // EmcPose → 9 flattened doubles
    void use_tool_length_offset(EmcPose offset) {
        cb->use_tool_length_offset(cb->ctx,
            offset.tran.x, offset.tran.y, offset.tran.z,
            offset.a, offset.b, offset.c,
            offset.u, offset.v, offset.w);
    }

    // Reconstructs CANON_TOOL_TABLE from out-parameters
    CANON_TOOL_TABLE get_external_tool_table(int32_t pocket) {
        CANON_TOOL_TABLE t;
        double offset[9];
        cb->get_external_tool_table(cb->ctx, pocket,
            &t.toolno, offset, &t.diameter, &t.frontangle, &t.backangle,
            &t.orientation);
        t.pocketno = pocket;
        t.offset.tran.x = offset[0];
        t.offset.tran.y = offset[1];
        t.offset.tran.z = offset[2];
        t.offset.a = offset[3];
        t.offset.b = offset[4];
        t.offset.c = offset[5];
        t.offset.u = offset[6];
        t.offset.v = offset[7];
        t.offset.w = offset[8];
        return t;
    }

    // Looks up a tool by toolno. Returns true if found, false otherwise.
    bool get_tool_by_number(int32_t toolno, CANON_TOOL_TABLE *t) {
        double offset[9];
        int32_t ret = cb->get_tool_by_number(cb->ctx, toolno,
            &t->pocketno, offset, &t->diameter, &t->frontangle, &t->backangle,
            &t->orientation);
        if (ret != 0)
            return false;
        t->toolno = toolno;
        t->offset.tran.x = offset[0];
        t->offset.tran.y = offset[1];
        t->offset.tran.z = offset[2];
        t->offset.a = offset[3];
        t->offset.b = offset[4];
        t->offset.c = offset[5];
        t->offset.u = offset[6];
        t->offset.v = offset[7];
        t->offset.w = offset[8];
        return true;
    }

    // Reconstructs EmcPose from out-parameter array
    EmcPose get_external_offsets() {
        EmcPose p;
        double vals[9];
        cb->get_external_offsets(cb->ctx, vals);
        p.tran.x = vals[0]; p.tran.y = vals[1]; p.tran.z = vals[2];
        p.a = vals[3]; p.b = vals[4]; p.c = vals[5];
        p.u = vals[6]; p.v = vals[7]; p.w = vals[8];
        return p;
    }

    // Callback returns const char** out-param; C++ API copies into buffer
    void get_external_parameter_file_name(char *filename, int max_size) {
        const char *name = nullptr;
        cb->get_external_parameter_file_name(cb->ctx, &name);
        if (name) {
            snprintf(filename, max_size, "%s", name);
        } else {
            filename[0] = '\0';
        }
    }

    double get_external_hal_value(const char *name, int32_t *found) {
        return cb->get_external_hal_value(cb->ctx, name, found);
    }
};

#endif // CANON_INTERFACE_HH
