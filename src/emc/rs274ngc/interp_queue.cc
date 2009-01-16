#include <string.h>
#include <stdlib.h>

#include "rs274ngc.hh"
#include "interp_queue.hh"

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

void enqueue_STRAIGHT_FEED(int l, 
                           double dx, double dy, double dz,
                           double x, double y, double z, 
                           double a, double b, double c, 
                           double u, double v, double w) {
    queued_canon q;
    q.type = QSTRAIGHT_FEED;
    q.data.straight_feed.line_number = l;
    q.data.straight_feed.dx = dx;
    q.data.straight_feed.dy = dy;
    q.data.straight_feed.dz = dz;
    q.data.straight_feed.x = x;
    q.data.straight_feed.y = y;
    q.data.straight_feed.z = z;
    q.data.straight_feed.a = a;
    q.data.straight_feed.b = b;
    q.data.straight_feed.c = c;
    q.data.straight_feed.u = u;
    q.data.straight_feed.v = v;
    q.data.straight_feed.w = w;
    qc().push_back(q);
}

void enqueue_STRAIGHT_TRAVERSE(int l, 
                               double dx, double dy, double dz,
                               double x, double y, double z, 
                               double a, double b, double c, 
                               double u, double v, double w) {
    queued_canon q;
    q.type = QSTRAIGHT_TRAVERSE;
    q.data.straight_traverse.line_number = l;
    q.data.straight_traverse.x = x;
    q.data.straight_traverse.y = y;
    q.data.straight_traverse.z = z;
    q.data.straight_traverse.a = a;
    q.data.straight_traverse.b = b;
    q.data.straight_traverse.c = c;
    q.data.straight_traverse.u = u;
    q.data.straight_traverse.v = v;
    q.data.straight_traverse.w = w;
    qc().push_back(q);
}

void enqueue_ARC_FEED(int l, 
                      double end1, double end2, double center1, double center2,
                      int turn,
                      double end3,
                      double a, double b, double c,
                      double u, double v, double w) {
    queued_canon q;

    q.type = QARC_FEED;
    q.data.arc_feed.line_number = l;
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

void dequeue_canons(void) {

    endpoint_valid = 0;

    if(qc().empty()) return;

    for(unsigned int i = 0; i<qc().size(); i++) {
        queued_canon &q = qc()[i];

        switch(q.type) {
        case QARC_FEED:
            ARC_FEED(q.data.arc_feed.line_number, q.data.arc_feed.end1, q.data.arc_feed.end2, 
                     q.data.arc_feed.center1, q.data.arc_feed.center2, 
                     q.data.arc_feed.turn, 
                     q.data.arc_feed.end3,
                     q.data.arc_feed.a, q.data.arc_feed.b, q.data.arc_feed.c, 
                     q.data.arc_feed.u, q.data.arc_feed.v, q.data.arc_feed.w);
            break;
        case QSTRAIGHT_FEED:
            STRAIGHT_FEED(q.data.straight_feed.line_number, 
                          q.data.straight_feed.x, q.data.straight_feed.y, q.data.straight_feed.z, 
                          q.data.straight_feed.a, q.data.straight_feed.b, q.data.straight_feed.c, 
                          q.data.straight_feed.u, q.data.straight_feed.v, q.data.straight_feed.w);
            break;
        case QSTRAIGHT_TRAVERSE:
            STRAIGHT_TRAVERSE(q.data.straight_traverse.line_number, 
                          q.data.straight_traverse.x, q.data.straight_traverse.y, q.data.straight_traverse.z, 
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

int move_endpoint_and_flush(double x, double y) {
    if(qc().empty()) return 0;
    
    for(unsigned int i = 0; i<qc().size(); i++) {
        queued_canon &q = qc()[i];

        switch(q.type) {
        case QARC_FEED:
            // detect increase of arc length and error
            q.data.arc_feed.end1 = x;
            q.data.arc_feed.end2 = y;
            break;
        case QSTRAIGHT_TRAVERSE:
            q.data.straight_traverse.x = x;
            q.data.straight_traverse.y = y;
            break;
        case QSTRAIGHT_FEED: 
            {
                double x1 = q.data.straight_feed.dx; // direction of original motion
                double y1 = q.data.straight_feed.dy;
                double x2 = x - endpoint[0];         // new direction after clipping
                double y2 = y - endpoint[1];

                double dot = x1 * x2 + y1 * y2; // not normalized
                if(endpoint_valid && dot<0) {
                    // oops, the move is the wrong way.  this means the
                    // path has crossed because we backed up further
                    // than the line is long.  this will gouge.

                    if(0) printf("reversal start G1F66X%gY%g oldend G1F66X%gY%g newend G1F66X%gY%g\n", 
                                 endpoint[0], endpoint[1],
                                 q.data.straight_feed.x, q.data.straight_feed.y,
                                 x, y);
                    return -1;
                } else {
                    if(0) printf("........ start G1F66X%gY%g oldend G1F66X%gY%g newend G1F66X%gY%g\n", 
                                 endpoint[0], endpoint[1],
                                 q.data.straight_feed.x, q.data.straight_feed.y,
                                 x, y);
                }
                q.data.straight_feed.x = x;
                q.data.straight_feed.y = y;
            }
            break;
        default:
            ;
        }
    }
    dequeue_canons();
    endpoint[0] = x; endpoint[1] = y;
    endpoint_valid = 1;
    return 0;
}

int move_endpoint_and_flush_zx(double z, double x) {
    if(qc().empty()) return 0;
    
    for(unsigned int i = 0; i<qc().size(); i++) {
        queued_canon &q = qc()[i];

        switch(q.type) {
        case QARC_FEED:
            q.data.arc_feed.end1 = z;
            q.data.arc_feed.end2 = x;
            break;
        case QSTRAIGHT_TRAVERSE:
            q.data.straight_traverse.z = z;
            q.data.straight_traverse.x = x;
            break;
        case QSTRAIGHT_FEED:
            {
                double z1 = q.data.straight_feed.dz; // see above
                double x1 = q.data.straight_feed.dx;
                double z2 = z - endpoint[0];
                double x2 = x - endpoint[1];

                double dot = z1 * z2 + x1 * x2;
                if(endpoint_valid && dot<0) {
                    if(0) printf("reversal start G1F66Z%gX%g oldend G1F66Z%gX%g newend G1F66Z%gX%g\n", 
                                 endpoint[0], endpoint[1],
                                 q.data.straight_feed.z, q.data.straight_feed.x,
                                 z, x);
                    return -1;
                } else {
                    if(0) printf("........ start G1F66Z%gX%g oldend G1F66Z%gX%g newend G1F66Z%gX%g\n", 
                                 endpoint[0], endpoint[1],
                                 q.data.straight_feed.z, q.data.straight_feed.x,
                                 z, x);
                }
            }
            q.data.straight_feed.z = z;
            q.data.straight_feed.x = x;
            break;
        default:
            ;
        }
    }
    dequeue_canons();
    endpoint[0] = z; endpoint[1] = x;
    endpoint_valid = 1;
    return 0;
}

void set_endpoint(double x, double y) {
    endpoint[0] = x; endpoint[1] = y; 
    endpoint_valid = 1;
}

void set_endpoint_zx(double z, double x) {
    endpoint[0] = z; endpoint[1] = x;
    endpoint_valid = 1;
}

