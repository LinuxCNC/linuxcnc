#ifndef TOOLDB_H
#define TOOLDB_H

#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#define DB_FILENAME "tools_database.db"

extern char theline[256];
extern int* tools;
extern int tools_count;
extern int debug;
int get_tool_count();
void do_reply(const char* msg);
void saveline(const char* line);
const char* currentline();
void nak_reply(const char* msg);
char* get_tool(int toolno);
char* fetch_tool_from_db(int toolno);
void tool_cmd(char cmd, const char* params);
void format_tool_data(char* output, int tool_number, int pocket, double diameter, double z_offset, const char* tool_name);
void put_cmds(char cmd, const char* params);
void unknown_cmd(char cmd, const char* params);
void do_cmd(const char* line);
void get_cmd(char cmd, const char* params);
void put_tool(int toolno, const char* toolline);
void load_spindle(int toolno, const char* toolline);
void unload_spindle(int toolno, const char* toolline);
void tooldb_callbacks(
    char* (*tool_getter)(int toolno),
    void (*tool_putter)(int toolno, const char* toolline),
    void (*spindle_loader)(int toolno, const char* toolline),
    void (*spindle_unloader)(int toolno, const char* toolline)
);
void tooldb_tools(int* tool_list, int count);
void startup_ack();
void tooldb_loop();

void handle_interrupt(int sig);

#endif // TOOLDB_H
