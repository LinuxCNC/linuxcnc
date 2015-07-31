# vim: sts=4 sw=4 et
cdef extern from "halcmd_rtapiapp.h":

    int rtapi_connect(int instance, char *uri,  char *svc_uuid)
    int rtapi_loadrt(int instance, const char *modname, const char **args)
    int rtapi_unloadrt(int instance, const char *modname)
    int rtapi_shutdown(int instance)
    int rtapi_ping(int instance)
    int rtapi_newthread(int instance, const char *name,
                        int period, int cpu, int use_fp, int flags)
    int rtapi_delthread(int instance, const char *name)
    int rtapi_callfunc(int instance, const char *func, const char **args)
    int rtapi_newinst(int instance, const char *comp, const char *instname, const char **args)
    int rtapi_delinst(int instance, const char *instname)

    const char *rtapi_rpcerror()
