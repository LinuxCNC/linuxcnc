#ifndef EMC_TASK_HH
#define EMC_TASK_HH
extern NMLmsg *emcTaskCommand;
extern int stepping;
extern int steppingWait;
extern int emcTaskQueueCommand(NMLmsg *cmd);
#endif

