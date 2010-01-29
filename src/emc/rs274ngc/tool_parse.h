#ifndef TOOL_PARSE_H
#define TOOL_PARSE_H

#include "emctool.h"
#ifdef CPLUSPLUS
extern "C"  {
#endif

int loadToolTable(const char *filename,
	struct CANON_TOOL_TABLE toolTable[CANON_POCKETS_MAX],
	int fms[CANON_POCKETS_MAX],
	char *ttcomments[CANON_POCKETS_MAX],
	int random_toolchanger
	);

#ifdef CPLUSPLUS
}
#endif

#endif
