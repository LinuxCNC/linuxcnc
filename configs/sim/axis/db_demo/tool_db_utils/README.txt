Linuxcnc SQLite3 Integration Tool

The program is a tool loader and manager that interfaces with an SQLite3 database to manage, store, and retrieve information about tools. This tool loader is designed to be extensible and to handle commands from an external source, with functions for putting, getting, loading, and unloading tools.

Eventually it should be able to track tool life and usage through different options in the database. 

The SQLite3 database (tools_database.db) contains a table named tools to store information about tools. The columns in the table include:

    tool_number: The identifier for the tool.
    pocket: The pocket position of the tool.
    diameter: The diameter of the tool.
    z_offset: The Z offset of the tool.
    tool_name: The name or description of the tool.

Command Inputs: The program is designed to handle command inputs through the standard input (stdin). The supported commands are:
        g: Get tool information for all tools.
        p: Put (update or insert) tool information.
        u: Unload tool from spindle.
        l: Load tool into spindle.
        t: Retrieve specific tool information.

Callbacks: Callbacks are functions that the program calls when specific events or actions occur. The program supports four callbacks:
        tool_getter: Fetch tool information.
        tool_putter: Update or insert tool information.
        spindle_loader: Load a tool into the spindle.
        spindle_unloader: Unload a tool from the spindle.

INSTALL 

Run make in the source directory, or run this: 

gcc tool_loader.c -o tool_loader -lsqlite3

Add to Linuxcnc ini file like this (Make sure the compiled binary is in the same directory as the ini file)  

[EMCIO]
DB_PROGRAM           = ./tool_loader

