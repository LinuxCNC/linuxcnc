
#define PROCFS


#ifdef RTAPI
#ifdef PROCFS
int hal_init_procfs(void);

void hal_shutdown_procfs(void);

#endif //RTAPI
#endif //MODULE
