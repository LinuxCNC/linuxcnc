#!/bin/sh
INI_FILE_NAME=tp.ini
# Need the export for the parse_ini call to work in haltcl
export INI_FILE_NAME
halrun "$INI_FILE_NAME"
