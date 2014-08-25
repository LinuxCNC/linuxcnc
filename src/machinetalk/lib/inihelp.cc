#include "inihelp.hh"

// if *value == NULL, find in fp/key/section and strdup the result
int str_inidefault(char **value, FILE *fp, const char *key, const char *section)
{
    if (value == NULL) return -1;
    if (*value == NULL) {
	const char *s = iniFind(fp, key, section);
	if (s != NULL) {
	    *value = strdup(s);
	    return 1;
	}
	return 0;
    } else
	return 0; // already set
}
