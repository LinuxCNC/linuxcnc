#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "rtapi_string.h"

#define MAX 16

int io[MAX] = {0,};
int dir[MAX] = {0,};
RTAPI_MP_ARRAY_INT(io, MAX, "I/O addresses of 8255s");
RTAPI_MP_ARRAY_INT(dir, MAX, "I/O direction of 8255s");

static int comp_id;

#ifdef MODULE_INFO
MODULE_INFO(emc2, "component:pci_8255:");
MODULE_INFO(emc2, "pin:a0:bit:in:");
MODULE_INFO(emc2, "pin:b0:bit:in:");
MODULE_INFO(emc2, "pin:c0:bit:in:");
MODULE_INFO(emc2, "param:ioaddr:s32:r::None");
MODULE_INFO(emc2, "param:dir:s32:r::None");
MODULE_INFO(emc2, "funct:read:0:");
MODULE_INFO(emc2, "funct:write:0:");
#endif // __MODULE_INFO

union inv { hal_bit_t *not_; hal_bit_t invert; };

struct state {
    hal_bit_t *a[8];
    hal_bit_t *b[8];
    hal_bit_t *c[8];
    union inv ai[8];
    union inv bi[8];
    union inv ci[8];
    hal_s32_t ioaddr;
    hal_s32_t dir_;
};

static void read(struct state *inst, long period);
static void write(struct state *inst, long period);
static int extra_setup(struct state *inst, long extra_arg);
static void extra_cleanup(void);

static int export(char *prefix, long extra_arg) {
    char buf[HAL_NAME_LEN + 2];
    int r = 0;
    int i;
    hal_pin_dir_t direction;
    int sz = sizeof(struct state);
    struct state *inst = hal_malloc(sz);
    memset(inst, 0, sz);
    r = extra_setup(inst, extra_arg);
    if(r != 0) return r;

    if(inst->dir_ & 8) direction = HAL_IN; else direction = HAL_OUT;
    for(i=0; i<8; i++) {
        r = hal_pin_bit_newf(direction, &(inst->a[i]), comp_id,
            "%s.a%d", prefix, i);
        if(r != 0) return r;
        if(direction == HAL_IN) {
            r = hal_pin_bit_newf(direction, &(inst->ai[i].not_), comp_id,
                "%s.a%d-not", prefix, i);
        } else {
            r = hal_param_bit_newf(HAL_RW, &(inst->bi[i].invert), comp_id,
                    "%s.a%d-invert", prefix, i);
        }
        if(r != 0) return r;
    }
    if(inst->dir_ & 2) direction = HAL_IN; else direction = HAL_OUT;
    for(i=0; i<8; i++) {
        r = hal_pin_bit_newf(direction, &(inst->b[i]), comp_id,
            "%s.b%d", prefix, i);
        if(r != 0) return r;
        if(direction == HAL_IN) {
            r = hal_pin_bit_newf(direction, &(inst->bi[i].not_), comp_id,
                "%s.b%d-not", prefix, i);
        } else {
            r = hal_param_bit_newf(HAL_RW, &(inst->bi[i].invert), comp_id,
                    "%s.b%d-invert", prefix, i);
        }
        if(r != 0) return r;
    }
    for(i=0; i<8; i++) {
        if(i < 4) {
            if(inst->dir_ & 1) direction = HAL_IN;
            else direction = HAL_OUT;
        } else { 
            if(inst->dir_ & 4) direction = HAL_IN;
            else direction = HAL_OUT;
        }
        r = hal_pin_bit_newf(direction, &(inst->c[i]), comp_id,
            "%s.c%d", prefix, i);
        if(r != 0) return r;
        if(direction == HAL_IN) {
            r = hal_pin_bit_newf(direction, &(inst->ai[i].not_), comp_id,
                "%s.c%d-not", prefix, i);
        } else {
            r = hal_param_bit_newf(HAL_RW, &(inst->bi[i].invert), comp_id,
                    "%s.c%d-invert", prefix, i);
        }
        if(r != 0) return r;
    }
    r = hal_param_s32_newf(HAL_RO, &(inst->ioaddr), comp_id,
        "%s.io-addr", prefix);
    if(r != 0) return r;
    r = hal_param_s32_newf(HAL_RO, &(inst->dir_), comp_id,
        "%s.dir", prefix);
    if(r != 0) return r;
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.read", prefix);
    r = hal_export_funct(buf, (void(*)(void *inst, long))read, inst, 0, 0, comp_id);
    if(r != 0) return r;
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.write", prefix);
    r = hal_export_funct(buf, (void(*)(void *inst, long))write, inst, 0, 0, comp_id);
    if(r != 0) return r;
    return 0;
}
static int get_count(void);
static int export_1(char *prefix, char *argstr) {
    int arg = simple_strtol(argstr, NULL, 0);
    return export(prefix, arg);
}
int rtapi_app_main(void) {
    int r = 0;
    int i;
    int count = get_count();
    comp_id = hal_init("pci_8255");
    if(comp_id < 0) return comp_id;
    for(i=0; i<count; i++) {
        char buf[HAL_NAME_LEN + 2];
        rtapi_snprintf(buf, HAL_NAME_LEN, "8255.%d", i);
        r = export(buf, i);
        if(r != 0) break;
    }
    hal_set_constructor(comp_id, export_1);
    if(r) {
    extra_cleanup();
        hal_exit(comp_id);
    } else {
        hal_ready(comp_id);
    }
    return r;
}

void rtapi_app_exit(void) {
    extra_cleanup();
    hal_exit(comp_id);
}

#define FUNCTION(name) static void name(struct state *inst, long period)
#define EXTRA_SETUP() static int extra_setup(struct state *inst, long extra_arg)
#define EXTRA_CLEANUP() static void extra_cleanup(void)
#define fperiod (period * 1e-9)
#define a(i) (*inst->a[i])
#define b(i) (*inst->b[i])
#define c(i) (*inst->c[i])
#define ai_invert(i) (inst->ai[i].invert)
#define bi_invert(i) (inst->bi[i].invert)
#define ci_invert(i) (inst->ci[i].invert)
#define ai_not(i) (*inst->ai[i].not_)
#define bi_not(i) (*inst->bi[i].not_)
#define ci_not(i) (*inst->ci[i].not_)
#define ioaddr (inst->ioaddr)
#define dir_ (inst->dir_)

#include "rtapi_errno.h"

int get_count(void) {
    int i = 0;
    for(i=0; i<MAX && io[i]; i++) { /* Nothing */ }
    return i;
}

EXTRA_SETUP() {
    int byte;

    rtapi_print_msg(RTAPI_MSG_ERR, "requesting I/O region 0x%x\n",
			io[extra_arg]);
    if(!rtapi_request_region(io[extra_arg], 16, "pci_8255")) {
	// set this I/O port to 0 so that EXTRA_CLEANUP does not release the IO
	// ports that were never requested.
        io[extra_arg] = 0; 
        return -EBUSY;
    }
    dir_ = dir[extra_arg] & 0xf;
    ioaddr = io[extra_arg];
    byte = (1<<7) + ((dir_ & 8) ? (1<<4) : 0)
         + ((dir_ & 4) ? (1<<3) : 0)
         + ((dir_ & 2) ? (1<<1) : 0)
         + ((dir_ & 1) ? (1<<0) : 0);
    rtapi_outb(byte, ioaddr + 3*4);
    return 0;
}

EXTRA_CLEANUP() {
    int i;
    for(i=0; i < MAX && io[i]; i++) {
        rtapi_print_msg(RTAPI_MSG_ERR, "releasing I/O region 0x%x\n",
			io[i]);
        rtapi_release_region(io[i], 16);
    }
}

FUNCTION(write) {
    int p = dir_;
    int i;
    if((p & 5) == 0) {
        int byte = 0;
        for(i=0; i<8; i++) {
            int t = (c(i) != 0) != (ci_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        rtapi_outb(byte, ioaddr + 2*4);
    } else if((p & 5) == 4) {
        int byte = 0;
        for(i=0; i<4; i++) {
            int t = (c(i) != 0) != (ci_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        rtapi_outb(byte, ioaddr + 2*4);
    } else if((p & 5) == 1) {
        int byte = 0;
        for(i=4; i<8; i++) {
            int t = (c(i) != 0) != (ci_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        rtapi_outb(byte, ioaddr + 2*4);
    }

    if((p & 2) == 0) {
        int byte = 0;
        for(i=4; i<8; i++) {
            int t = (b(i) != 0) != (bi_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        rtapi_outb(byte, ioaddr + 1*4);
    }

    if((p & 8) == 0) {
        int byte = 0;
        for(i=4; i<8; i++) {
            int t = (b(i) != 0) != (bi_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        rtapi_outb(byte, ioaddr);
    }
}

FUNCTION(read) {
    int p = dir_;
    int i;
    if((p & 5) == 5) {
        int byte = rtapi_inb(ioaddr + 2*4);
        for(i=0; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            c(i) = t;
            ci_not(i) = !t;
        }
    } else if((p & 5) == 4) {
        int byte = rtapi_inb(ioaddr + 2*4);
        for(i=4; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            c(i) = t;
            ci_not(i) = !t;
        }
    } else if((p & 5) == 1) {
        int byte = rtapi_inb(ioaddr + 2*4);
        for(i=0; i<4; i++) {
            int t = (byte & (1<<i)) != 0;
            c(i) = t;
            ci_not(i) = !t;
        }
    }

    if(p & 2) {
        int byte = rtapi_inb(ioaddr + 1*4);
        for(i=0; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            b(i) = t;
            bi_not(i) = !t;
        }
    }

    if(p & 8) {
        int byte = rtapi_inb(ioaddr);
        for(i=0; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            a(i) = t;
            ai_not(i) = !t;
        }
    }
}
