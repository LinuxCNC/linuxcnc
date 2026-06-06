/* rtapi_firmware.h - Firmware loading for LinuxCNC cmod drivers
 *
 * Copyright (C) 2014-2026 Jeff Epler <jepler@unpythonic.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */
#ifndef RTAPI_FIRMWARE_H
#define RTAPI_FIRMWARE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtapi_firmware {
    size_t size;
    const uint8_t *data;
};

/* device parameter is unused, kept for API compat */
struct rtapi_device {
    char name[256];
};

int rtapi_request_firmware(const struct rtapi_firmware **fw,
    const char *name, struct rtapi_device *device);

void rtapi_release_firmware(const struct rtapi_firmware *fw);

#ifdef __cplusplus
}
#endif

#endif /* RTAPI_FIRMWARE_H */
