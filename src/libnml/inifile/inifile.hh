#ifndef INIFILE_HH
#define INIFILE_HH

/*
   inifile.hh

   Decls for INI file format functions

   Modification history:

   24-Feb-1998  FMP replaced __CPLUSPLUS__ with __cplusplus
   18-Dec-1997  FMP set up as C/C++ split for emcnml directory
   24-Apr-1997  FMP split into C and C++ parts
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
} class IniFile {
  public:
    IniFile();
    IniFile(const char *path);
     ~IniFile();

    const int open(const char *path);
    const int close();
    const char *find(const char *tag, const char *section = NULL);
    int section(const char *section, INIFILE_ENTRY array[], int max);
    const int valid();

  private:
      FILE * fp;
};

#endif /* C++ part */

#endif /* INIFILE_HH */
