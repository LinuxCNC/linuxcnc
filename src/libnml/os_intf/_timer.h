#ifndef _TIMER_H
#define _TIMER_H

/* Useful time routines */

#ifdef __cplusplus
extern "C" {
#endif
/* clock tick resolution, in seconds */
    extern double clk_tck(void);
/* number of seconds from standard epoch, to clock tick resolution */
    extern double etime(void);
/* sleeps # of seconds, to clock tick resolution */
    extern void esleep(double secs);
    void start_timer_server(int priority, int sem_id);
    void kill_timer_server(void);
    extern void print_etime(void);

#ifdef __cplusplus
}
#endif
extern int etime_disabled;
extern double etime_disable_time;
extern int esleep_use_yield;

#endif
