#ifndef _HAL_RCOMP_H
#define _HAL_RCOMP_H

#include <rtapi.h>

RTAPI_BEGIN_DECLS

// compiled component support
#define CCOMP_MAGIC  0xbeef0815
typedef struct {
    int magic;
    hal_comp_t *comp;
    int n_pins;
    hal_pin_t  **pin;           // all members (nesting resolved)
    unsigned long *changed;     // bitmap
    hal_data_u    *tracking;    // tracking values of monitored pins
    void *user_data;             // uninterpreted by HAL code
    unsigned long user_flags;    // uninterpreted by HAL code
} hal_compiled_comp_t;

// flags for userarg2 in a rcomp
typedef enum {
    // if a rcomp is defined like so in halcmd:
    // newcomp motorctrl timer=100 acceptdefaults
    // any pin value for OUT/IN_OUT present in the MT_HALRCOMP_BIND
    // message will be applied IFF THE COMPONENT IS IN STATE UNBOUND
    // AND THE COMPONENT WAS NEVER BOUND BEFORE
    // technically this means: the first UI to connect ever will get a
    // chance on setting the OUT/IO values; other UI's wont. Also,
    // repeated disconnects/connects will NOT cause the BIND
    // values to be accepted.

    RCOMP_ACCEPT_VALUES_ON_BIND = 1,

} rcompflags_t;

typedef int(*comp_report_callback_t)(int,  hal_compiled_comp_t *,
				     hal_pin_t *pin,
				     hal_data_u *value,
				     void *cb_data);

extern int hal_compile_comp(const char *name, hal_compiled_comp_t **ccomp);
extern int hal_ccomp_match(hal_compiled_comp_t *ccomp);
extern int hal_ccomp_report(hal_compiled_comp_t *ccomp,
			    comp_report_callback_t report_cb,
			    void *cb_data, int report_all);
extern int hal_ccomp_free(hal_compiled_comp_t *ccomp);
extern int hal_ccomp_args(hal_compiled_comp_t *ccomp, int *arg1, int *arg2);


RTAPI_END_DECLS

#endif
