/********************************************************************
* Description:  spindle.h
*               Typedefs for spindle HAL component.
*
* Author: Les Newell <les at sheetcam dot com>
* License: GPL Version 2 or later
*    
* Copyright (c) 2009 All rights reserved.
*
********************************************************************/

typedef struct {
    hal_float_t *scale;
    hal_float_t *min;
    hal_float_t *max;
    hal_float_t *accel;
    hal_float_t *decel;
    hal_float_t *speed_tolerance;
    hal_float_t *zero_tolerance;
    hal_float_t *offset;
    hal_bit_t *select;
} gear_t;
