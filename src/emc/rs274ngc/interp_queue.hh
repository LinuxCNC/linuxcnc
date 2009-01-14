
#include <vector>
typedef enum queued_canon_type {QSTRAIGHT_TRAVERSE, QSTRAIGHT_FEED, QARC_FEED, QSET_FEED_RATE, QDWELL, QSET_FEED_MODE,
                                QMIST_ON, QMIST_OFF, QFLOOD_ON, QFLOOD_OFF,
                                QSTART_SPINDLE_CLOCKWISE, QSTART_SPINDLE_COUNTERCLOCKWISE, QSTOP_SPINDLE_TURNING,
                                QSET_SPINDLE_MODE, QSET_SPINDLE_SPEED,
                                QCOMMENT};

struct straight_traverse {
    int line_number;
    double x,y,z, a,b,c, u,v,w;
};

struct straight_feed {
    int line_number;
    double dx, dy, dz;          // direction of original motion
    double x,y,z, a,b,c, u,v,w; // target
};

struct arc_feed {
    int line_number;
    double end1, end2, center1, center2;
    int turn;
    double end3, a,b,c, u,v,w;
};

struct set_feed_rate {
    double feed;
};

struct set_feed_mode {
    int mode;
};

struct dwell {
    double time;
};

struct set_spindle_mode {
    double mode;
};

struct set_spindle_speed {
    double speed;
};

struct comment {
    char *comment;
};

typedef struct queued_canon {
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
    } data;
};

std::vector<queued_canon>& qc(void);

void enqueue_SET_FEED_RATE(double feed);
void enqueue_DWELL(double time);
void enqueue_SET_FEED_MODE(int mode);
void enqueue_MIST_ON(void);
void enqueue_MIST_OFF(void);
void enqueue_FLOOD_ON(void);
void enqueue_FLOOD_OFF(void);
void enqueue_START_SPINDLE_CLOCKWISE(void);
void enqueue_START_SPINDLE_COUNTERCLOCKWISE(void);
void enqueue_STOP_SPINDLE_TURNING(void);
void enqueue_SET_SPINDLE_MODE(double mode);
void enqueue_SET_SPINDLE_SPEED(double speed);
void enqueue_COMMENT(char *c);
void enqueue_STRAIGHT_FEED(int l, 
                           double dx, double dy, double dz,
                           double x, double y, double z, 
                           double a, double b, double c, 
                           double u, double v, double w);
void enqueue_STRAIGHT_TRAVERSE(int l, 
                               double dx, double dy, double dz,
                               double x, double y, double z, 
                               double a, double b, double c, 
                               double u, double v, double w);
void enqueue_ARC_FEED(int l, 
                      double end1, double end2, double center1, double center2,
                      int turn,
                      double end3,
                      double a, double b, double c,
                      double u, double v, double w);
void dequeue_canons(void);
void set_endpoint(double x, double y);
void set_endpoint_zx(double z, double x);
int move_endpoint_and_flush(double x, double y);
int move_endpoint_and_flush_zx(double z, double x);
void qc_reset(void);
void qc_scale(double scale);
