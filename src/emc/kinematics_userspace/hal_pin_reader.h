/********************************************************************
 * Description: hal_pin_reader.h
 *   Simple HAL pin reader for userspace kinematics parameter refresh.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#ifndef HAL_PIN_READER_H
#define HAL_PIN_READER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read a HAL float pin by name.
 *
 * @param name Full pin name (e.g., "5axiskins.pivot-length")
 * @param value Pointer to store the pin value
 * @return 0 on success, -1 if pin not found or wrong type
 */
int hal_pin_reader_read_float(const char *name, double *value);

/**
 * Read a HAL bit pin by name.
 *
 * @param name Full pin name (e.g., "maxkins.conventional-directions")
 * @param value Pointer to store the pin value (0 or 1)
 * @return 0 on success, -1 if pin not found or wrong type
 */
int hal_pin_reader_read_bit(const char *name, int *value);

/**
 * Read a HAL s32 pin by name.
 *
 * @param name Full pin name
 * @param value Pointer to store the pin value
 * @return 0 on success, -1 if pin not found or wrong type
 */
int hal_pin_reader_read_s32(const char *name, int *value);

#ifdef __cplusplus
}
#endif

#endif /* HAL_PIN_READER_H */
