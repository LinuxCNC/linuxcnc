
#include "rtapi.h"
#include "rtapi_ctype.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "rtapi_math64.h"
#include <rtapi_io.h>
#include "hal.h"
#include "stdio.h"

#include "sc_struct.h"
#include "sc_extern.h"
#include "sc_block.h"
#include "sc_conversion.h"

#include "emcpose.h"
#include "motion.h"
#include "tc.h"

/*

To run this :
~/linuxcnc/cmake/components/sc_tp/$ ./runtest

Todo:
1. When program start's out off position, run to start position.


*/

/* module information */
MODULE_AUTHOR("Skynet_Cyberdyne");
MODULE_DESCRIPTION("sc_tp");
MODULE_LICENSE("GPL2");

static int comp_idx;

typedef struct {
    bool ok;
} skynet_t;
skynet_t *skynet;

typedef struct {
    hal_float_t *Pin;
} float_data_t;

//! Pins
typedef struct {
    hal_bit_t *Pin;
} bit_data_t;
bit_data_t
*module,
*state_stop,
*state_run,
*state_wait,
*state_finished,
*done_out_bit;

typedef struct { //! Int.
    hal_s32_t *Pin;
} s32_data_t;
s32_data_t
*tp_gcode_buffer_size,
*tp_gcode_loaded_lines;

typedef struct { //! Param int.
    hal_s32_t Pin;
} param_s32_data_t;
param_s32_data_t
*pvec_rw_i,
*tp_current_gcode_line;

typedef struct { //! Uint.
    hal_u32_t *Pin;
} u32_data_t;

typedef struct { //! Param Uint.
    hal_u32_t Pin;
} param_u32_data_t;

typedef struct {
    hal_port_t *Pin;
} port_data_t;
port_data_t *port;

//! Params
typedef struct {
    hal_float_t Pin;
} param_float_data_t;
param_float_data_t
*test_endvel_rw_f,
*traject_progress_rw_f,
*runtimer_rw_f, //! Runtimer.
*jm_rw_f,       //! Jerk max.
*af_rw_f;       //! Adaptive feed 0-1.

typedef struct {
    hal_bit_t Pin;
} param_bit_data_t;
param_bit_data_t
*done_out_rw,
*done,
*tp_gcode_print,
*tp_gcode_optimize,
*tp_run;

static int comp_idx; /* component ID */

static void the_function();
static int setup_pins();

int rtapi_app_main(void) {

    int r = 0;
    comp_idx = hal_init("sc_tp");
    if(comp_idx < 0) return comp_idx;
    r = hal_export_funct("sc-tp", the_function, &skynet,0,0,comp_idx);

    r+=setup_pins();

    if(r) {
        hal_exit(comp_idx);
    } else {
        hal_ready(comp_idx);
    }
    return 0;
}

void rtapi_app_exit(void){
    hal_exit(comp_idx);
}

//! Perforn's every ms.
static void the_function(){
    *module->Pin=1;
}

static int setup_pins(){
    int r=0;

    //! Output pins, type bit.
    module = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("sc-tp.module",HAL_OUT,&(module->Pin),comp_idx);

    state_run = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("sc-tp.state_run",HAL_OUT,&(state_run->Pin),comp_idx);

    state_stop = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("sc-tp.state_stop",HAL_OUT,&(state_stop->Pin),comp_idx);

    state_wait = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("sc-tp.state_wait",HAL_OUT,&(state_wait->Pin),comp_idx);

    state_finished = (bit_data_t*)hal_malloc(sizeof(bit_data_t));
    r+=hal_pin_bit_new("sc-tp.state_finished",HAL_OUT,&(state_finished->Pin),comp_idx);

    //! Parameter bit.
    done = (param_bit_data_t*)hal_malloc(sizeof(param_bit_data_t));
    r+=hal_param_bit_new("sc-tp.done",HAL_RW,&(done->Pin),comp_idx);

    tp_gcode_print = (param_bit_data_t*)hal_malloc(sizeof(param_bit_data_t));
    r+=hal_param_bit_new("sc-tp.print",HAL_RW,&(tp_gcode_print->Pin),comp_idx);

    tp_gcode_optimize = (param_bit_data_t*)hal_malloc(sizeof(param_bit_data_t));
    r+=hal_param_bit_new("sc-tp.optimize",HAL_RW,&(tp_gcode_optimize->Pin),comp_idx);

    tp_run = (param_bit_data_t*)hal_malloc(sizeof(param_bit_data_t));
    r+=hal_param_bit_new("sc-tp.run",HAL_RW,&(tp_run->Pin),comp_idx);


    done_out_rw = (param_bit_data_t*)hal_malloc(sizeof(param_bit_data_t));
    r+=hal_param_bit_new("sc-tp.done_out_rw",HAL_RW,&(done_out_rw->Pin),comp_idx);



    //! Parameter float.
    jm_rw_f = (param_float_data_t*)hal_malloc(sizeof(param_float_data_t));
    r+=hal_param_float_new("sc-tp.jm",HAL_RW,&(jm_rw_f->Pin),comp_idx);

    af_rw_f = (param_float_data_t*)hal_malloc(sizeof(param_float_data_t));
    r+=hal_param_float_new("sc-tp.adaptive_feed",HAL_RW,&(af_rw_f->Pin),comp_idx);

    runtimer_rw_f = (param_float_data_t*)hal_malloc(sizeof(param_float_data_t));
    r+=hal_param_float_new("sc-tp.runtimer",HAL_RW,&(runtimer_rw_f->Pin),comp_idx);

    traject_progress_rw_f = (param_float_data_t*)hal_malloc(sizeof(param_float_data_t));
    r+=hal_param_float_new("sc-tp.traject_progress_rw_f",HAL_RW,&(traject_progress_rw_f->Pin),comp_idx);

    test_endvel_rw_f = (param_float_data_t*)hal_malloc(sizeof(param_float_data_t));
    r+=hal_param_float_new("sc-tp.test_endvel_rw_f",HAL_RW,&(test_endvel_rw_f->Pin),comp_idx);


    //! Integer pins.
    tp_gcode_buffer_size= (s32_data_t*)hal_malloc(sizeof(s32_data_t));
    r+=hal_pin_s32_new("sc-tp.buffer_size",HAL_OUT,&(tp_gcode_buffer_size->Pin),comp_idx);

    tp_gcode_loaded_lines= (s32_data_t*)hal_malloc(sizeof(s32_data_t));
    r+=hal_pin_s32_new("sc-tp.loaded_gcode_lines",HAL_OUT,&(tp_gcode_loaded_lines->Pin),comp_idx);

    //! Integer parameters.
    tp_current_gcode_line = (param_s32_data_t*)hal_malloc(sizeof(param_s32_data_t));
    r+=hal_param_s32_new("sc-tp.gcode_current_line",HAL_RW,&(tp_current_gcode_line->Pin),comp_idx);

    pvec_rw_i = (param_s32_data_t*)hal_malloc(sizeof(param_s32_data_t));
    r+=hal_param_s32_new("sc-tp.pvec_size",HAL_RW,&(pvec_rw_i->Pin),comp_idx);

    return r;
}


//! Status and config from motion.h
static emcmot_status_t *emcmotStatus;
static emcmot_config_t *emcmotConfig;

//==========================================================
// tp module interface
// motmod function ptrs for functions called by tp:
static void(  *_DioWrite)(int,char);
static void(  *_AioWrite)(int,double);
static void(  *_SetRotaryUnlock)(int,int);
static int (  *_GetRotaryIsUnlocked)(int);
static double(*_axis_get_vel_limit)(int);
static double(*_axis_get_acc_limit)(int);

void tpMotFunctions(void(  *pDioWrite)(int,char)
                    ,void(  *pAioWrite)(int,double)
                    ,void(  *pSetRotaryUnlock)(int,int)
                    ,int (  *pGetRotaryIsUnlocked)(int)
                    ,double(*paxis_get_vel_limit)(int)
                    ,double(*paxis_get_acc_limit)(int)
                    )
{
    _DioWrite            = *pDioWrite;
    _AioWrite            = *pAioWrite;
    _SetRotaryUnlock     = *pSetRotaryUnlock;
    _GetRotaryIsUnlocked = *pGetRotaryIsUnlocked;
    _axis_get_vel_limit  = *paxis_get_vel_limit;
    _axis_get_acc_limit  = *paxis_get_acc_limit;
}

void tpMotData(emcmot_status_t *pstatus
               ,emcmot_config_t *pconfig
               )
{
    emcmotStatus = pstatus;
    emcmotConfig = pconfig;
}

//! Example how to get values :
//! printf("adaptive feed scale, feed overide.. : %f \n",emcmotStatus->net_feed_scale);
//! printf("emc config max feed scale: %f \n",emcmotConfig->maxFeedScale);

V update_gui();
V ruckig_update_hal_state_pins();
T old_a, old_jm=0, old_vm=0, old_fo=0, old_ro=0, old_af=0;
B check_user_interupt();
V update_ruckig_cycle();

//! Holds lcnc recieved values.
T vm=15, a=15, af=1, jm=10, fo=1, ro=1;
B reverse=0;
V update_pins_params();

//! Skynet.
struct EmcPose tp_current_emc_pose;
struct EmcPose tp_gcode_last_pose;

//! Current 3d position.
struct sc_pnt xyz;
struct sc_dir abc;
struct sc_ext uvw;

//! Gcode vector dynamic.
struct sc_vector *vector_ptr;
int vector_exec_nr=0;

//! Lcnc, for addLine, addArc.
int gcode_last_loaded_line_nr;

//! Ruckig stuff.
bool ruckig_init=0;
sc_ruckig *ruckig_ptr;
enum sc_ruckig_state {
    sc_ruckig_none,
    sc_ruckig_finished,
    sc_ruckig_run,
    sc_ruckig_stop,
    sc_ruckig_pause,
    sc_ruckig_pause_resume,
    sc_ruckig_wait
} state;
I id=0; //! Interpolated id.

T curpos=0, curvel=0, curacc=0;
T traject_progress=0;
T tarpos=0;

I delay=0; //! Cycle delay.
B new_calc=false;
I old_size=0;
B optimized=0;
B init_motion=0;

//! Mdi cycle.
V abort_cycle(){

    vector_clear(vector_ptr);
    ruckig_set_mempos(ruckig_ptr,0);
    vector_exec_nr=0;
    tarpos=0;
    init_motion=0;
    old_size=0;
}

V runcycle(){

    if(vector_size(vector_ptr)!=old_size){
        vector_optimize_gcode(vector_ptr);
        old_size=vector_size(vector_ptr);
        printf("optimized gcode for items: %d \n",vector_size(vector_ptr));


    }

    if(!init_motion && vector_optimized_size(vector_ptr)>0 && ruckig_state_finished(ruckig_ptr)){
        tarpos+=vector_optimized_at(vector_ptr,vector_exec_nr);
        ruckig_set_all_and_run(ruckig_ptr, 0.001, a, jm, curpos, curvel, curacc, tarpos, 0 /*test_endvel_rw_f->Pin*/, 0, vm, fo, af);
        printf("init motion. \n");
        init_motion=1;
    }

    if(init_motion && vector_optimized_size(vector_ptr)>0 && ruckig_state_finished(ruckig_ptr)){
        if(vector_exec_nr<vector_optimized_size(vector_ptr)-1){
            vector_exec_nr++;
            printf("next motion. \n");
            init_motion=0;
        }
        if(vector_exec_nr==vector_optimized_size(vector_ptr)-1){
            printf("finished queue. \n");
            done->Pin=1;
        }
    }




}

int tpRunCycle(TP_STRUCT * const tp, long period)
{
    //! Program flow :

    //! 1. Update user iputs.
    update_pins_params();

    //! 2. Set ruckig calculation flag if something changed.
    check_user_interupt();

    //! 3. Update ruckig cycle.
    update_ruckig_cycle();

    if(test_endvel_rw_f->Pin>vm){ //! Limit ve to vm.
        test_endvel_rw_f->Pin=vm;
    }

    //! 3.1 If new_calc flag, user interupt is true. Update ruckig.
    // if(new_calc && program){
    //
    //     ruckig_set_all_and_run(ruckig_ptr, 0.001, a, jm, curpos, curvel, curacc, tarpos, 0 /*test_endvel_rw_f->Pin*/, 0, vm, fo, af);
    //     new_calc=0;
    // }

    runcycle();

    //! 6. Update gui.
    update_gui();

    return 0;
}

int tpCreate(TP_STRUCT * const tp, int _queueSize,int id)
{
    vector_ptr=vector_init_ptr();

    ruckig_ptr=ruckig_init_ptr();
    ruckig_run(ruckig_ptr); //! To get ruckig in run state, then jumps to finish state.

    if(jm_rw_f->Pin==0){
        rtapi_print_msg(RTAPI_MSG_ERR,"jerk max set to 15. \n");
        jm_rw_f->Pin=15;
    }
    if(af_rw_f->Pin==0){
        rtapi_print_msg(RTAPI_MSG_ERR,"adaptive feed set to 1. \n");
        af_rw_f->Pin=1;
    }

    return 0;
}

int tpClear(TP_STRUCT * const tp)
{
    printf("clear. \n");
    done->Pin = 1;
    return 0;
}

int tpInit(TP_STRUCT * const tp)
{
    return 0;
}

int tpSetCycleTime(TP_STRUCT * const tp, double secs)
{
    if(secs!=0.001){ //! Only check the cycle time. This time is fixed compiled in Ruckig.
        rtapi_print_msg(RTAPI_MSG_ERR,"cycle time != 0.001 sec.");
    }
    return 0;
}

int tpSetVmax(TP_STRUCT * const tp, double vMax, double ini_maxvel)
{
    return 0;
}

int tpSetVlimit(TP_STRUCT * const tp, double vLimit)
{
    vm=vLimit;
    return 0;
}

int tpSetAmax(TP_STRUCT * const tp, double aMax)
{
    a=aMax;
    return 0;
}

int tpSetId(TP_STRUCT * const tp, int id)
{
    //! Sets gcode line nr id for upcoming new line, arc.
    gcode_last_loaded_line_nr=id;
    return 0;
}

int tpGetExecId(TP_STRUCT * const tp)
{
    //! Using pvec:
    T nr=0;
    if(vector_size(vector_ptr)>0){
        nr=vector_at(vector_ptr,id).gcode_line_nr;
        tp_current_gcode_line->Pin=nr;
    }
    return nr;
}

int tpSetTermCond(TP_STRUCT * const tp, int cond, double tolerance)
{
    return 0;
}

int tpSetPos(TP_STRUCT * const tp, EmcPose const * const pos)
{
    //! For loading the gcode line by line, the startpoint of the
    //! first gcode line is *pos. From there the sequence is updated.
    tp_gcode_last_pose=*pos;
    return 0;
}

int tpSetCurrentPos(TP_STRUCT * const tp, EmcPose const * const pos)
{
    printf("tpSetCurrentPos. \n");
    tp_current_emc_pose=*pos;
    return 0;
}

int tpAddCurrentPos(TP_STRUCT * const tp, EmcPose const * const disp)
{
    return 0;
}

int tpErrorCheck(TP_STRUCT const * const tp) {

    return 0;
}

int tpSetSpindleSync(TP_STRUCT * const tp, int spindle, double sync, int mode) {
    return 0;
}

int tpPause(TP_STRUCT * const tp)
{
    if(vector_size(vector_ptr)>0){
        ruckig_pause(ruckig_ptr);
    }
    return 0;
}

int tpResume(TP_STRUCT * const tp)
{
    if(vector_size(vector_ptr)>0){
        ruckig_pause_resume(ruckig_ptr);
    }
    return 0;
}

int tpAbort(TP_STRUCT * const tp)
{
    if(vector_size(vector_ptr)>0){
        ruckig_stop(ruckig_ptr);
    }

    abort_cycle();

    //! Todo : controlled stop, then tp done = 1.
    printf("abort \n");
    done->Pin = 1;
    return 0;
}

int tpGetMotionType(TP_STRUCT * const tp)
{
    return tp->motionType;
}

int tpGetPos(TP_STRUCT const * const tp, EmcPose * const pos)
{
    *pos=tp_current_emc_pose;
    return 0;
}

int tpIsDone(TP_STRUCT * const tp)
{
    return done->Pin;
}

int tpQueueDepth(TP_STRUCT * const tp)
{
    return 0;
}

int tpActiveDepth(TP_STRUCT * const tp)
{
    return 0;
}

int tpSetAout(TP_STRUCT * const tp, unsigned char index, double start, double end) {
    return 0;
}

int tpSetDout(TP_STRUCT * const tp, int index, unsigned char start, unsigned char end) {
    return 0;
}

int tpSetRunDir(TP_STRUCT * const tp, tc_direction_t dir)
{
    return 0;
}

int tpAddRigidTap(TP_STRUCT * const tp,
                  EmcPose end,
                  double vel,
                  double ini_maxvel,
                  double acc,
                  unsigned char enables,
                  double scale,
                  struct state_tag_t tag) {

    printf("tpAddRigidTap \n");
    done->Pin=0;
    return 0;
}

int tpAddLine(TP_STRUCT *
              const tp,
              EmcPose end,
              int canon_motion_type,
              double vel,
              double ini_maxvel,
              double acc,
              unsigned char enables,
              char atspeed,
              int indexer_jnum,
              struct state_tag_t tag)


{
    // printf("tpAddLine \n");
    if(vector_size(vector_ptr)==0){
        tp_gcode_last_pose=tp_current_emc_pose;
    }

    // A way to store gcode into a pvec.
    struct sc_block b;
    b.primitive_id=sc_line;
    b.type=canon_motion_type;
    b.pnt_s=emc_pose_to_sc_pnt(tp_gcode_last_pose);
    b.pnt_w.x=0;
    b.pnt_w.y=0;
    b.pnt_w.z=0;
    b.pnt_c.x=0;
    b.pnt_c.y=0;
    b.pnt_c.z=0;
    b.angle_end_deg=0;

    b.pnt_e=emc_pose_to_sc_pnt(end);

    b.dir_s=emc_pose_to_sc_dir(tp_gcode_last_pose);
    b.dir_e=emc_pose_to_sc_dir(end);

    b.ext_s=emc_pose_to_sc_ext(tp_gcode_last_pose);
    b.ext_e=emc_pose_to_sc_ext(end);

    b.gcode_line_nr=gcode_last_loaded_line_nr;

    b.vo=0;
    b.vm=0;
    b.ve=0;

    b.path_lenght=line_lenght_c(b.pnt_s,b.pnt_e);

    vector_pushback(vector_ptr,b);

    //! Update last pose to end of gcode block.
    tp_gcode_last_pose=end;

    //! Update loaded items to hal.
    *tp_gcode_loaded_lines->Pin=vector_size(vector_ptr);

    done->Pin=0;

    return 0;
}

int tpAddCircle(TP_STRUCT * const tp,
                EmcPose end,
                PmCartesian center,
                PmCartesian normal,
                int turn,
                int canon_motion_type, //! arc_3->lin_2->GO_1
                double vel,
                double ini_maxvel,
                double acc,
                unsigned char enables,
                char atspeed,
                struct state_tag_t tag)
{
    // printf("tpAddCircle \n");

    if(vector_size(vector_ptr)==0){
        tp_gcode_last_pose=tp_current_emc_pose;
    }

    struct sc_block b;
    b.primitive_id=sc_arc;
    b.type=canon_motion_type;
    b.pnt_s=emc_pose_to_sc_pnt(tp_gcode_last_pose);

    b.dir_s=emc_pose_to_sc_dir(tp_gcode_last_pose);
    b.dir_e=emc_pose_to_sc_dir(end);

    b.ext_s=emc_pose_to_sc_ext(tp_gcode_last_pose);
    b.ext_e=emc_pose_to_sc_ext(end);

    //! Create a 3d arc using waypoint technique.
    sc_arc_get_mid_waypoint_c(emc_pose_to_sc_pnt(tp_gcode_last_pose),
                              emc_cart_to_sc_pnt(center),
                              emc_pose_to_sc_pnt(end),&b.pnt_w);

    b.pnt_e=emc_pose_to_sc_pnt(end);
    b.gcode_line_nr=gcode_last_loaded_line_nr;

    b.vo=0;
    b.vm=0;
    b.ve=0;

    b.path_lenght=arc_lenght_c(b.pnt_s,b.pnt_w,b.pnt_e);

    vector_pushback(vector_ptr,b);

    //! Update last pose to end of gcode block.
    tp_gcode_last_pose=end;

    //! Update loaded items to hal.
    *tp_gcode_loaded_lines->Pin=vector_size(vector_ptr);

    done->Pin=0;

    return 0;
}

inline V update_pins_params(){
    fo=emcmotStatus->net_feed_scale;                //! Feed overide.
    ro=emcmotStatus->rapid_scale;                   //! Rapid overide.
    af=af_rw_f->Pin;                                //! Adaptive feed.
    jm=jm_rw_f->Pin;                                //! Jerk max.
    pvec_rw_i->Pin=vector_size(vector_ptr);         //! Gcode vector size.
    traject_progress_rw_f->Pin=traject_progress;    //! Traject progress.
    runtimer_rw_f->Pin+=0.001;                      //! Control timer.

}

inline V update_ruckig_cycle(){
    ruckig_update(ruckig_ptr,&curpos,&curvel,&curacc);  //! Update cycle every 0.001 sec.
    state=ruckig_get_state(ruckig_ptr);
    ruckig_update_hal_state_pins();                     //! User can see the state in halshow.

}

inline B check_user_interupt(){

    if(a!=old_a || jm!=old_jm || vm!=old_vm || fo!=old_fo || af!=old_af){
        printf("user interupt \n");
        //! This flag is reset after ruckig update is done.
        new_calc=true;
        old_a=a;
        old_jm=jm;
        old_vm=vm;
        old_fo=fo;
        old_ro=ro;
        old_af=af;
        return 1;
    }
    return 0;
}

inline V ruckig_update_hal_state_pins(){
    switch (state) {
    case sc_ruckig_finished:
        *state_run->Pin=false;
        *state_stop->Pin=false;
        *state_wait->Pin=false;
        *state_finished->Pin=true;
        break;
    case sc_ruckig_pause:
        *state_run->Pin=false;
        *state_stop->Pin=true;
        *state_wait->Pin=false;
        *state_finished->Pin=false;
        break;
    case sc_ruckig_pause_resume:
        *state_run->Pin=true;
        *state_stop->Pin=false;
        *state_wait->Pin=false;
        *state_finished->Pin=false;
        break;
    case sc_ruckig_run:
        *state_run->Pin=true;
        *state_stop->Pin=false;
        *state_wait->Pin=false;
        *state_finished->Pin=false;
        break;
    case sc_ruckig_wait:
        *state_run->Pin=false;
        *state_stop->Pin=false;
        *state_wait->Pin=true;
        *state_finished->Pin=false;
        break;
    case sc_ruckig_stop:
        *state_run->Pin=false;
        *state_stop->Pin=true;
        *state_wait->Pin=false;
        *state_finished->Pin=false;
        break;
    case sc_ruckig_none:
        *state_run->Pin=false;
        *state_stop->Pin=false;
        *state_wait->Pin=false;
        *state_finished->Pin=false;
        break;
    default:
        break;
    }
}

inline V update_gui(){

    T traject_lenght=vector_traject_lenght(vector_ptr);
    traject_progress=curpos/traject_lenght;

    if(vector_size(vector_ptr)>0){

        T curve_progress=0;
        id=0;
        vector_interpolate_traject(vector_ptr,traject_progress,traject_lenght,&curve_progress,&id);

        if(vector_at(vector_ptr,id).primitive_id==sc_line){
            interpolate_line_c(vector_at(vector_ptr,id).pnt_s,
                               vector_at(vector_ptr,id).pnt_e,
                               curve_progress,
                               &xyz);
        }
        if(vector_at(vector_ptr,id).primitive_id==sc_arc){
            interpolate_arc_c(vector_at(vector_ptr,id).pnt_s,
                              vector_at(vector_ptr,id).pnt_w,
                              vector_at(vector_ptr,id).pnt_e,
                              curve_progress,
                              &xyz);
        }
        tp_current_emc_pose.tran.x=xyz.x;
        tp_current_emc_pose.tran.y=xyz.y;
        tp_current_emc_pose.tran.z=xyz.z;

        interpolate_dir_c(vector_at(vector_ptr,id).dir_s,
                          vector_at(vector_ptr,id).dir_e,
                          curve_progress,
                          &abc);
        tp_current_emc_pose.a=abc.a;
        tp_current_emc_pose.b=abc.b;
        tp_current_emc_pose.c=abc.c;

        interpolate_ext_c(vector_at(vector_ptr,id).ext_s,
                          vector_at(vector_ptr,id).ext_e,
                          curve_progress,
                          &uvw);
        tp_current_emc_pose.u=uvw.u;
        tp_current_emc_pose.v=uvw.v;
        tp_current_emc_pose.w=uvw.w;

        //! Update emc with some values.
        emcmotConfig->trajCycleTime=0.001;
        emcmotStatus->distance_to_go=traject_lenght-curpos;

        EmcPose pose;
        pose.tran.x=vector_at(vector_ptr,id).pnt_e.x-xyz.x;
        pose.tran.y=vector_at(vector_ptr,id).pnt_e.y-xyz.y;
        pose.tran.z=vector_at(vector_ptr,id).pnt_e.z-xyz.z;
        emcmotStatus->dtg=pose;
        emcmotStatus->current_vel=curvel;
    }
}

void tpToggleDIOs(TC_STRUCT * const tc) {

}

struct state_tag_t tpGetExecTag(TP_STRUCT * const tp)
{

}

//! This function is responsible for long startup delay if return=1.
int tcqFull(TC_QUEUE_STRUCT const * const tcq)
{
    return 0;
}

EXPORT_SYMBOL(tpMotFunctions);
EXPORT_SYMBOL(tpMotData);

EXPORT_SYMBOL(tpAbort);
EXPORT_SYMBOL(tpActiveDepth);
EXPORT_SYMBOL(tpAddCircle);
EXPORT_SYMBOL(tpAddLine);
EXPORT_SYMBOL(tpAddRigidTap);
EXPORT_SYMBOL(tpClear);
EXPORT_SYMBOL(tpCreate);
EXPORT_SYMBOL(tpGetExecId);
EXPORT_SYMBOL(tpGetExecTag);
EXPORT_SYMBOL(tpGetMotionType);
EXPORT_SYMBOL(tpGetPos);
EXPORT_SYMBOL(tpIsDone);
EXPORT_SYMBOL(tpPause);
EXPORT_SYMBOL(tpQueueDepth);
EXPORT_SYMBOL(tpResume);
EXPORT_SYMBOL(tpRunCycle);
EXPORT_SYMBOL(tpSetAmax);
EXPORT_SYMBOL(tpSetAout);
EXPORT_SYMBOL(tpSetCycleTime);
EXPORT_SYMBOL(tpSetDout);
EXPORT_SYMBOL(tpSetId);
EXPORT_SYMBOL(tpSetPos);
EXPORT_SYMBOL(tpSetRunDir);
EXPORT_SYMBOL(tpSetSpindleSync);
EXPORT_SYMBOL(tpSetTermCond);
EXPORT_SYMBOL(tpSetVlimit);
EXPORT_SYMBOL(tpSetVmax);
EXPORT_SYMBOL(tcqFull);






































