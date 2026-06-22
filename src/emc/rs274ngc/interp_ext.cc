/********************************************************************
* Description: interp_ext.cc
*   Extension handler registry and dispatch for the RS274NGC interpreter.
*   Replaces the removed Python dispatch with a C callback mechanism.
*
* License: GPL Version 2
* Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
********************************************************************/

#include "rs274ngc_interp.hh"
#include "interp_ext.h"
#include "interp_queue.hh"
#include <unordered_map>
#include <string>

// --- Registry data structure (opaque to header consumers) ---

struct OwordEntry {
    interp_ext_oword_fn_cb fn;
    void *user;
};

struct RemapPrologEntry {
    interp_ext_remap_prolog_fn_cb fn;
    void *user;
};

struct RemapEpilogEntry {
    interp_ext_remap_epilog_fn_cb fn;
    void *user;
};

struct InterpExtRegistry {
    std::unordered_map<std::string, OwordEntry> owords;
    std::unordered_map<std::string, RemapPrologEntry> prologs;
    std::unordered_map<std::string, RemapEpilogEntry> epilogs;
};

// --- Context callback implementations ---

static double ctx_get_param(void *interp, const char *name)
{
    Interp *ip = static_cast<Interp*>(interp);
    int status;
    double value = 0.0;
    ip->find_named_param(name, &status, &value);
    return value;
}

static int ctx_set_param(void *interp, const char *name, double val)
{
    Interp *ip = static_cast<Interp*>(interp);
    ip->add_named_param(name, name[0] == '_' ? PA_GLOBAL : 0);
    return ip->store_named_param(&ip->_setup, name, val);
}

static int ctx_find_tool_pocket(void *interp, int tool_number)
{
    Interp *ip = static_cast<Interp*>(interp);
    int pocket = -1;
    ip->find_tool_pocket(&ip->_setup, tool_number, &pocket);
    return pocket;
}

static void ctx_set_error(void *interp, const char *msg)
{
    Interp *ip = static_cast<Interp*>(interp);
    ip->setSavedError(msg);
}

// --- Block word accessors (current remap block) ---

#define IP static_cast<Interp*>(interp)
#define RBLOCK (IP->_setup.blocks[IP->_setup.remap_level])

static int    ctx_block_t_flag(void *interp)     { return RBLOCK.t_flag; }
static int    ctx_block_t_number(void *interp)   { return RBLOCK.t_number; }
static int    ctx_block_s_flag(void *interp)     { return RBLOCK.s_flag; }
static double ctx_block_s_number(void *interp)   { return RBLOCK.s_number; }
static int    ctx_block_f_flag(void *interp)     { return RBLOCK.f_flag; }
static double ctx_block_f_number(void *interp)   { return RBLOCK.f_number; }
static int    ctx_block_q_flag(void *interp)     { return RBLOCK.q_flag; }
static double ctx_block_q_number(void *interp)   { return RBLOCK.q_number; }
static int    ctx_block_builtin_used(void *interp) { return RBLOCK.builtin_used; }
static int    ctx_block_motion_code(void *interp)  { return RBLOCK.g_modes[1]; }

// --- Setup state read ---

static int    ctx_get_selected_tool(void *interp)    { return IP->_setup.selected_tool; }
static int    ctx_get_selected_pocket(void *interp)  { return IP->_setup.selected_pocket; }
static int    ctx_get_current_tool(void *interp)     { return IP->_setup.tool_table[0].toolno; }
static int    ctx_get_current_pocket(void *interp)   { return IP->_setup.current_pocket; }
static int    ctx_get_cutter_comp_side(void *interp) { return IP->_setup.cutter_comp_side; }
static int    ctx_get_value_returned(void *interp)   { return IP->_setup.value_returned; }
static double ctx_get_return_value(void *interp)     { return IP->_setup.return_value; }
static double ctx_get_feed_rate(void *interp)        { return IP->_setup.feed_rate; }
static int    ctx_get_feed_mode(void *interp)        { return IP->_setup.feed_mode; }
static double ctx_get_speed(void *interp, int s)     { return IP->_setup.speed[s]; }
static int    ctx_get_motion_mode(void *interp)      { return IP->_setup.motion_mode; }
static int    ctx_get_plane(void *interp)            { return IP->_setup.plane; }

// --- Setup state write ---

static void ctx_set_selected_tool(void *interp, int v)   { IP->_setup.selected_tool = v; }
static void ctx_set_selected_pocket(void *interp, int v) { IP->_setup.selected_pocket = v; }
static void ctx_set_current_tool(void *interp, int v)    { IP->_setup.tool_table[0].toolno = v; }
static void ctx_set_current_pocket(void *interp, int v)  { IP->_setup.current_pocket = v; }
static void ctx_set_speed_value(void *interp, int s, double v) { IP->_setup.speed[s] = v; }
static void ctx_set_feed_rate_value(void *interp, double v)    { IP->_setup.feed_rate = v; }
static void ctx_set_motion_mode(void *interp, int v)          { IP->_setup.motion_mode = v; }
static void ctx_set_toolchange_flag(void *interp, int v)      { IP->_setup.toolchange_flag = v; }
static void ctx_call_set_tool_parameters(void *interp)        { IP->set_tool_parameters(); }

// --- Canon calls ---

static void ctx_canon_select_tool(void *interp, int tool) {
    IP->_setup.canon.select_tool(tool);
}
static void ctx_canon_change_tool(void *interp, int pocket) {
    IP->_setup.canon.change_tool(pocket);
}
static void ctx_canon_change_tool_number(void *interp, int pocket) {
    IP->_setup.canon.change_tool_number(pocket);
}
static void ctx_canon_enqueue_set_spindle_speed(void *interp, int spindle, double speed) {
    enqueue_SET_SPINDLE_SPEED(&IP->_setup, spindle, speed);
}
static void ctx_canon_enqueue_set_feed_rate(void *interp, double rate) {
    enqueue_SET_FEED_RATE(&IP->_setup, rate);
}

#undef RBLOCK
#undef IP

// Per-call state for get_phase/get_user (stored in _setup for multi-instance safety)

static int32_t ctx_get_phase(void *interp) { return static_cast<Interp*>(interp)->_setup.ext_phase; }
static void *ctx_get_user(void *interp) { return static_cast<Interp*>(interp)->_setup.ext_user; }

static void fill_ctx(interp_ctx_callbacks_t *ctx, Interp *ip, void *user, int phase)
{
    ctx->ctx = ip;

    ip->_setup.ext_phase = phase;
    ip->_setup.ext_user = user;

    // Named parameters
    ctx->get_param = ctx_get_param;
    ctx->set_param = ctx_set_param;
    ctx->find_tool_pocket = ctx_find_tool_pocket;
    ctx->set_error = ctx_set_error;

    // Block word access
    ctx->block_t_flag = ctx_block_t_flag;
    ctx->block_t_number = ctx_block_t_number;
    ctx->block_s_flag = ctx_block_s_flag;
    ctx->block_s_number = ctx_block_s_number;
    ctx->block_f_flag = ctx_block_f_flag;
    ctx->block_f_number = ctx_block_f_number;
    ctx->block_q_flag = ctx_block_q_flag;
    ctx->block_q_number = ctx_block_q_number;
    ctx->block_builtin_used = ctx_block_builtin_used;
    ctx->block_motion_code = ctx_block_motion_code;

    // Setup state read
    ctx->get_selected_tool = ctx_get_selected_tool;
    ctx->get_selected_pocket = ctx_get_selected_pocket;
    ctx->get_current_tool = ctx_get_current_tool;
    ctx->get_current_pocket = ctx_get_current_pocket;
    ctx->get_cutter_comp_side = ctx_get_cutter_comp_side;
    ctx->get_value_returned = ctx_get_value_returned;
    ctx->get_return_value = ctx_get_return_value;
    ctx->get_feed_rate = ctx_get_feed_rate;
    ctx->get_feed_mode = ctx_get_feed_mode;
    ctx->get_speed = ctx_get_speed;
    ctx->get_motion_mode = ctx_get_motion_mode;
    ctx->get_plane = ctx_get_plane;

    // Setup state write
    ctx->set_selected_tool = ctx_set_selected_tool;
    ctx->set_selected_pocket = ctx_set_selected_pocket;
    ctx->set_current_tool = ctx_set_current_tool;
    ctx->set_current_pocket = ctx_set_current_pocket;
    ctx->set_speed_value = ctx_set_speed_value;
    ctx->set_feed_rate_value = ctx_set_feed_rate_value;
    ctx->set_motion_mode = ctx_set_motion_mode;
    ctx->set_toolchange_flag = ctx_set_toolchange_flag;
    ctx->call_set_tool_parameters = ctx_call_set_tool_parameters;

    // Canon calls
    ctx->canon_select_tool = ctx_canon_select_tool;
    ctx->canon_change_tool = ctx_canon_change_tool;
    ctx->canon_change_tool_number = ctx_canon_change_tool_number;
    ctx->canon_enqueue_set_spindle_speed = ctx_canon_enqueue_set_spindle_speed;
    ctx->canon_enqueue_set_feed_rate = ctx_canon_enqueue_set_feed_rate;

    ctx->get_phase = ctx_get_phase;
    ctx->get_user = ctx_get_user;
}

// --- Interp C++ registration methods ---

int Interp::ext_register_oword(const char *name, interp_ext_oword_fn_cb fn, void *user)
{
    if (!ext_registry) ext_registry = new InterpExtRegistry;
    ext_registry->owords[name] = {fn, user};
    return 0;
}

int Interp::ext_register_remap_prolog(const char *name, interp_ext_remap_prolog_fn_cb fn, void *user)
{
    if (!ext_registry) ext_registry = new InterpExtRegistry;
    ext_registry->prologs[name] = {fn, user};
    return 0;
}

int Interp::ext_register_remap_epilog(const char *name, interp_ext_remap_epilog_fn_cb fn, void *user)
{
    if (!ext_registry) ext_registry = new InterpExtRegistry;
    ext_registry->epilogs[name] = {fn, user};
    return 0;
}

bool Interp::ext_has_oword(const char *name)
{
    if (!ext_registry) return false;
    return ext_registry->owords.count(name) > 0;
}

bool Interp::ext_has_remap_handler(const char *name)
{
    if (!ext_registry) return false;
    return ext_registry->prologs.count(name) > 0 ||
           ext_registry->epilogs.count(name) > 0;
}

int Interp::ext_call_oword(const char *name, const double *args, int n_args,
                           double *retval, int phase)
{
    if (!ext_registry) return INTERP_EXT_ERROR;
    auto it = ext_registry->owords.find(name);
    if (it == ext_registry->owords.end()) return INTERP_EXT_ERROR;

    interp_ctx_callbacks_t ctx;
    fill_ctx(&ctx, this, it->second.user, phase);
    return it->second.fn(&ctx, name, args, n_args, retval);
}

int Interp::ext_call_remap_prolog(const char *name, int phase)
{
    if (!ext_registry) return INTERP_EXT_ERROR;
    auto it = ext_registry->prologs.find(name);
    if (it == ext_registry->prologs.end()) return INTERP_EXT_ERROR;

    interp_ctx_callbacks_t ctx;
    fill_ctx(&ctx, this, it->second.user, phase);
    return it->second.fn(&ctx, name);
}

int Interp::ext_call_remap_epilog(const char *name, int phase)
{
    if (!ext_registry) return INTERP_EXT_ERROR;
    auto it = ext_registry->epilogs.find(name);
    if (it == ext_registry->epilogs.end()) return INTERP_EXT_ERROR;

    interp_ctx_callbacks_t ctx;
    fill_ctx(&ctx, this, it->second.user, phase);
    return it->second.fn(&ctx, name);
}

// --- C-linkage wrappers (exported from librs274.so) ---

extern "C" {

int interp_ext_register_oword(void *interp, const char *name,
                              interp_ext_oword_fn_cb fn, void *user)
{
    return static_cast<Interp*>(interp)->ext_register_oword(name, fn, user);
}

int interp_ext_register_remap_prolog(void *interp, const char *name,
                                     interp_ext_remap_prolog_fn_cb fn, void *user)
{
    return static_cast<Interp*>(interp)->ext_register_remap_prolog(name, fn, user);
}

int interp_ext_register_remap_epilog(void *interp, const char *name,
                                     interp_ext_remap_epilog_fn_cb fn, void *user)
{
    return static_cast<Interp*>(interp)->ext_register_remap_epilog(name, fn, user);
}

int interp_ext_has_oword(void *interp, const char *name)
{
    return static_cast<Interp*>(interp)->ext_has_oword(name) ? 1 : 0;
}

int interp_ext_has_remap_handler(void *interp, const char *name)
{
    return static_cast<Interp*>(interp)->ext_has_remap_handler(name) ? 1 : 0;
}

} // extern "C"

// Called from Interp destructor (avoids incomplete-type delete warning)
void interp_ext_registry_destroy(InterpExtRegistry *reg)
{
    delete reg;
}
