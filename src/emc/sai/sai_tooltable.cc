/*
 * sai_tooltable.cc - Simple tool table for the standalone interpreter
 *
 * Provides tooldata_get/tooldata_put/tooldata_load using a plain array.
 * Parses the standard LinuxCNC tool table text format:
 *   T<n> P<n> D<d> X<x> Y<y> Z<z> A<a> B<b> C<c> U<u> V<v> W<w> I<f> J<b> Q<o>
 *
 * Derived from a work by Tom Kramer (original saicanon.cc)
 * Copyright (c) 2007 All rights reserved.
 * Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
 * License: GPL Version 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "emctool.h"

/* Storage */
static CANON_TOOL_TABLE tool_table[CANON_POCKETS_MAX];
static int last_index = 0;

/* C-linkage API matching tooldata.hh signatures */
extern "C" {

enum toolidx_t { IDX_OK = 0, IDX_NEW, IDX_FAIL };

toolidx_t tooldata_get(CANON_TOOL_TABLE *pdata, int idx)
{
    if (idx < 0 || idx >= CANON_POCKETS_MAX) return IDX_FAIL;
    *pdata = tool_table[idx];
    return IDX_OK;
}

toolidx_t tooldata_put(CANON_TOOL_TABLE tdata, int idx)
{
    if (idx < 0 || idx >= CANON_POCKETS_MAX) return IDX_FAIL;
    tool_table[idx] = tdata;
    if (idx > last_index) last_index = idx;
    return IDX_OK;
}

int tooldata_last_index_get(void) { return last_index; }

} /* extern "C" */

/* Parse one tool table line. Returns pocket index or -1 on skip/error. */
static int parse_tool_line(const char *input)
{
    char work[CANON_TOOL_ENTRY_LEN];
    strncpy(work, input, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    /* Strip comment */
    char *semi = strchr(work, ';');
    if (semi) *semi = '\0';

    if (work[0] == ';' || work[0] == '\n' || work[0] == '\0')
        return 0;

    CANON_TOOL_TABLE t = {};
    t.toolno = -1;
    int idx = -1;
    int valid = 1;

    char *token = strtok(work, " \t\r\n");
    while (token) {
        switch (toupper(token[0])) {
        case 'T': valid = (sscanf(&token[1], "%d", &t.toolno) == 1); break;
        case 'P': valid = (sscanf(&token[1], "%d", &idx) == 1); break;
        case 'D': valid = (sscanf(&token[1], "%lf", &t.diameter) == 1); break;
        case 'X': valid = (sscanf(&token[1], "%lf", &t.offset.tran.x) == 1); break;
        case 'Y': valid = (sscanf(&token[1], "%lf", &t.offset.tran.y) == 1); break;
        case 'Z': valid = (sscanf(&token[1], "%lf", &t.offset.tran.z) == 1); break;
        case 'A': valid = (sscanf(&token[1], "%lf", &t.offset.a) == 1); break;
        case 'B': valid = (sscanf(&token[1], "%lf", &t.offset.b) == 1); break;
        case 'C': valid = (sscanf(&token[1], "%lf", &t.offset.c) == 1); break;
        case 'U': valid = (sscanf(&token[1], "%lf", &t.offset.u) == 1); break;
        case 'V': valid = (sscanf(&token[1], "%lf", &t.offset.v) == 1); break;
        case 'W': valid = (sscanf(&token[1], "%lf", &t.offset.w) == 1); break;
        case 'I': valid = (sscanf(&token[1], "%lf", &t.frontangle) == 1); break;
        case 'J': valid = (sscanf(&token[1], "%lf", &t.backangle) == 1); break;
        case 'Q': valid = (sscanf(&token[1], "%d", &t.orientation) == 1); break;
        default: break;
        }
        if (!valid) return -1;
        token = strtok(NULL, " \t\r\n");
    }

    if (idx < 0 || idx >= CANON_POCKETS_MAX) return -1;
    t.pocketno = idx;
    tooldata_put(t, idx);
    return idx;
}

/* Load tool table from text file */
int tooldata_load(const char *filename, char **)
{
    /* Clear table */
    memset(tool_table, 0, sizeof(tool_table));
    for (int i = 0; i < CANON_POCKETS_MAX; i++)
        tool_table[i].toolno = -1;
    last_index = 0;

    if (!filename || !filename[0]) return 0;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "sai_tooltable: cannot open '%s'\n", filename);
        return -1;
    }

    char line[CANON_TOOL_ENTRY_LEN];
    while (fgets(line, sizeof(line), fp)) {
        parse_tool_line(line);
    }
    fclose(fp);
    return 0;
}

/* Stub - SAI doesn't need shared memory */
struct EMC_TOOL_STAT;
int tool_mmap_creator(const EMC_TOOL_STAT *, int) { return 0; }
