/*
 * iniparse.c - Minimal INI file parser
 *
 * Copyright (C) 2004 Fred Proctor & Will Shackleford (original)
 * Copyright (C) 2026 LinuxCNC contributors (cleanup)
 * License: GPL Version 2
 */

#include "iniparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INI_LINELEN 255
#define INI_MAX_EXTEND_LINES 20

static char *skip_white(const char *s)
{
    while (*s) {
        if (*s == ';' || *s == '#')
            return NULL;
        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
            return (char *)s;
        s++;
    }
    return NULL;
}

static char *after_equal(const char *s)
{
    while (*s && *s != '=')
        s++;
    if (*s != '=')
        return NULL;
    s++;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    if (*s == '\0')
        return NULL;
    return (char *)s;
}

const char *ini_find(FILE *fp, const char *tag, const char *section, int num)
{
    static char line[INI_LINELEN + 2];
    char bracket_section[INI_LINELEN + 2];
    char eline[(INI_LINELEN + 2) * (INI_MAX_EXTEND_LINES + 1)];
    char *elineptr;
    char *elinenext = eline;
    int extend_ct = 0;
    char *non_white;
    int newline_pos;
    int len;
    char tag_end;
    char *value;
    char *end_value;

    if (fp == NULL || tag == NULL)
        return NULL;

    rewind(fp);

    /* Find [section] if specified */
    if (section != NULL) {
        snprintf(bracket_section, sizeof(bracket_section), "[%s]", section);

        while (!feof(fp)) {
            if (fgets(line, INI_LINELEN + 1, fp) == NULL)
                return NULL;

            newline_pos = strlen(line) - 1;
            if (newline_pos >= 0 && line[newline_pos] == '\n')
                line[newline_pos] = '\0';

            non_white = skip_white(line);
            if (non_white == NULL)
                continue;

            if (strncmp(bracket_section, non_white, strlen(bracket_section)) == 0)
                break;
        }

        if (feof(fp))
            return NULL;
    }

    /* Search for tag */
    while (!feof(fp)) {
        if (fgets(line, INI_LINELEN + 1, fp) == NULL)
            return NULL;

        newline_pos = strlen(line) - 1;
        if (newline_pos < 0)
            newline_pos = 0;
        if (line[newline_pos] == '\n')
            line[newline_pos] = '\0';

        /* Handle backslash line continuation */
        if (newline_pos > 0 && line[newline_pos - 1] == '\\') {
            newline_pos = newline_pos - 1;
            line[newline_pos] = '\0';
            if (!extend_ct) {
                elineptr = eline;
                strncpy(elineptr, line, newline_pos);
                elinenext = elineptr + newline_pos;
            } else {
                strncpy(elinenext, line, newline_pos);
                elinenext = elinenext + newline_pos;
            }
            *elinenext = '\0';
            extend_ct++;
            if (extend_ct > INI_MAX_EXTEND_LINES) {
                fprintf(stderr, "iniparse: too many backslash line extends (limit=%d)\n",
                        INI_MAX_EXTEND_LINES);
                return NULL;
            }
            continue;
        } else {
            if (extend_ct) {
                strncpy(elinenext, line, newline_pos);
                elinenext = elinenext + newline_pos;
                *elinenext = '\0';
            }
        }
        if (!extend_ct)
            elineptr = line;
        extend_ct = 0;

        non_white = skip_white(elineptr);
        if (non_white == NULL)
            continue;

        /* Check for new section - means we left our section */
        if (section != NULL && non_white[0] == '[')
            return NULL;

        len = strlen(tag);
        if (strncmp(tag, non_white, len) != 0)
            continue;

        /* Check that next char after tag is whitespace or '=' */
        tag_end = non_white[len];
        if (tag_end != ' ' && tag_end != '\t' && tag_end != '\r'
            && tag_end != '\n' && tag_end != '=')
            continue;

        /* Found a match - check if it's the nth one */
        if (--num > 0)
            continue;

        value = after_equal(non_white + len);
        if (value == NULL)
            return NULL;

        /* Trim trailing whitespace */
        end_value = value + strlen(value) - 1;
        while (end_value > value &&
               (*end_value == ' ' || *end_value == '\t' || *end_value == '\r'))
            *end_value-- = '\0';

        return value;
    }

    return NULL;
}

int ini_find_int(FILE *fp, const char *tag, const char *section, int *result)
{
    const char *s = ini_find(fp, tag, section, 1);
    int tmp;

    if (s == NULL)
        return -1;
    if (sscanf(s, "%i", &tmp) != 1)
        return -1;
    *result = tmp;
    return 0;
}

int ini_find_double(FILE *fp, const char *tag, const char *section, double *result)
{
    const char *s = ini_find(fp, tag, section, 1);
    double tmp;

    if (s == NULL)
        return -1;
    if (sscanf(s, "%lf", &tmp) != 1)
        return -1;
    *result = tmp;
    return 0;
}

int ini_tilde_expansion(const char *file, char *path, size_t size)
{
    const char *home;
    int res;

    if (file == NULL || path == NULL)
        return -1;

    if (strlen(file) < 2 || !(file[0] == '~' && file[1] == '/')) {
        res = snprintf(path, size, "%s", file);
        return (res < 0 || (size_t)res >= size) ? -1 : 0;
    }

    home = getenv("HOME");
    if (home == NULL)
        return -1;

    res = snprintf(path, size, "%s%s", home, file + 1);
    return (res < 0 || (size_t)res >= size) ? -1 : 0;
}
