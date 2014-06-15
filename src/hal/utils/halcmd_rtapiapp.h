#ifndef HALCMD_RTAPIAPP_H
#define HALCMD_RTAPIAPP_H

#ifdef __cplusplus
extern "C" {
#endif

    int rtapi_connect(int instance, char *uri, const char *svc_uuid);
    int rtapi_loadrt(int instance, const char *modname, const char **args);
    int rtapi_unloadrt(int instance, const char *modname);
    int rtapi_shutdown(int instance);
    int rtapi_ping(int instance);
    int rtapi_newthread(int instance, const char *name, int period, int cpu, int use_fp);
    int rtapi_delthread(int instance, const char *name);
    const char *rtapi_rpcerror(void);

#ifdef __cplusplus
}
#endif

#endif // HALCMD_RTAPIAPP_H
