/*
 */

#ifndef _INIHELP_HH
#define _INIHELP_HH

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "inifile.h"

#ifdef __cplusplus
extern "C" {
#endif

    int str_inidefault(char **value, FILE *fp, const char *key, const char *section);


#ifdef __cplusplus
}
#endif

#endif
