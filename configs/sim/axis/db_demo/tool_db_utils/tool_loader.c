#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

/////////// Sqlite3 database location
#define DB_FILENAME "tools_database.db"

char theline[256]; // Adjust size if necessary
int* tools;
int tools_count;

void do_reply(const char* msg);
void saveline(const char* line);
const char* currentline();
void nak_reply(const char* msg);
char* get_tool(int toolno);
char* fetch_tool_from_db(int toolno);
void tool_cmd(char cmd, const char* params);
void format_tool_data(char* output, int tool_number, int pocket, double diameter, double z_offset, const char* tool_name);

// Forward declarations of the additional functions
void put_cmds(char cmd, const char* params);
void unknown_cmd(char cmd, const char* params);
void do_cmd(const char* line);
void put_tool(int toolno, const char* toolline);
void load_spindle(int toolno, const char* toolline);
void unload_spindle(int toolno, const char* toolline);
// Callback function pointers
char* (*get_tool_func_ptr)(int toolno) = NULL;
void (*put_tool_func_ptr)(int toolno, const char* toolline) = NULL;
void (*load_spindle_func_ptr)(int toolno, const char* toolline) = NULL;
void (*unload_spindle_func_ptr)(int toolno, const char* toolline) = NULL;




char* strupr(char* str) {
    char* orig = str;
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
    return orig;
}


int debug = 1; 

void get_cmd(char cmd, const char* params) {
    for (int i = 0; i < tools_count; i++) {
        int tno = tools[i];
        char* msg = NULL;

        msg = get_tool(tno);
        if (!msg) {
            nak_reply("get_cmd(): Exception: Failed to get tool data");
            return;
        }

        do_reply(msg);

        if (tno == 0 && debug) {
            fprintf(stderr, "no tool in spindle\n");
        }
        
        if (msg) {
            free(msg);  // get_tool function returns dynamically allocated memory
        }
    }

    do_reply("FINI (get_cmd)");
}
// Implementing put_cmds
void put_cmds(char cmd, const char* params) {
    const char* uparams = strupr(strdup(params));
    if (strlen(uparams) == 0) {
        fprintf(stderr, "cmd=%c: requires tool entry line\n", cmd);
        free((void*) uparams);
        return;
    }
    
    char* space_pos = strchr(uparams, ' ');
    if (!space_pos) {
        fprintf(stderr, "cmd=%c: failed to parse <%s>\n", cmd, params);
        free((void*) uparams);
        return;
    }

    int toolno = atoi(uparams + 1); // Assumes T is always first
    const char* toolline = uparams;

    if (!strstr(toolline, "T") || !strstr(toolline, " P")) {
        fprintf(stderr, "cmd=%c: failed to parse <%s>\n", cmd, params);
        free((void*) uparams);
        return;
    }

    switch(cmd) {
        case 'p':
            put_tool(toolno, toolline);
            break;
        case 'l':
            load_spindle(toolno, toolline);
            break;
        case 'u':
            unload_spindle(toolno, toolline);
            break;
        default:
            fprintf(stderr, "Invalid cmd passed to put_cmds.\n");
            break;
    }

    do_reply("FINI (update recvd)");
    free((void*) uparams);
}

void put_tool(int toolno, const char* toolline) {
    printf("PUT: Request to update tool %d with line: %s\n", toolno, toolline);
}

void load_spindle(int toolno, const char* toolline) {
    printf("LOAD: Loading tool into spindle: %s\n", toolline);
}

void unload_spindle(int toolno, const char* toolline) {
    printf("UNLOAD: Unloading tool from spindle: %s\n", toolline);
}

// Implementing unknown_cmd
void unknown_cmd(char cmd, const char* params) {
    nak_reply("unknown cmd");
}

// Implementing do_cmd
void do_cmd(const char* line) {
    char* line_copy = strdup(line);
    char* cmd = strtok(line_copy, " ");
    char* params = strtok(NULL, "");

    if (params) {
        params = params + strspn(params, " \t"); // Skip leading whitespaces
    } else {
        params = "";
    }

    void (*thecmd)(char, const char*);
    if (cmd[0] == 'g')      thecmd = get_cmd;
    else if (cmd[0] == 'p') thecmd = put_cmds;
    else if (cmd[0] == 'u') thecmd = put_cmds;
    else if (cmd[0] == 'l') thecmd = put_cmds;
    else if (cmd[0] == 't') thecmd = tool_cmd;
    else                    thecmd = unknown_cmd;

    thecmd(cmd[0], params);
    free(line_copy);
}


void do_reply(const char* msg) {
    printf("%s\n", msg);
    fflush(stdout);
}

void saveline(const char* line) {
    strncpy(theline, line, sizeof(theline) - 1);
    theline[sizeof(theline) - 1] = '\0';
}

const char* currentline() {
    return theline;
}

void nak_reply(const char* msg) {
    if (strcmp(msg, "empty_line") != 0) {
        fprintf(stderr, "Error traceback (detailed error info could be added here)\n");
    }
    printf("NAK %s <%s>\n", msg, currentline());
    fflush(stdout);
}

char* get_tool(int toolno) {
    // This loop checks if the tool is in your in-memory `tools` array. 
    // If you want to only fetch from the database, you can comment out this loop.
    /*for (int i = 0; i < tools_count; i++) {
        if (tools[i] == toolno) {
            return strdup("Tool found in static data.");
        }
    }
*/
    char* tool_data = fetch_tool_from_db(toolno);
    
    if (!tool_data) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "No data available for tool %d.", toolno);
        fprintf(stderr, "%s\n", error_msg);
        return strdup("NAK");
    }

    return tool_data;
}


char* fetch_tool_from_db(int toolno) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    char formatted_tool_data[256];
    int rc;

    rc = sqlite3_open(DB_FILENAME, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    char sql[] = "SELECT tool_number, pocket, diameter, z_offset, tool_name FROM tools WHERE tool_number=?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, toolno);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int tool_number = sqlite3_column_int(stmt, 0);
        int pocket = sqlite3_column_int(stmt, 1);
        double diameter = sqlite3_column_double(stmt, 2);
        double z_offset = sqlite3_column_double(stmt, 3);
        const unsigned char* tool_name = sqlite3_column_text(stmt, 4);

        format_tool_data(formatted_tool_data, tool_number, pocket, diameter, z_offset, (const char*)tool_name);

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return strdup(formatted_tool_data);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return NULL;
}

void tool_cmd(char cmd, const char* params) {
    if (strlen(params) == 0) {
        nak_reply("no toolno");
        return;
    }
    char* end;
    int toolno = strtol(params, &end, 10);
    if (end == params || *end != '\0') {
        nak_reply("non-integer toolno");
        return;
    }

    int found = 0;
    for (int i = 0; i < tools_count; i++) {
        if (tools[i] == toolno) {
            found = 1;
            break;
        }
    }
    if (!found) {
        nak_reply("toolno out-of-range");
        return;
    }
    
    char* reply = get_tool(toolno);
    if (reply) {
        do_reply(reply);
        free(reply);
    }
}

void format_tool_data(char* output, int tool_number, int pocket, double diameter, double z_offset, const char* tool_name) {
    time_t t;
    struct tm* tm_info;
    char current_time[18];
    time(&t);
    tm_info = localtime(&t);
    strftime(current_time, 18, "%d%b%H:%M.%S", tm_info);

    // Adjusting the output format for tool numbers less than 10
    if (tool_number < 10) {
        sprintf(output, "T%d  P%d D%.2f Z%.2f ;Tool_%d %s", tool_number, pocket, diameter, z_offset, tool_number, current_time);
    } else {
        sprintf(output, "T%d P%d D%.2f Z%.2f ;Tool_%d %s", tool_number, pocket, diameter, z_offset, tool_number, current_time);
    }
}
////////////CALLBACKS

void tooldb_callbacks(
    char* (*tool_getter)(int toolno),
    void (*tool_putter)(int toolno, const char* toolline),
    void (*spindle_loader)(int toolno, const char* toolline),
    void (*spindle_unloader)(int toolno, const char* toolline)) {
    get_tool_func_ptr = tool_getter;
    put_tool_func_ptr = tool_putter;
    load_spindle_func_ptr = spindle_loader;
    unload_spindle_func_ptr = spindle_unloader;
}


void tooldb_tools(int* tool_list, int count) {
    tools = tool_list;
    tools_count = count;
}

void startup_ack() {
    do_reply("v2.1");
}

void tooldb_loop() {
    startup_ack();
    char line[256]; // Adjust the buffer size if necessary

    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;  // Exit the loop on EOF or error
        }

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        saveline(line);

        if (strlen(line) == 0) {
            nak_reply("empty_line");
        } else {
            do_cmd(line);
        }
    }
}


volatile sig_atomic_t flag = 0;

void handle_interrupt(int sig) {
    flag = 1;
}

int main() {
    fprintf(stderr, "v2.1\n");

    tooldb_callbacks(get_tool, put_tool, load_spindle, unload_spindle);

    int list[19];
    for (int i = 0; i < 19; i++) {
        list[i] = i;
    }
    tooldb_tools(list, 19);

    while (1) {
        if (flag) {
            printf("\nExiting tool loader...\n");
            break;
        }

        tooldb_loop();
    }

    return 0;
}
