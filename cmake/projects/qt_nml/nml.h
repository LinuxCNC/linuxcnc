#ifndef NML_H
#define NML_H

#include <config.h>

#include <emc.hh>
#include <emc_nml.hh>
#include <string.h>
#include <iostream>

//! For info about nml usage visit : ~/linuxcnc/cmake/halui_src/halui.cc
class nml {
public:
    nml(){}

    struct status {
        bool estop=0;
        bool machine_on=0;
        char task_active_gcodes_string[256];
        char task_active_mcodes_string[256];
        char task_active_fcodes_string[256];
        char task_active_scodes_string[256];
        //! 0=manual 1=mdi 2=auto.
        int mode;
        //! Coordinates.
        float x=0, y=0, z=0, a=0, b=0, c=0, u=0, v=0, w=0;
        //! Feed override, jog override.
        float feed_override=0;
        float rapid_override=0;
        float spindle_override=0;
        float max_velocity=0;
        float current_velocity=0;
        float current_rpm=0;
        float adatpive_feed=0;
        //! Extra.
        int spindle_direction=0;
        bool spindle_brake=0;
        int spindle_increasing=0;
        bool spindle_enabled=0;
        bool spindle_homed=0;
        float spindle_scale=0;
        float lube_level=0;
        bool coolant=0;
        bool mist=0;
        bool lube=0;
        //! Gcode.
        int current_line=0;
        int motion_line=0;

        char command[256];
        char inifile[256];
        char file[256];

        bool homed_x=0, homed_y=0, homed_z=0, homed_all=0;
        bool run=0, pause=0, stop=0, idle=0;

        bool adaptive_feed_enabled=0;

    } theStatus;

    void update(){
        for(int i=0; i<1; i++){
            usleep(1);
            if(!stat->valid()) continue;
            if(stat->peek() != EMC_STAT_TYPE) continue;
            emcStatus = static_cast<EMC_STAT*>(stat->get_address());
        }

        if(emcStatus->task.state== EMC_TASK_STATE_ENUM::EMC_TASK_STATE_ESTOP){
            theStatus.estop=true;
        } else {
            theStatus.estop=false;
        }

        if(emcStatus->task.state== EMC_TASK_STATE_ENUM::EMC_TASK_STATE_ON){
            theStatus.machine_on=true;
        } else {
            theStatus.machine_on=false;
        }

        if(emcStatus->task.mode==EMC_TASK_MODE_ENUM::EMC_TASK_MODE_MANUAL){
            theStatus.mode=0;
        }
        if(emcStatus->task.mode==EMC_TASK_MODE_ENUM::EMC_TASK_MODE_MDI){
            theStatus.mode=1;
        }
        if(emcStatus->task.mode==EMC_TASK_MODE_ENUM::EMC_TASK_MODE_AUTO){
            theStatus.mode=2;
        }

        char string[256];
        int t;
        int code;

        //! Active G codes
        theStatus.task_active_gcodes_string[0] = 0;
        for (t = 1; t < ACTIVE_G_CODES; t++) {
            code = emcStatus->task.activeGCodes[t];
            if (code == -1) {
                continue;
            }
            if (code % 10) {
                sprintf(string, "G%.1f ", (double) code / 10.0);
            } else {
                sprintf(string, "G%d ", code / 10);
            }
            strcat(theStatus.task_active_gcodes_string, string);
        }

        //! Active M codes.
        theStatus.task_active_mcodes_string[0] = 0;
        for (t = 1; t < ACTIVE_M_CODES; t++) {
            code = emcStatus->task.activeMCodes[t];
            if (code == -1) {
                continue;
            }
            sprintf(string, "M%d ", code);
            strcat(theStatus.task_active_mcodes_string, string);
        }

        //! Active F and S codes.
        sprintf(string, "F%.0f ", emcStatus->task.activeSettings[1]);
        theStatus.task_active_fcodes_string[0] = 0;
        strcat(theStatus.task_active_fcodes_string, string);
        sprintf(string, "S%.0f", abs(emcStatus->task.activeSettings[2]));
        theStatus.task_active_scodes_string[0] = 0;
        strcat(theStatus.task_active_scodes_string, string);

        theStatus.x=emcStatus->motion.traj.position.tran.x;
        theStatus.y=emcStatus->motion.traj.position.tran.y;
        theStatus.z=emcStatus->motion.traj.position.tran.z;
        theStatus.a=emcStatus->motion.traj.position.a;
        theStatus.b=emcStatus->motion.traj.position.b;
        theStatus.c=emcStatus->motion.traj.position.c;
        theStatus.u=emcStatus->motion.traj.position.u;
        theStatus.v=emcStatus->motion.traj.position.v;
        theStatus.w=emcStatus->motion.traj.position.w;

        theStatus.feed_override=emcStatus->motion.traj.scale; //! Velocity override scale.
        theStatus.rapid_override=emcStatus->motion.traj.rapid_scale; //! Rapic override scale.
        theStatus.max_velocity=emcStatus->motion.traj.maxVelocity*60;
        theStatus.current_velocity=emcStatus->motion.traj.current_vel*60;
        theStatus.current_rpm=emcStatus->motion.spindle->speed;

        //! Extra
        theStatus.spindle_direction=emcStatus->motion.spindle->direction;
        theStatus.spindle_brake=emcStatus->motion.spindle->brake;
        theStatus.spindle_increasing=emcStatus->motion.spindle->increasing;
        theStatus.spindle_enabled=emcStatus->motion.spindle->enabled;
        theStatus.spindle_homed=emcStatus->motion.spindle->homed;
        theStatus.spindle_scale=emcStatus->motion.spindle->spindle_scale;
        theStatus.lube_level=emcStatus->io.lube.level;
        theStatus.coolant=emcStatus->io.coolant.flood;
        theStatus.mist=emcStatus->io.coolant.mist;
        theStatus.lube=emcStatus->io.lube.on;

        theStatus.current_line=emcStatus->task.currentLine;
        theStatus.motion_line=emcStatus->task.motionLine; //! current executed gcode line

        strcpy(theStatus.command,emcStatus->task.command);
        strcpy(theStatus.inifile,emcStatus->task.ini_filename);
        strcpy(theStatus.file,emcStatus->task.file);

        theStatus.homed_x=emcStatus->motion.joint[0].homed;
        theStatus.homed_y=emcStatus->motion.joint[1].homed;
        theStatus.homed_z=emcStatus->motion.joint[2].homed;

        if(theStatus.homed_x && theStatus.homed_y && theStatus.homed_z){
            theStatus.homed_all=1;
        } else {
            theStatus.homed_all=0;
        }

        theStatus.pause = emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED;
        theStatus.run= emcStatus->task.interpState == EMC_TASK_INTERP_READING ||
                                            emcStatus->task.interpState == EMC_TASK_INTERP_WAITING;
        theStatus.idle= emcStatus->task.interpState == EMC_TASK_INTERP_IDLE;

        theStatus.adaptive_feed_enabled=emcStatus->motion.traj.adaptive_feed_enabled;

        //! Todo : theStatus.adatpive_feed
    }
    void machine_on(){
        EMC_TASK_SET_STATE s;
        s.state=EMC_TASK_STATE_ENUM(EMC_TASK_STATE_ON);
        cmd->write(&s);
    }
    void machine_off(){
        EMC_TASK_SET_STATE s;
        s.state=EMC_TASK_STATE_ENUM(EMC_TASK_STATE_OFF);
        cmd->write(&s);
    }
    void estop_reset(){
        EMC_TASK_SET_STATE s;
        s.state=EMC_TASK_STATE_ENUM(EMC_TASK_STATE_ESTOP_RESET);
        cmd->write(&s);
    }
    void estop(){
        EMC_TASK_SET_STATE s;
        s.state=EMC_TASK_STATE_ENUM(EMC_TASK_STATE_ESTOP);
        cmd->write(&s);
    }
    void teleop(){
        EMC_TRAJ_SET_MODE x;
        x.mode=EMC_TRAJ_MODE_TELEOP;
        cmd->write(&x);
    }
    void coord(){
        EMC_TRAJ_SET_MODE x;
        x.mode=EMC_TRAJ_MODE_COORD;
        cmd->write(&x);
    }
    void free(){
        EMC_TRAJ_SET_MODE x;
        x.mode=EMC_TRAJ_MODE_FREE;
        cmd->write(&x);
    }
    void home_x(){
        EMC_JOINT_HOME m;
        m.joint=0;
        cmd->write(&m);
    }
    void home_y(){
        EMC_JOINT_HOME m;
        m.joint=1;
        cmd->write(&m);
    }
    void home_z(){
        EMC_JOINT_HOME m;
        m.joint=2;
        cmd->write(&m);
    }
    void home_all(){
        EMC_JOINT_HOME m;
        m.joint=-1;
        cmd->write(&m);
    }
    void unhome_x(){
        EMC_JOINT_UNHOME m;
        m.joint=0;
        cmd->write(&m);
    }
    void unhome_y(){
        EMC_JOINT_UNHOME m;
        m.joint=1;
        cmd->write(&m);
    }
    void unhome_z(){
        EMC_JOINT_UNHOME m;
        m.joint=2;
        cmd->write(&m);
    }
    void unhome_all(){
        EMC_JOINT_UNHOME m;
        m.joint=-1;
        cmd->write(&m);
    }
    void mode_auto(){
        EMC_TASK_SET_MODE m;
        m.mode=EMC_TASK_MODE_AUTO;
        cmd->write(&m);
    }
    void mode_mdi(){
        EMC_TASK_SET_MODE m;
        m.mode=EMC_TASK_MODE_MDI;
        cmd->write(&m);
    }
    void mode_manual(){
        EMC_TASK_SET_MODE m;
        m.mode=EMC_TASK_MODE_MANUAL;
        cmd->write(&m);
    }
    void run(int theLine){
        EMC_TASK_PLAN_RUN r;
        r.line=theLine;
        cmd->write(&r);
    }
    void run_step(){
        EMC_TASK_PLAN_STEP r;
        cmd->write(&r);
    }
    void pause(){
        EMC_TASK_PLAN_PAUSE p;
        cmd->write(&p);
    }
    void resume(){
        EMC_TASK_PLAN_RESUME r;
        cmd->write(&r);
    }
    void reverse(){
        EMC_TASK_PLAN_REVERSE r;
        cmd->write(&r);
    }
    void forward(){
        EMC_TASK_PLAN_FORWARD f;
        cmd->write(&f);
    }
    void end(){
        EMC_TASK_PLAN_END e;
        cmd->write(&e);
    }
    void stop(){
        EMC_TASK_ABORT e;
        cmd->write(&e);
    }
    void load(std::string theFile){
        //! Mention, the drawing will not be displayed by a gui like axis. But the code is
        //! loaded and will run.

        EMC_TASK_PLAN_CLOSE m0;
        cmd->write(&m0);

        EMC_TASK_PLAN_OPEN o;
        if(theFile.size()==0){
            std::string str; //=EMC2_NCFILES_DIR; //!  "~/linuxcnc/nc_files"
            str.append("/test.ngc");
            strcpy(o.file, str.c_str());
        } else {
            strcpy(o.file, theFile.c_str());
        }
        cmd->write(&o);
    }
    void spindle_off(int theSpindle){
        EMC_SPINDLE_OFF m;
        m.spindle=theSpindle;
        cmd->write(&m);
    }
    void spindle_on(int theSpindle, float theRpm){
        EMC_SPINDLE_ON m;
        m.spindle=theSpindle;
        m.speed=theRpm;
        cmd->write(&m);
    }
    void jog(int axis, float speed){
        EMC_JOG_CONT j;
        j.jjogmode = 0;
        j.joint_or_axis = axis;
        j.vel = speed/60;
        cmd->write(&j);
    }
    void jog_stop(int axis){
        EMC_JOG_STOP s;
        s.jjogmode = 0;
        s.joint_or_axis=axis;
        cmd->write(&s);
    }
    //! Factor 1.0 - ...
    void setFeedOveride(float theScale){
        EMC_TRAJ_SET_SCALE o;
        o.scale=theScale;
        cmd->write(&o);
    }
    void setMaxVelocity(float theVelocity){
        EMC_TRAJ_SET_MAX_VELOCITY o;
        o.velocity=theVelocity/60;
        cmd->write(&o);
    }
    void setRapidOverride(float theScale){
        EMC_TRAJ_SET_RAPID_SCALE o;
        o.scale=theScale;
        cmd->write(&o);
    }
    void setSpindleOverride(int theSpindle, int theScale){
        EMC_TRAJ_SET_SPINDLE_SCALE s;
        s.spindle=theSpindle;
        s.scale=theScale;
        cmd->write(&s);
    }
    void mdi(std::string theCommand){
        EMC_TASK_PLAN_EXECUTE mdi;
        strcpy(mdi.command,theCommand.c_str());
        cmd->write(&mdi);
    }

private:
    RCS_CMD_CHANNEL *cmd = new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", EMC2_DEFAULT_NMLFILE);
    RCS_STAT_CHANNEL *stat = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", EMC2_DEFAULT_NMLFILE);
    EMC_STAT  *emcStatus;
};
#endif



















