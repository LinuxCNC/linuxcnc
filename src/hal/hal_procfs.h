
#define PROCFS


#ifdef RTAPI
#ifdef PROCFS
int hal_init_procfs();

void hal_shutdown_procfs();

#endif //RTAPI
#endif //MODULE
