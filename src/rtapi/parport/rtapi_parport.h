//    Copyright 2014-2026 Jeff Epler
//
//    Standalone rtapi_parport header — no rtapi.h dependency.
//    For use by librtapi_parport.so and cmod parport drivers.

#ifndef RTAPI_PARPORT_H
#define RTAPI_PARPORT_H

#include <sys/io.h>

// Parport mode flags (from linux/parport.h)
#define PARPORT_MODE_PCSPP      (1<<0)
#define PARPORT_MODE_TRISTATE   (1<<1)
#define PARPORT_MODE_EPP        (1<<2)
#define PARPORT_MODE_ECP        (1<<3)
#define PARPORT_MODE_COMPAT     (1<<4)
#define PARPORT_MODE_DMA        (1<<5)
#define PARPORT_MODE_SAFEININT  (1<<6)

typedef struct rtapi_parport_t {
    unsigned short base;
    unsigned short base_hi;
    int fd;
} rtapi_parport_t;

#ifdef __cplusplus
extern "C" {
#endif

int rtapi_parport_get(const char *mod_name, rtapi_parport_t *port,
                      unsigned short base, unsigned short base_hi,
                      unsigned int modes);
void rtapi_parport_release(rtapi_parport_t *port);

#ifdef __cplusplus
}
#endif

#endif
