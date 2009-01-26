/********************************************************************
* Description: interp_queue.cc
*
* Author: Chris Radek
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2009 All rights reserved.
*
********************************************************************/

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "interp_queue.hh"
#include "interp_internal.hh"

// lathe tools have strange origin points that are not at
// the center of the radius.  This means that the point that
// radius compensation controls (center of radius) is not at
// the tool's origin.  These functions do the necessary
// translation.  Notice tool orientations 0 (mill) and 9, and 
// those with radius 0 (a point) do not need any translation.

static double latheorigin_x(setup_pointer settings, double x) {
    int o = settings->cutter_comp_orientation;
    double r = settings->cutter_comp_radius;
    if(settings->plane != CANON_PLANE_XZ) return x;

    if(o==2 || o==6 || o==1) x -= r;
    if(o==3 || o==8 || o==4) x += r;
    return x;
}

static double latheorigin_z(setup_pointer settings, double z) {
    int o = settings->cutter_comp_orientation;
    double r = settings->cutter_comp_radius;
    if(settings->plane != CANON_PLANE_XZ) return z;

    if(o==2 || o==7 || o==3) z -= r;
    if(o==1 || o==5 || o==4) z += r;
    return z;
}

static double endpoint[2];
static int endpoint_valid = 0;

std::vector<queued_canon>& qc(void) {
    static std::vector<queued_canon> c;
    if(0) printf("len %d\n", c.size());
    return c;
}

void qc_reset(void) {
    qc().clear();
    endpoint_valid = 0;
}

void enqueue_SET_FEED_RATE(double feed) {
    if(qc().empty()) {
        SET_FEED_RATE(feed);
        return;
    }
    queued_canon q;
    q.type = QSET_FEED_RATE;
    q.data.set_feed_rate.feed = feed;
    qc().push_back(q);
}

void enqueue_DWELL(double time) {
    if(qc().empty()) {
        DWELL(time);
        return;
    }
    queued_canon q;
    q.type = QDWELL;
    q.data.dwell.time = time;
    qc().push_back(q);
}

void enqueue_SET_FEED_MODE(int mode) {
    if(qc().empty()) {
        SET_FEED_MODE(mode);
        return;
    }
    queued_canon q;
    q.type = QSET_FEED_MODE;
    q.data.set_feed_mode.mode = mode;
    qc().push_back(q);
}

void enqueue_MIST_ON(void) {
    if(qc().empty()) {
        MIST_ON();
        return;
    }
    queued_canon q;
    q.type = QMIST_ON;
    qc().push_back(q);
}

void enqueue_MIST_OFF(void) {
    if(qc().empty()) {
        MIST_OFF();
        return;
    }
    queued_canon q;
    q.type = QMIST_OFF;
    qc().push_back(q);
}

void enqueue_FLOOD_ON(void) {
    if(qc().empty()) {
        FLOOD_ON();
        return;
    }
    queued_canon q;
    q.type = QFLOOD_ON;
    qc().push_back(q);
}

void enqueue_FLOOD_OFF(void) {
    if(qc().empty()) {
        FLOOD_OFF();
        return;
    }
    queued_canon q;
    q.type = QFLOOD_OFF;
    qc().push_back(q);
}

void enqueue_START_SPINDLE_CLOCKWISE(void) {
    if(qc().empty()) {
        START_SPINDLE_CLOCKWISE();
        return;
    }
    queued_canon q;
    q.type = QSTART_SPINDLE_CLOCKWISE;
    qc().push_back(q);
}

void enqueue_START_SPINDLE_COUNTERCLOCKWISE(void) {
    if(qc().empty()) {
        START_SPINDLE_COUNTERCLOCKWISE();
        return;
    }
    queued_canon q;
    q.type = QSTART_SPINDLE_COUNTERCLOCKWISE;
    qc().push_back(q);
}

void enqueue_STOP_SPINDLE_TURNING(void) {
    if(qc().empty()) {
        STOP_SPINDLE_TURNING();
        return;
    }
    queued_canon q;
    q.type = QSTOP_SPINDLE_TURNING;
    qc().push_back(q);
}

void enqueue_SET_SPINDLE_MODE(double mode) {
    if(qc().empty()) {
        SET_SPINDLE_MODE(mode);
        return;
    }
    queued_canon q;
    q.type = QSET_SPINDLE_MODE;
    q.data.set_spindle_mode.mode = mode;
    qc().push_back(q);
}

void enqueue_SET_SPINDLE_SPEED(double speed) {
    if(qc().empty()) {
        SET_SPINDLE_SPEED(speed);
        return;
    }
    queued_canon q;
    q.type = QSET_SPINDLE_SPEED;
    q.data.set_spindle_speed.speed = speed;
    qc().push_back(q);
}

void enqueue_COMMENT(char *c) {
    if(qc().empty()) {
        COMMENT(c);
        return;
    }
    queued_canon q;
    q.type = QCOMMENT;
    q.data.comment.comment = strdup(c);
    qc().push_back(q);
}

int enqueue_STRAIGHT_FEED(setup_pointer settings, int l, 
                           double dx, double dy, double dz,
                           double x, double y, double z, 
                           double a, double b, double c, 
                           double u, double v, double w) {
    queued_canon q;
    q.type = QSTRAIGHT_FEED;
    q.data.straight_feed.line_number = l;
    switch(settings->plane) {
    case CANON_PLANE_XY:
        q.data.straight_feed.dx = dx;
        q.data.straight_feed.dy = dy;
        q.data.straight_feed.dz = dz;
        q.data.straight_feed.x = x;
        q.data.straight_feed.y = y;
        q.data.straight_feed.z = z;
        break;
    case CANON_PLANE_XZ:
        q.data.straight_feed.dz = dx;
        q.data.straight_feed.dx = dy;
        q.data.straight_feed.dy = dz;
        q.data.straight_feed.z = x;
        q.data.straight_feed.x = y;
        q.data.straight_feed.y = z;
        break;
    default:
        ;
    }        
    q.data.straight_feed.a = a;
    q.data.straight_feed.b = b;
    q.data.straight_feed.c = c;
    q.data.straight_feed.u = u;
    q.data.straight_feed.v = v;
    q.data.straight_feed.w = w;
    qc().push_back(q);
    return 0;
}

int enqueue_STRAIGHT_TRAVERSE(setup_pointer settings, int l, 
                               double dx, double dy, double dz,
                               double x, double y, double z, 
                               double a, double b, double c, 
                               double u, double v, double w) {
    queued_canon q;
    q.type = QSTRAIGHT_TRAVERSE;
    q.data.straight_traverse.line_number = l;
    switch(settings->plane) {
    case CANON_PLANE_XY:
        q.data.straight_traverse.x = x;
        q.data.straight_traverse.y = y;
        q.data.straight_traverse.z = z;
        break;
    case CANON_PLANE_XZ:
        q.data.straight_traverse.z = x;
        q.data.straight_traverse.x = y;
        q.data.straight_traverse.y = z;
        break;
    default:
        ;
    }        
    q.data.straight_traverse.a = a;
    q.data.straight_traverse.b = b;
    q.data.straight_traverse.c = c;
    q.data.straight_traverse.u = u;
    q.data.straight_traverse.v = v;
    q.data.straight_traverse.w = w;
    qc().push_back(q);
    return 0;
}

void enqueue_ARC_FEED(setup_pointer settings, int l, 
                      double original_turns,
                      double end1, double end2, double center1, double center2,
                      int turn,
                      double end3,
                      double a, double b, double c,
                      double u, double v, double w) {
    queued_canon q;

    q.type = QARC_FEED;
    q.data.arc_feed.line_number = l;
    q.data.arc_feed.original_turns = original_turns;
    q.data.arc_feed.end1 = end1;
    q.data.arc_feed.end2 = end2;
    q.data.arc_feed.center1 = center1;
    q.data.arc_feed.center2 = center2;
    q.data.arc_feed.turn = turn;
    q.data.arc_feed.end3 = end3;
    q.data.arc_feed.a = a;
    q.data.arc_feed.b = b;
    q.data.arc_feed.c = c;
    q.data.arc_feed.u = u;
    q.data.arc_feed.v = v;
    q.data.arc_feed.w = w;

    qc().push_back(q);
}

void qc_scale(double scale) {
    
    if(qc().empty()) return;

    for(unsigned int i = 0; i<qc().size(); i++) {
        queued_canon &q = qc()[i];
        endpoint[0] *= scale;
        endpoint[1] *= scale;
        switch(q.type) {
        case QARC_FEED:
            q.data.arc_feed.end1 *= scale;
            q.data.arc_feed.end2 *= scale;
            q.data.arc_feed.end3 *= scale;
            q.data.arc_feed.center1 *= scale;
            q.data.arc_feed.center2 *= scale;
            q.data.arc_feed.u *= scale;
            q.data.arc_feed.v *= scale;
            q.data.arc_feed.w *= scale;
            break;
        case QSTRAIGHT_FEED:
            q.data.straight_feed.x *= scale;
            q.data.straight_feed.y *= scale;
            q.data.straight_feed.z *= scale;
            q.data.straight_feed.u *= scale;
            q.data.straight_feed.v *= scale;
            q.data.straight_feed.w *= scale;
            break;
        case QSTRAIGHT_TRAVERSE:
            q.data.straight_traverse.x *= scale;
            q.data.straight_traverse.y *= scale;
            q.data.straight_traverse.z *= scale;
            q.data.straight_traverse.u *= scale;
            q.data.straight_traverse.v *= scale;
            q.data.straight_traverse.w *= scale;
            break;
        default:
            ;
        }
    }
}

void dequeue_canons(setup_pointer settings) {

    endpoint_valid = 0;

    if(qc().empty()) return;

    for(unsigned int i = 0; i<qc().size(); i++) {
        queued_canon &q = qc()[i];

        switch(q.type) {
        case QARC_FEED:
            ARC_FEED(q.data.arc_feed.line_number, 
                     latheorigin_z(settings, q.data.arc_feed.end1), 
                     latheorigin_x(settings, q.data.arc_feed.end2), 
                     latheorigin_z(settings, q.data.arc_feed.center1),
                     latheorigin_x(settings, q.data.arc_feed.center2), 
                     q.data.arc_feed.turn, 
                     q.data.arc_feed.end3,
                     q.data.arc_feed.a, q.data.arc_feed.b, q.data.arc_feed.c, 
                     q.data.arc_feed.u, q.data.arc_feed.v, q.data.arc_feed.w);
            break;
        case QSTRAIGHT_FEED:
            STRAIGHT_FEED(q.data.straight_feed.line_number, 
                          latheorigin_x(settings, q.data.straight_feed.x), 
                          q.data.straight_feed.y, 
                          latheorigin_z(settings, q.data.straight_feed.z),
                          q.data.straight_feed.a, q.data.straight_feed.b, q.data.straight_feed.c, 
                          q.data.straight_feed.u, q.data.straight_feed.v, q.data.straight_feed.w);
            break;
        case QSTRAIGHT_TRAVERSE:
            STRAIGHT_TRAVERSE(q.data.straight_traverse.line_number, 
                              latheorigin_x(settings, q.data.straight_traverse.x),
                              q.data.straight_traverse.y,
                              latheorigin_z(settings, q.data.straight_traverse.z),
                              q.data.straight_traverse.a, q.data.straight_traverse.b, q.data.straight_traverse.c, 
                              q.data.straight_traverse.u, q.data.straight_traverse.v, q.data.straight_traverse.w);
            break;
        case QSET_FEED_RATE:
            SET_FEED_RATE(q.data.set_feed_rate.feed);
            break;
        case QDWELL:
            DWELL(q.data.dwell.time);
            break;
        case QSET_FEED_MODE:
            SET_FEED_MODE(q.data.set_feed_mode.mode);
            break;
        case QMIST_ON:
            MIST_ON();
            break;
        case QMIST_OFF:
            MIST_OFF();
            break;
        case QFLOOD_ON:
            FLOOD_ON();
            break;
        case QFLOOD_OFF:
            FLOOD_OFF();
            break;
        case QSTART_SPINDLE_CLOCKWISE:
            START_SPINDLE_CLOCKWISE();
            break;
        case QSTART_SPINDLE_COUNTERCLOCKWISE:
            START_SPINDLE_COUNTERCLOCKWISE();
            break;
        case QSTOP_SPINDLE_TURNING:
            STOP_SPINDLE_TURNING();
            break;
        case QSET_SPINDLE_MODE:
            SET_SPINDLE_MODE(q.data.set_spindle_mode.mode);
            break;
        case QSET_SPINDLE_SPEED:
            SET_SPINDLE_SPEED(q.data.set_spindle_speed.speed);
            break;
        case QCOMMENT:
            COMMENT(q.data.comment.comment);
            free(q.data.comment.comment);
            break;
        }
    }
    qc().clear();
}

int Interp::move_endpoint_and_flush(setup_pointer settings, double x, double y) {
    double x1;
    double y1;
    double x2;
    double y2;
    double dot;
    static char name[] = "move_endpoint_and_flush";

    if(qc().empty()) return 0;
    
    for(unsigned int i = 0; i<qc().size(); i++) {
        // there may be several moves in the queue, and we need to
        // change all of them.  consider moving into a concave corner,
        // then up and back down, then continuing on.  there will be
        // three moves to change.

        queued_canon &q = qc()[i];

        switch(q.type) {
        case QARC_FEED:
            double r1, r2, l1, l2;
            r1 = hypot(q.data.arc_feed.end1 - q.data.arc_feed.center1,
                       q.data.arc_feed.end2 - q.data.arc_feed.center2);
            l1 = q.data.arc_feed.original_turns;
            q.data.arc_feed.end1 = x;
            q.data.arc_feed.end2 = y;
            r2 = hypot(q.data.arc_feed.end1 - q.data.arc_feed.center1,
                       q.data.arc_feed.end2 - q.data.arc_feed.center2);
            l2 = find_turn(endpoint[0], endpoint[1],
                           q.data.arc_feed.center1, q.data.arc_feed.center2,
                           q.data.arc_feed.turn,
                           q.data.arc_feed.end1, q.data.arc_feed.end2);
            if(fabs(r1-r2) > .01) 
                ERF((_("BUG: cutter compensation has generated an invalid arc with mismatched radii r1 %f r2 %f\n"), r1, r2));
            if(l1 && fabs(l2) > fabs(l1)) {
                ERF((_("Arc move in concave corner is too short to be reachable by the tool without gouging")));
            }
            break;
        case QSTRAIGHT_TRAVERSE:
            switch(settings->plane) {
            case CANON_PLANE_XY:
                x1 = q.data.straight_traverse.dx; // direction of original motion
                y1 = q.data.straight_traverse.dy;                
                x2 = x - endpoint[0];         // new direction after clipping
                y2 = y - endpoint[1];
                break;
            case CANON_PLANE_XZ:
                x1 = q.data.straight_traverse.dz; // direction of original motion
                y1 = q.data.straight_traverse.dx;                
                x2 = x - endpoint[0];         // new direction after clipping
                y2 = y - endpoint[1];
                break;
            default:
                ERF((_("BUG: Unsupported plane in cutter compensation")));
            }

            dot = x1 * x2 + y1 * y2; // not normalized; we only care about the angle

            if(endpoint_valid && dot<0) {
                // oops, the move is the wrong way.  this means the
                // path has crossed because we backed up further
                // than the line is long.  this will gouge.
                ERF((_("Straight move in concave corner is too short to be reachable by the tool without gouging")));
            }
            switch(settings->plane) {
            case CANON_PLANE_XY:
                q.data.straight_traverse.x = x;
                q.data.straight_traverse.y = y;
                break;
            case CANON_PLANE_XZ:
                q.data.straight_traverse.z = x;
                q.data.straight_traverse.x = y;
                break;
            }
            break;
        case QSTRAIGHT_FEED: 
            switch(settings->plane) {
            case CANON_PLANE_XY:
                x1 = q.data.straight_feed.dx; // direction of original motion
                y1 = q.data.straight_feed.dy;                
                x2 = x - endpoint[0];         // new direction after clipping
                y2 = y - endpoint[1];
                break;
            case CANON_PLANE_XZ:
                x1 = q.data.straight_feed.dz; // direction of original motion
                y1 = q.data.straight_feed.dx;                
                x2 = x - endpoint[0];         // new direction after clipping
                y2 = y - endpoint[1];
                break;
            default:
                ERF((_("BUG: Unsupported plane in cutter compensation")));
            }

            dot = x1 * x2 + y1 * y2;

            if(endpoint_valid && dot<0) {
                // oops, the move is the wrong way.  this means the
                // path has crossed because we backed up further
                // than the line is long.  this will gouge.
                ERF((_("Straight move in concave corner is too short to be reachable by the tool without gouging")));
            }
            switch(settings->plane) {
            case CANON_PLANE_XY:
                q.data.straight_feed.x = x;
                q.data.straight_feed.y = y;
                break;
            case CANON_PLANE_XZ:
                q.data.straight_feed.z = x;
                q.data.straight_feed.x = y;
                break;
            }
            break;
        default:
            // other things are not moves - we don't have to mess with them.
            ;
        }
    }
    dequeue_canons(settings);
    endpoint[0] = x; endpoint[1] = y;
    endpoint_valid = 1;
    return 0;
}

void set_endpoint(double x, double y) {
    endpoint[0] = x; endpoint[1] = y; 
    endpoint_valid = 1;
}
