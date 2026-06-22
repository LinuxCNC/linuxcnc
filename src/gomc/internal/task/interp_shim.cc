// interp_shim.cc — thin C++ wrapper exposing InterpBase virtual calls as plain C.
// This is the ONLY C++ code in the Go milltask module.
//
// Original interpreter: Copyright 2004-2006 Jeff Epler <jepler@unpythonic.net>
//                       and Chris Radek <chris@timeguy.com> (GPL v2)
// This shim: Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2

#include "config.h"
#include "emc/rs274ngc/interp_base.hh"
#include "emc/rs274ngc/rs274ngc_interp.hh"  // Interp class, USER_DEFINED_FUNCTION_NUM

// Include the generated canon callback table header
#define CANON_API_CGO
#include "gomc/generated/gmi/canon/canon_api.h"

// Include interp_shim.h for the interp_ini_accessor_t typedef.
#include "interp_shim.h"

extern "C" {

// interp_new creates the default interpreter (rs274ngc).
void *interp_new(void) {
    return static_cast<void*>(makeInterp());
}

// interp_from_lib loads an interpreter from a shared library.
void *interp_from_lib(const char *shlib) {
    return static_cast<void*>(interp_from_shlib(shlib));
}

// interp_delete destroys an interpreter instance.
void interp_delete(void *handle) {
    delete static_cast<InterpBase*>(handle);
}

// interp_ini_load loads INI configuration.
int interp_ini_load(void *handle, const char *inifile) {
    return static_cast<InterpBase*>(handle)->ini_load(inifile);
}

// interp_init initializes the interpreter.
int interp_init(void *handle) {
    return static_cast<InterpBase*>(handle)->init();
}

// interp_open opens a G-code file for interpretation.
int interp_open(void *handle, const char *filename) {
    return static_cast<InterpBase*>(handle)->open(filename);
}

// interp_read reads the next line from the open file.
int interp_read(void *handle) {
    return static_cast<InterpBase*>(handle)->read();
}

// interp_read_string reads a line from a string.
int interp_read_string(void *handle, const char *line) {
    return static_cast<InterpBase*>(handle)->read(line);
}

// interp_execute executes the last read line.
int interp_execute(void *handle) {
    return static_cast<InterpBase*>(handle)->execute();
}

// interp_execute_string executes a string directly.
int interp_execute_string(void *handle, const char *line) {
    return static_cast<InterpBase*>(handle)->execute(line);
}

// interp_execute_string_lineno executes a string with explicit line number.
int interp_execute_string_lineno(void *handle, const char *line, int line_number) {
    return static_cast<InterpBase*>(handle)->execute(line, line_number);
}

// interp_synch synchronizes interpreter state with the machine.
int interp_synch(void *handle) {
    return static_cast<InterpBase*>(handle)->synch();
}

// interp_close closes the currently open file.
int interp_close(void *handle) {
    return static_cast<InterpBase*>(handle)->close();
}

// interp_reset resets the interpreter to initial state.
int interp_reset(void *handle) {
    return static_cast<InterpBase*>(handle)->reset();
}

// interp_exit exits the interpreter.
int interp_exit(void *handle) {
    return static_cast<InterpBase*>(handle)->exit();
}

// interp_on_abort notifies the interpreter of an abort condition.
int interp_on_abort(void *handle, int reason, const char *message) {
    return static_cast<InterpBase*>(handle)->on_abort(reason, message);
}

// interp_set_canon_callbacks sets the canon callback table.
void interp_set_canon_callbacks(void *handle, const canon_callbacks_t *cb) {
    static_cast<InterpBase*>(handle)->set_canon_callbacks(cb);
}

// interp_line returns the current line number.
int interp_line(void *handle) {
    return static_cast<InterpBase*>(handle)->line();
}

// interp_sequence_number returns the current sequence number (N-word).
int interp_sequence_number(void *handle) {
    return static_cast<InterpBase*>(handle)->sequence_number();
}

// interp_call_level returns the current subroutine call level.
int interp_call_level(void *handle) {
    return static_cast<InterpBase*>(handle)->call_level();
}

// interp_error_text retrieves the error text for a return code.
const char *interp_error_text(void *handle, int errcode, char *buf, size_t buflen) {
    return static_cast<InterpBase*>(handle)->error_text(errcode, buf, buflen);
}

// interp_line_text retrieves the current line text.
const char *interp_line_text(void *handle, char *buf, size_t buflen) {
    return static_cast<InterpBase*>(handle)->line_text(buf, buflen);
}

// interp_file_name retrieves the current file name.
const char *interp_file_name(void *handle, char *buf, size_t buflen) {
    return static_cast<InterpBase*>(handle)->file_name(buf, buflen);
}

// interp_command retrieves the current command text.
const char *interp_command(void *handle, char *buf, size_t buflen) {
    return static_cast<InterpBase*>(handle)->command(buf, buflen);
}

// interp_set_loglevel sets the interpreter log level.
void interp_set_loglevel(void *handle, int level) {
    static_cast<InterpBase*>(handle)->set_loglevel(level);
}

// interp_set_loop_on_main_m99 controls M99 loop behavior.
void interp_set_loop_on_main_m99(void *handle, int state) {
    static_cast<InterpBase*>(handle)->set_loop_on_main_m99(state != 0);
}

// interp_set_task_mode sets _setup.task_mode (1=task, 0=preview).
// When task_mode=1, save_parameters actually writes the var file.
void interp_set_task_mode(void *handle, int mode) {
    Interp *ip = dynamic_cast<Interp*>(static_cast<InterpBase*>(handle));
    if (ip) {
        ip->_setup.task_mode = mode;
    }
}

// interp_set_user_defined_function registers a function pointer for M(100+idx).
void interp_set_user_defined_function(void *handle, int idx,
    void (*fn)(int num, double arg1, double arg2)) {
    if (idx < 0 || idx >= USER_DEFINED_FUNCTION_NUM) return;
    Interp *ip = dynamic_cast<Interp*>(static_cast<InterpBase*>(handle));
    if (ip) {
        ip->_setup.user_defined_function[idx] = fn;
    }
}

// interp_active_g_codes retrieves the active G-code array.
void interp_active_g_codes(void *handle, int *gcodes, int max_len) {
    static_cast<InterpBase*>(handle)->active_g_codes(gcodes);
    (void)max_len; // array size is ACTIVE_G_CODES (17)
}

// interp_active_m_codes retrieves the active M-code array.
void interp_active_m_codes(void *handle, int *mcodes, int max_len) {
    static_cast<InterpBase*>(handle)->active_m_codes(mcodes);
    (void)max_len;
}

// interp_active_settings retrieves the active settings array.
void interp_active_settings(void *handle, double *settings, int max_len) {
    static_cast<InterpBase*>(handle)->active_settings(settings);
    (void)max_len;
}

// interp_set_ini_accessor stores the INI accessor callback struct in the
// interpreter's setup struct.  Must be called before init().
void interp_set_ini_accessor(void *handle, const interp_ini_accessor_t *accessor) {
    Interp *ip = dynamic_cast<Interp*>(static_cast<InterpBase*>(handle));
    if (ip && accessor) {
        ip->_setup.ini_accessor.ctx = accessor->ctx;
        ip->_setup.ini_accessor.get = accessor->get;
        ip->_setup.ini_accessor.get_nth = accessor->get_nth;
    }
}

// interp_ini_load_accessor loads INI config using the accessor callbacks
// instead of opening a file.  Replaces interp_ini_load() for gomc usage.
int interp_ini_load_accessor(void *handle, const interp_ini_accessor_t *accessor) {
    Interp *ip = dynamic_cast<Interp*>(static_cast<InterpBase*>(handle));
    if (!ip || !accessor) return -1;

    // Store accessor for runtime use (fetch_ini_param)
    ip->_setup.ini_accessor.ctx = accessor->ctx;
    ip->_setup.ini_accessor.get = accessor->get;
    ip->_setup.ini_accessor.get_nth = accessor->get_nth;

    // Read PARAMETER_FILE — the only thing ini_load() does
    const char *param_file = accessor->get(accessor->ctx, "RS274NGC", "PARAMETER_FILE");
    if (param_file && param_file[0] != '\0') {
        ip->_setup.canon.set_parameter_file_name(param_file);
    } else {
        return -1;  // parameter file name is required
    }
    return 0;
}

} // extern "C"
