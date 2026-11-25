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
    hal_real_t scale;
    hal_real_t min;
    hal_real_t max;
    hal_real_t accel;
    hal_real_t decel;
    hal_real_t speed_tolerance;
    hal_real_t zero_tolerance;
    hal_real_t offset;
    hal_bool_t select;
} gear_t;
