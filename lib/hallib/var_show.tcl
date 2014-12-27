# var_show.tcl

# this halfile can be used to show context and
# ini variable arrays available to tcl halfiles
# example: [HAL]LIB:var_show.tcl arg1 arg2

#begin-----------------------------------------------------------------
source [file join $::env(HALLIB_DIR) util_lib.tcl]

show_ini
show_context

puts ::argv=$::argv
puts ::arglen=[llength $::argv]
