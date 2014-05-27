#include <hal.h>

union halobject_union {
    hal_pin_t *pin;
    hal_param_t *param;
    hal_sig_t *signal;
    hal_comp_t *comp;
    hal_group_t *group;
    hal_member_t *member;
};

typedef struct  {
    hal_object_type type;
    union halobject_union o;
    void *ptr; // points to raw value if there's one (usually hal_data_u *)
} halitem_t;
