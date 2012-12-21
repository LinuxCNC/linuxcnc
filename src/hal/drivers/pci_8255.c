//    Copyright (C) 2008 Jeff Epler
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "rtapi_string.h"

#define MAX 16

MODULE_LICENSE("GPL");
int io[MAX] = {0,};
int dir[MAX] = {0,};
RTAPI_MP_ARRAY_INT(io, MAX, "I/O addresses of 8255s");
RTAPI_MP_ARRAY_INT(dir, MAX, "I/O direction of 8255s");

static int comp_id;

union inv { hal_bit_t *not_; hal_bit_t invert; };

struct port {
    hal_bit_t *a[8];
    hal_bit_t *b[8];
    hal_bit_t *c[8];
    union inv ai[8];
    union inv bi[8];
    union inv ci[8];
    hal_u32_t dir_;
    hal_u32_t ioaddr;
};

struct state {
    struct port ports[3];
    hal_bit_t *relay;
    hal_bit_t relay_invert;
    hal_u32_t ioaddr;
};

static void read(struct port *inst, long period);
static void write(struct port *inst, long period);
static void write_relay(struct state *inst, long period);
static void write_all(struct state *inst, long period);
static void read_all(struct state *inst, long period);

static void extra_cleanup(void);

#define SHIFT 4
static inline void WRITE(int value, hal_u32_t base, int offset) { 
    // int *mem = (int*) base;
    rtapi_outb(value, base + SHIFT*offset);
    // mem[offset] = value;
}

static inline int READ(hal_u32_t base, int offset) {
    return rtapi_inb(base + SHIFT*offset);
    // int *mem = (int*) base;
    // return mem[offset];
}


static int export(char *prefix, struct port *inst, int ioaddr, int dir) {
    char buf[HAL_NAME_LEN + 1];
    int r = 0;
    int i;
    hal_pin_dir_t direction;
    int sz = sizeof(struct port);
    memset(inst, 0, sz);
    inst->dir_ = dir;
    inst->ioaddr = ioaddr;

    if(inst->dir_ & 8) direction = HAL_OUT; else direction = HAL_IN;
    for(i=0; i<8; i++) {
        r = hal_pin_bit_newf(direction, &(inst->a[i]), comp_id,
            "%s.a%d", prefix, i);
        if(r != 0) return r;
        if(direction == HAL_OUT) {
            r = hal_pin_bit_newf(direction, &(inst->ai[i].not_), comp_id,
                "%s.a%d-not", prefix, i);
        } else {
            r = hal_param_bit_newf(HAL_RW, &(inst->ai[i].invert), comp_id,
                    "%s.a%d-invert", prefix, i);
        }
        if(r != 0) return r;
    }
    if(inst->dir_ & 2) direction = HAL_OUT; else direction = HAL_IN;
    for(i=0; i<8; i++) {
        r = hal_pin_bit_newf(direction, &(inst->b[i]), comp_id,
            "%s.b%d", prefix, i);
        if(r != 0) return r;
        if(direction == HAL_OUT) {
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
            if(inst->dir_ & 1) direction = HAL_OUT;
            else direction = HAL_IN;
        } else { 
            if(inst->dir_ & 4) direction = HAL_OUT;
            else direction = HAL_IN;
        }
        r = hal_pin_bit_newf(direction, &(inst->c[i]), comp_id,
            "%s.c%d", prefix, i);
        if(r != 0) return r;
        if(direction == HAL_OUT) {
            r = hal_pin_bit_newf(direction, &(inst->ci[i].not_), comp_id,
                "%s.c%d-not", prefix, i);
        } else {
            r = hal_param_bit_newf(HAL_RW, &(inst->ci[i].invert), comp_id,
                    "%s.c%d-invert", prefix, i);
        }
        if(r != 0) return r;
    }
    r = hal_param_u32_newf(HAL_RO, &(inst->dir_), comp_id,
        "%s.dir", prefix);
    if(r != 0) return r;
    rtapi_snprintf(buf, sizeof(buf), "%s.read", prefix);
    r = hal_export_funct(buf, (void(*)(void *inst, long))read, inst, 0, 0, comp_id);
    if(r != 0) return r;
    rtapi_snprintf(buf, sizeof(buf), "%s.write", prefix);
    r = hal_export_funct(buf, (void(*)(void *inst, long))write, inst, 0, 0, comp_id);
    if(r != 0) return r;

    rtapi_print_msg(RTAPI_MSG_DBG, "registering %s ... %x %x\n", prefix,
	(dir&3) | ((dir & 0xc) << 1) | 0x80, ioaddr);

    WRITE((dir&3) | ((dir & 0xc) << 1) | 0x80, ioaddr, 3);

    return 0;
}

static struct state *inst = 0;
static int count = 0;

static void write_relay(struct state *inst, long period) {
    int val = (!*inst->relay) ^ (!inst->relay_invert);
    // relay is active low
    if(val) {
	WRITE(0, inst->ioaddr, 3);
    } else {
	WRITE(0x10, inst->ioaddr, 3);
    }
}

static void write_all(struct state *inst, long period) {
    int i;
    for(i=0; i<count; i++) {
	write_relay(&inst[i], period);
	write(&inst[i].ports[0], period);
	write(&inst[i].ports[1], period);
	write(&inst[i].ports[2], period);
    }
}

static void read_all(struct state *inst, long period) {
    int i;
    for(i=0; i<count; i++) {
	read(&inst[i].ports[0], period);
	read(&inst[i].ports[1], period);
	read(&inst[i].ports[2], period);
    }
}

static int get_count(void);
int rtapi_app_main(void) {
    int r = 0;
    int i, j;
    char buf[HAL_NAME_LEN + 1];

    count = get_count();
    comp_id = hal_init("pci_8255");
    if(comp_id < 0) return comp_id;
    inst = hal_malloc(count * sizeof(struct state));
    if(!inst) goto out_error;

    for(i=0; i<count; i++) {
	// PIB reset low, 15 cycle operation
	WRITE(0x30, io[i], 0);
	// relay off (active-high), cs low
	WRITE(0x10, io[i]+3, 0);
	// relay, CS# as outputs
	WRITE(0x11, io[i]+2, 0);

	for(j=0; j<3; j++) {
	    rtapi_snprintf(buf, sizeof(buf), "pci8255.%d.%d", i, j);
	    r = export(buf, &inst[i].ports[j],
		    io[i] + 0xc0 + 16*j, (dir[i] >> (4*j)) & 0xf);
	    if(r != 0) goto out_error;
	}
	hal_pin_bit_newf(HAL_IN, &(inst[i].relay), comp_id, "pci8255.%d.relay", i);
	hal_param_bit_newf(HAL_RW, &(inst[i].relay_invert), comp_id, 
		    "pci8255.%d.relay-invert", i);
	rtapi_snprintf(buf, sizeof(buf), "pci8255.%d.write-relay", i);
	r = hal_export_funct(buf, (void(*)(void *inst, long))write_relay, &inst[i], 0, 0, comp_id);
	r = hal_param_u32_newf(HAL_RO, &(inst->ioaddr), comp_id,
	    "pci8255.%d.io-addr", i);
	inst->ioaddr = io[i];
	if(r != 0) return r;
    }
    r = hal_export_funct("pci8255.read-all", (void(*)(void *inst, long))read_all, inst, 0, 0, comp_id);
    r = hal_export_funct("pci8255.write-all", (void(*)(void *inst, long))write_all, inst, 0, 0, comp_id);
out_error:
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

#if 0
static int extra_setup(void) {
    int byte;

    rtapi_print_msg(RTAPI_MSG_DBG, "requesting I/O region 0x%x\n",
			io[extra_arg]);
    if(!rtapi_request_region(io[extra_arg], 16, "pci_8255")) {
	// set this I/O port to 0 so that EXTRA_CLEANUP does not release the IO
	// ports that were never requested.
        io[extra_arg] = 0; 
        return -EBUSY;
    }
    dir_ = dir[extra_arg/3] & 0xf;
    ioaddr = io[extra_arg/3];
    byte = (1<<7) + ((dir_ & 8) ? (1<<4) : 0)
         + ((dir_ & 4) ? (1<<3) : 0)
         + ((dir_ & 2) ? (1<<1) : 0)
         + ((dir_ & 1) ? (1<<0) : 0);
    WRITE(byte, ioaddr, 3);
    return 0;
}
#endif

static void extra_cleanup(void) {
#if 0
    int i;
    for(i=0; i < MAX && io[i]; i++) {
        rtapi_print_msg(RTAPI_MSG_DBG, "releasing I/O region 0x%x\n",
			io[i]);
        rtapi_release_region(io[i], 16);
    }
#endif
}

static void write(struct port *inst, long period) {
    int p = dir_;
    static int first=1;

    int i;
    if((p & 5) == 0) {
        int byte = 0;
        for(i=0; i<8; i++) {
            int t = (c(i) != 0) ^ (ci_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        WRITE(byte, ioaddr, 2);
	if(first) rtapi_print_msg(RTAPI_MSG_DBG, "write: 2a %02x\n", byte);
    } else if((p & 5) == 4) {
        int byte = 0;
        for(i=0; i<4; i++) {
            int t = (c(i) != 0) ^ (ci_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        WRITE(byte, ioaddr, 2);
	if(first) rtapi_print_msg(RTAPI_MSG_DBG, "write: 2b %02x\n", byte);
    } else if((p & 5) == 1) {
        int byte = 0;
        for(i=4; i<8; i++) {
            int t = (c(i) != 0) ^ (ci_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        WRITE(byte, ioaddr, 2);
	if(first) rtapi_print_msg(RTAPI_MSG_DBG, "write: 2c %02x\n", byte);
    }

    if((p & 2) == 0) {
        int byte = 0;
        for(i=0; i<8; i++) {
            int t = (b(i) != 0) ^ (bi_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        WRITE(byte, ioaddr, 1);
	if(first) rtapi_print_msg(RTAPI_MSG_DBG, "write: 1 %02x\n", byte);
    }

    if((p & 8) == 0) {
        int byte = 0;
        for(i=0; i<8; i++) {
            int t = (a(i) != 0) ^ (ai_invert(i) != 0);
            if(t) byte |= 1 << i;
        }
        WRITE(byte, ioaddr, 0);
	if(first) rtapi_print_msg(RTAPI_MSG_DBG, "write: 0 %02x\n", byte);
    }
    first = 0;
}

static void read(struct port *inst, long period) {
    int p = dir_;
    int i;
    if((p & 5) == 5) {
        int byte = READ(ioaddr, 2);
        for(i=0; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            c(i) = t;
            ci_not(i) = !t;
        }
    } else if((p & 5) == 4) {
        int byte = READ(ioaddr, 2);
        for(i=4; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            c(i) = t;
            ci_not(i) = !t;
        }
    } else if((p & 5) == 1) {
        int byte = READ(ioaddr, 2);
        for(i=0; i<4; i++) {
            int t = (byte & (1<<i)) != 0;
            c(i) = t;
            ci_not(i) = !t;
        }
    }

    if(p & 2) {
        int byte = READ(ioaddr, 1);
        for(i=0; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            b(i) = t;
            bi_not(i) = !t;
        }
    }

    if(p & 8) {
        int byte = READ(ioaddr, 0);
        for(i=0; i<8; i++) {
            int t = (byte & (1<<i)) != 0;
            a(i) = t;
            ai_not(i) = !t;
        }
    }
}
