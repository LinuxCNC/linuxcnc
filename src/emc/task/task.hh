#ifndef EMC_TASK_HH
#define EMC_TASK_HH
extern NMLmsg *emcTaskCommand;
extern int stepping;
extern int steppingWait;
extern int emcTaskQueueCommand(NMLmsg *cmd);
extern int emcPluginCall(EMC_EXEC_PLUGIN_CALL *call_msg);
extern int emcTaskOnce();

#endif

