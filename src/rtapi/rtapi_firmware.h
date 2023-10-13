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
#ifndef RTAPI_FIRMWARE_H
#define RTAPI_FIRMWARE_H

#ifdef __KERNEL__
#include <linux/firmware.h>

// types
#define rtapi_firmware firmware
#ifndef rtapi_device // could be defined in first-included rtapi_device.h
#define rtapi_device device
#endif

// functions
#define rtapi_request_firmware request_firmware
#define rtapi_release_firmware release_firmware

#else
#include <rtapi.h>

RTAPI_BEGIN_DECLS

#include <rtapi_stdint.h>
#include <stddef.h>

struct rtapi_device;
struct rtapi_firmware {
    size_t size;
    const rtapi_u8 *data;
};

extern int rtapi_request_firmware(const struct rtapi_firmware **fw,
    const char *name, struct rtapi_device *device);

extern void rtapi_release_firmware(const struct rtapi_firmware *fw);

RTAPI_END_DECLS

#endif
#endif
