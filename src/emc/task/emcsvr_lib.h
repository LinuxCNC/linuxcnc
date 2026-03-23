#ifndef EMCSVR_LIB_H
#define EMCSVR_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

int  emcsvr_init(const char *ini_file);
int  emcsvr_run(void);
void emcsvr_stop(void);
void emcsvr_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* EMCSVR_LIB_H */
