#!/usr/bin/tclsh
package require Tk
wm withdraw .
tk_messageBox \
    -title "Placeholder" \
    -type ok \
    -message "Specify a\n\n\[DISPLAY\]TOOL_EDITOR\n\napplicable for the\n\n\[EMCIO\]DB_PROGRAM"
exit 0
