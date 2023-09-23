#include "tooldb.h"
char theline[256];
int* tools;
int tools_count = 0;
int debug = 1; 

char* (*get_tool_func_ptr)(int toolno) = NULL;
void (*put_tool_func_ptr)(int toolno, const char* toolline) = NULL;
void (*load_spindle_func_ptr)(int toolno, const char* toolline) = NULL;
void (*unload_spindle_func_ptr)(int toolno, const char* toolline) = NULL;


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

// Implementing unknown_cmd
void unknown_cmd(char cmd, const char* params) {
    nak_reply("unknown cmd");
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


void tooldb_callbacks(
    char* (*tool_getter)(int toolno),
    void (*tool_putter)(int toolno, const char* toolline),
    void (*spindle_loader)(int toolno, const char* toolline),
    void (*spindle_unloader)(int toolno, const char* toolline)
) {

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
