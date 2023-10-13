#ifndef HALIO_H
#define HALIO_H

#ifndef ULAPI
#define ULAPI
#endif

//! Various c++ includes:
#include <iostream>
#include <list>
#include <vector>

//! Hal:
#include "hal.h"

typedef struct {
    hal_float_t *Pin;
} float_data_t;

typedef struct {
    hal_bit_t *Pin;
} bit_data_t;

typedef struct {
    hal_float_t Pin;
} param_data_t;

typedef struct {
    hal_s32_t *Pin;
} s32_data_t;

typedef struct {
    hal_u32_t *Pin;
} u32_data_t;

extern int comp_id;
extern s32_data_t *streamermeat;
extern float_data_t *J0_Fb;
extern float_data_t *J1_Fb;
extern float_data_t *J2_Fb;
extern float_data_t *J3_Fb;
extern float_data_t *J4_Fb;
extern float_data_t *J5_Fb;

extern float_data_t *CartX_Fb;
extern float_data_t *CartY_Fb;
extern float_data_t *CartZ_Fb;
extern float_data_t *EulerX_Fb;
extern float_data_t *EulerY_Fb;
extern float_data_t *EulerZ_Fb;

extern param_data_t *cart_stepsize;
extern param_data_t *euler_stepsize;
extern param_data_t *euler_maxdegsec;
extern param_data_t *joint_stepsize;
extern param_data_t *joint_maxdegsec;
extern param_data_t *tooldir_stepsize;

extern param_data_t *accmax;
extern param_data_t *velmax;

extern bit_data_t *tool0, *tool1, *tool2;

class halio
{
public:
    int comp_id=0;

    //! Input pins.
    float_data_t *j0;
    float_data_t *j1;
    float_data_t *j2;
    float_data_t *j3;
    float_data_t *j4;
    float_data_t *j5;

    float_data_t *cart_x;
    float_data_t *cart_y;
    float_data_t *cart_z;

    float_data_t *euler_x;
    float_data_t *euler_y;
    float_data_t *euler_z;

    //! Output pins.
    float_data_t *j0_cmd;
    float_data_t *j1_cmd;
    float_data_t *j2_cmd;
    float_data_t *j3_cmd;
    float_data_t *j4_cmd;
    float_data_t *j5_cmd;

    float_data_t *cart_x_cmd;
    float_data_t *cart_y_cmd;
    float_data_t *cart_z_cmd;

    float_data_t *euler_x_cmd;
    float_data_t *euler_y_cmd;
    float_data_t *euler_z_cmd;

    //! Joint origin, offsets.
    param_data_t *j0_x;
    param_data_t *j0_y;
    param_data_t *j0_z;
    param_data_t *j1_x;
    param_data_t *j1_y;
    param_data_t *j1_z;
    param_data_t *j2_x;
    param_data_t *j2_y;
    param_data_t *j2_z;
    param_data_t *j3_x;
    param_data_t *j3_y;
    param_data_t *j3_z;
    param_data_t *j4_x;
    param_data_t *j4_y;
    param_data_t *j4_z;
    param_data_t *j5_x;
    param_data_t *j5_y;
    param_data_t *j5_z;

    param_data_t *cart_stepsize;
    param_data_t *euler_stepsize;
    param_data_t *euler_maxdegsec;
    param_data_t *joint_stepsize;
    param_data_t *joint_maxdegsec;
    param_data_t *tooldir_stepsize;
    param_data_t *accmax;
    param_data_t *velmax;

    s32_data_t *streamermeat;

    bit_data_t *tool0, *tool1, *tool2;
    bit_data_t *mode_fk;

    int init();
    void close();
};
#endif
