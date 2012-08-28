#ifndef REDISLIB_H
#define REDISLIB_H

#ifdef __cplusplus
extern "C"
{
#endif
// redis primitives wrapped in redislib.c

extern int redis_init(const char *inifile);
extern int redis_close();

extern int redis_cmd(const char *format, ...);
extern int redis_get_double(double *value,const char *format, ...);
extern int redis_get_int(int *value, const char *format, ...);
extern int redis_get_string(char *value, int maxlength, const char *format, ...);

#ifdef __cplusplus
}
#endif

// use the EMC_DEBUG_REDIS bit for tracing REDIS (emc/nml_intf/debugflags.h)



#endif
