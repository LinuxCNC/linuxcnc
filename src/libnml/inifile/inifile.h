#ifndef INIFILE_H
#define INIFILE_H

/*
   inifile.h

   Decls for INI file format functions

*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>		/* FILE, NULL */

#define INIFILE_MAX_LINELEN 256	/* max number of chars in a line */

    typedef struct {
	char tag[INIFILE_MAX_LINELEN];
	char rest[INIFILE_MAX_LINELEN];
    } INIFILE_ENTRY;

    extern const char *iniFind(void *fp,	/* already opened file ptr */
	const char *tag,	/* string to find */
	const char *section);	/* section it's in */

    extern int iniSection(void *fp,	/* already opened file ptr */
	const char *section,	/* section you want */
	INIFILE_ENTRY array[],	/* entries to fill */
	int max);		/* how many you can hold */

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
class INIFILE {
  public:
    INIFILE();
    INIFILE(const char *path);
     ~INIFILE();

    const int open(const char *path);
    const int close();
    const char *find(const char *tag, const char *section = NULL);
    int section(const char *section, INIFILE_ENTRY array[], int max);
    const int valid();

  private:
      FILE * fp;
};

#endif /* C++ part */

#endif /* INIFILE_H */
