//    Copyright 2014 Jeff Epler
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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef RTAPI_PARPORT_H
#define RTAPI_PARPORT_H

#include <rtapi.h>
#include <rtapi_io.h>

// Parport mode flags (from linux/parport.h)
#define PARPORT_MODE_PCSPP	(1<<0)
#define PARPORT_MODE_TRISTATE	(1<<1)
#define PARPORT_MODE_EPP	(1<<2)
#define PARPORT_MODE_ECP	(1<<3)
#define PARPORT_MODE_COMPAT	(1<<4)
#define PARPORT_MODE_DMA	(1<<5)
#define PARPORT_MODE_SAFEININT	(1<<6)

#define RTAPI_PARPORT_DATA_PORT(t) (t->base + 0)
#define RTAPI_PARPORT_STATUS_PORT(t) (t->base + 1)
#define RTAPI_PARPORT_CONTROL_PORT(t) (t->base + 2)
#define RTAPI_PARPORT_EPP_ADDR_PORT(t) (t->base + 3)
#define RTAPI_PARPORT_EPP_DATA_PORT(t) (t->base + 4)
#define RTAPI_PARPORT_ECR_PORT(t) (t->base_hi + 2)

typedef struct rtapi_parport_t
{
    unsigned short base;
    unsigned short base_hi;
    int fd;
} rtapi_parport_t;

RTAPI_BEGIN_DECLS

static inline int rtapi_parport_data_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_DATA_PORT(t));
}

static inline int rtapi_parport_control_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_CONTROL_PORT(t));
}

static inline int rtapi_parport_status_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_STATUS_PORT(t));
}

static inline unsigned char rtapi_parport_epp_data_readb(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline unsigned long rtapi_parport_epp_data_readl(rtapi_parport_t *t) {
    return rtapi_inl(RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline unsigned long rtapi_parport_ecr_read(rtapi_parport_t *t) {
    return rtapi_inb(RTAPI_PARPORT_ECR_PORT(t));
}


static inline void rtapi_parport_data_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_DATA_PORT(t));
}

static inline void rtapi_parport_control_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_CONTROL_PORT(t));
}

static inline void rtapi_parport_status_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_STATUS_PORT(t));
}

static inline void rtapi_parport_epp_addr_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_EPP_ADDR_PORT(t));
}

static inline void rtapi_parport_epp_data_writeb(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline void rtapi_parport_epp_data_writel(rtapi_parport_t *t, unsigned long v) {
    rtapi_outl(v, RTAPI_PARPORT_EPP_DATA_PORT(t));
}

static inline void rtapi_parport_ecr_write(rtapi_parport_t *t, unsigned char v) {
    rtapi_outb(v, RTAPI_PARPORT_EPP_ADDR_PORT(t));
}

int rtapi_parport_get(const char *mod_name, rtapi_parport_t *port, unsigned short base, unsigned short base_hi, unsigned int modes);
void rtapi_parport_release(rtapi_parport_t *port);

RTAPI_END_DECLS

#endif
