#ifndef _PAUSE_H
#define _PAUSE_H

// the states of the pause finite state machine
// current state is exported as 'motion.pause-state'
enum pause_state {
    PS_RUNNING=0,  // aka 'not paused', normal execution

    // a pause command was issued and processed by command-handler
    // looking for the first pausable motion (e.g. not spindle-synced)
    // the current position is recorded in initial_pause_position
    PS_PAUSING=1,

    // motion stopped, and position is where the pause command was issued
    // This state is reached only if pause offset is zero.
    // on resume, no further move will be issued
    PS_PAUSED=2,

    // motion stopped, and positon is NOT the initial_pause_position
    // meaning a reentry sequence is needed before a resume (i.e. switch to the
    // primary queue) can be executed
    PS_PAUSED_IN_OFFSET=3,

    // currently honoring a jog command (incremental or continuous)
    // state can be reached only from PS_PAUSED and PS_PAUSED_IN_OFFSET
    // a coord mode motion in progress
    // once done, state transitions to PS_PAUSED_IN_OFFSET
    // (or - unlikely case: if stopped in inital_pause_position, to PS_PAUSED)
    PS_JOGGING=4,

    // a jog abort during jog move was issued. This happens on
    // keyboard jog/key release during a continuous jog.
    // when the move stops state transitions to PS_PAUSED and PS_PAUSED_IN_OFFSET
    // depending on stopped position.
    PS_JOG_ABORTING=5,

    // the reentry move to initial_pause_position is in progress
    // because a resume was issued
    // when this move completes, state switches back to PS_RUNNING
    // and the primary motion queue is resumed where we left off
    PS_RETURNING=6,

    // entered from state PS_RUNNING if a step is in progress
    //
    PS_PAUSING_FOR_STEP=7,
};

#endif
