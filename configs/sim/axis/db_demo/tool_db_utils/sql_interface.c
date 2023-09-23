#include "tooldb.h"

char* string_to_upper(char* s) {
    char* temp = s;
    while (*temp) {
        *temp = toupper((unsigned char) *temp);
        temp++;
    }
    return s;
}

int get_tool_count() {
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    int count = 0;

    rc = sqlite3_open(DB_FILENAME, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1; // Return -1 to indicate an error
    }

    char sql[] = "SELECT COUNT(*) FROM tools";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
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

void put_cmds(char cmd, const char* params) {
    char* duplicated_params = strdup(params);
    const char* uparams = string_to_upper(duplicated_params);
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
    // Print that the tool is being loaded into spindle
    printf("LOAD: Loading tool into spindle: %s\n", toolline);

    // Extract tool number and diameter and print them
    double diameter;
    sscanf(toolline, "T%d P%d D%lf", &toolno, &toolno, &diameter); // Note: This assumes the toolline format is consistent
    printf("Tool Number: %d, Diameter: %.2f\n", toolno, diameter);
}


void unload_spindle(int toolno, const char* toolline) {
    printf("UNLOAD: Unloading tool from spindle: %s\n", toolline);
}

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
