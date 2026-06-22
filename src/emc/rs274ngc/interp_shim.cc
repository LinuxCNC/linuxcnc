// interp_shim.cc — C++ shim exposing the RS274NGC interpreter to CGo.
//
// Wraps the InterpBase/Interp C++ virtual class as plain C functions
// for use from Go via CGo.
//
// Original interpreter: Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
//                       and Chris Radek <chris@timeguy.com> (GPL v2)
// This shim: Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
#include "interp_shim.h"

#include "rs274ngc_interp.hh"
#include "gomc/generated/gmi/canon/canon_api.h"

struct interp_handle {
    Interp *interp;
};

extern "C" {

interp_handle_t *interp_shim_new(void) {
    auto *h = new interp_handle_t;
    h->interp = new Interp;
    return h;
}

void interp_shim_set_callbacks(interp_handle_t *h,
                               const canon_callbacks_t *cb) {
    if (h && h->interp && cb) {
        h->interp->set_canon_callbacks(cb);
    }
}

int interp_shim_init(interp_handle_t *h) {
    if (!h || !h->interp) return INTERP_SHIM_ERROR;
    return h->interp->init();
}

int interp_shim_open(interp_handle_t *h, const char *filename) {
    if (!h || !h->interp || !filename) return INTERP_SHIM_ERROR;
    return h->interp->open(filename);
}

int interp_shim_read(interp_handle_t *h) {
    if (!h || !h->interp) return INTERP_SHIM_ERROR;
    return h->interp->read();
}

int interp_shim_read_string(interp_handle_t *h, const char *code) {
    if (!h || !h->interp || !code) return INTERP_SHIM_ERROR;
    return h->interp->read(code);
}

int interp_shim_execute(interp_handle_t *h) {
    if (!h || !h->interp) return INTERP_SHIM_ERROR;
    return h->interp->execute();
}

int interp_shim_sequence_number(interp_handle_t *h) {
    if (!h || !h->interp) return 0;
    return h->interp->sequence_number();
}

int interp_shim_close(interp_handle_t *h) {
    if (!h || !h->interp) return INTERP_SHIM_ERROR;
    return h->interp->close();
}

double interp_shim_get_parameter(interp_handle_t *h, int index) {
    if (!h || !h->interp || index < 0 || index >= 5602) return 0.0;
    return h->interp->_setup.parameters[index];
}

void interp_shim_destroy(interp_handle_t *h) {
    if (h) {
        delete h->interp;
        delete h;
    }
}

void interp_shim_error_text(interp_handle_t *h, int error_code,
                            char *buf, int buf_size) {
    if (!h || !h->interp) {
        snprintf(buf, buf_size, "no interpreter");
        return;
    }
    h->interp->error_text(error_code, buf, buf_size);
}

} // extern "C"
