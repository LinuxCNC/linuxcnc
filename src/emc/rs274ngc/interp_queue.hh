/********************************************************************
* Description: interp_queue.hh
*
* Author: Chris Radek
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2009 All rights reserved.
*
********************************************************************/
#include <vector>

enum queued_canon_type {QSTRAIGHT_TRAVERSE, QSTRAIGHT_FEED, QARC_FEED, QSET_FEED_RATE, QDWELL, QSET_FEED_MODE,
                                QMIST_ON, QMIST_OFF, QFLOOD_ON, QFLOOD_OFF,
                                QSTART_SPINDLE_CLOCKWISE, QSTART_SPINDLE_COUNTERCLOCKWISE, QSTOP_SPINDLE_TURNING,
                                QSET_SPINDLE_MODE, QSET_SPINDLE_SPEED,
			QCOMMENT, QM_USER_COMMAND,QSTART_CHANGE, 
			QORIENT_SPINDLE, QWAIT_ORIENT_SPINDLE_COMPLETE};

struct straight_traverse {
    int line_number;
    double dx, dy, dz;          // direction of original motion
    double x,y,z, a,b,c, u,v,w;
};

struct straight_feed {
    int line_number;
    double dx, dy, dz;          // direction of original motion
    double x,y,z, a,b,c, u,v,w; // target
};

struct arc_feed {
    int line_number;
    double original_turns;
    double end1, end2, center1, center2;
    int turn;
    double end3, a,b,c, u,v,w;
};

struct set_feed_rate {
    double feed;
};

struct set_feed_mode {
	int spindle;
    int mode;
};

struct dwell {
    double time;
};

struct set_spindle_mode {
    int spindle;
    double mode;
};

struct set_spindle_speed {
    int spindle;
    double speed;
};

struct comment {
    char *comment;
};

struct mcommand {
    int    index;
    double p_number;
    double q_number;
};

struct orient_spindle {
    int spindle;
    double orientation;
    int mode;
};

struct wait_orient_spindle_complete {
	int spindle;
    double timeout;
};

struct queued_canon {
    queued_canon_type type;
    union {
        struct straight_traverse straight_traverse;
        struct straight_feed straight_feed;
        struct arc_feed arc_feed;
        struct set_feed_rate set_feed_rate;
        struct dwell dwell;
        struct set_feed_mode set_feed_mode;
        struct set_spindle_mode set_spindle_mode;
        struct set_spindle_speed set_spindle_speed;
        struct comment comment;
        struct mcommand mcommand;
        struct orient_spindle orient_spindle;
        struct wait_orient_spindle_complete wait_orient_spindle_complete;
    } data;
};

std::vector<queued_canon>& qc(void);

void enqueue_SET_FEED_RATE(double feed);
void enqueue_DWELL(double time);
void enqueue_SET_FEED_MODE(int spindle, int mode);
void enqueue_MIST_ON(void);
void enqueue_MIST_OFF(void);
void enqueue_FLOOD_ON(void);
void enqueue_FLOOD_OFF(void);
void enqueue_START_SPINDLE_CLOCKWISE(int spindle);
void enqueue_START_SPINDLE_COUNTERCLOCKWISE(int spinde);
void enqueue_STOP_SPINDLE_TURNING(int spindle);
void enqueue_SET_SPINDLE_MODE(int spindle, double mode);
void enqueue_SET_SPINDLE_SPEED(int spindle, double speed);
void enqueue_COMMENT(const char *c);
int enqueue_STRAIGHT_FEED(setup_pointer settings, int l, 
                          double dx, double dy, double dz,
                          double x, double y, double z, 
                          double a, double b, double c, 
                          double u, double v, double w);
int enqueue_STRAIGHT_TRAVERSE(setup_pointer settings, int l, 
                              double dx, double dy, double dz,
                              double x, double y, double z, 
                              double a, double b, double c, 
                              double u, double v, double w);
void enqueue_ARC_FEED(setup_pointer settings, int l, 
                      double original_arclen,
                      double end1, double end2, double center1, double center2,
                      int turn,
                      double end3,
                      double a, double b, double c,
                      double u, double v, double w);
void enqueue_M_USER_COMMAND(int index,double p_number,double q_number);
void enqueue_START_CHANGE(void);
void enqueue_ORIENT_SPINDLE(int spindle, double orientation, int mode);
void enqueue_WAIT_ORIENT_SPINDLE_COMPLETE(int spindle, double timeout);
void dequeue_canons(setup_pointer settings);
void set_endpoint(double x, double y);
void set_endpoint_zx(double z, double x);
int move_endpoint_and_flush(setup_pointer settings, double x, double y);
void qc_reset(void);
void qc_scale(double scale);
