/********************************************************************
* Description: rs274ngc_errors.cc
*
*   Derived from a work by Thomas Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/

// *INDENT-OFF*
char * _rs274ngc_errors[] = {
/*   0 */ "No error",
/*   1 */ "No error",
/*   2 */ "No error",
/*   3 */ "No error",
/*   4 */ "A file is already open", // rs274ngc_open
/*   5 */ "All axes missing with g92", // enhance_block
/*   6 */ "All axes missing with motion code", // enhance_block
/*   7 */ "Arc radius too small to reach end point", // arc_data_r
/*   8 */ "Argument to acos out of range", // execute_unary
/*   9 */ "Argument to asin out of range", // execute_unary
/*  10 */ "Attempt to divide by zero", // execute_binary1
/*  11 */ "Attempt to raise negative to non integer power", // execute_binary1
/*  12 */ "Bad character used", // read_one_item
/*  13 */ "Bad format unsigned integer", // read_integer_unsigned
/*  14 */ "Bad number format", // read_real_number
/*  15 */ "Bug bad g code modal group 0", // check_g_codes
/*  16 */ "Bug code not g0 or g1", // convert_straight, convert_straight_comp1, convert_straight_comp2
/*  17 */ "Bug code not g17 g18 or g19", // convert_set_plane
/*  18 */ "Bug code not g20 or g21", // convert_length_units
/*  19 */ "Bug code not g28 or g30", // convert_home
/*  20 */ "Bug code not g2 or g3", // arc_data_comp_ijk, arc_data_ijk
/*  21 */ "Bug code not g40 g41 or g42", // convert_cutter_compensation
/*  22 */ "Bug code not g43 or g49", // convert_tool_length_offset
/*  23 */ "Bug code not g4 g10 g28 g30 g53 or g92 series", // convert_modal_0
/*  24 */ "Bug code not g61 g61 1 or g64", // convert_control_mode
/*  25 */ "Bug code not g90 or g91", // convert_distance_mode
/*  26 */ "Bug code not g93 or g94", // convert_feed_mode
/*  27 */ "Bug code not g98 or g99", // convert_retract_mode
/*  28 */ "Bug code not in g92 series", // convert_axis_offsets
/*  29 */ "Bug code not in range g54 to g593", // convert_coordinate_system
/*  30 */ "Bug code not m0 m1 m2 m30 m60", // convert_stop
/*  31 */ "Bug distance mode not g90 or g91", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  32 */ "Bug function should not have been called", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx, read_a, read_b, read_c, read_comment, read_d, read_f, read_g, read_h, read_i, read_j, read_k, read_l, read_line_number, read_m, read_p, read_parameter, read_parameter_setting, read_q, read_r, read_real_expression, read_s, read_t, read_x, read_y, read_z
/*  33 */ "Bug in tool radius comp", // arc_data_comp_r
/*  34 */ "Bug plane not xy yz or xz", // convert_arc, convert_cycle
/*  35 */ "Bug side not right or left", // convert_straight_comp1, convert_straight_comp2
/*  36 */ "Bug unknown motion code", // convert_motion
/*  37 */ "Bug unknown operation", // execute_binary1, execute_binary2, execute_unary
/*  38 */ "Cannot change axis offsets with cutter radius comp", // convert_axis_offsets
/*  39 */ "Cannot change units with cutter radius comp", // convert_length_units
/*  40 */ "Cannot create backup file", // rs274ngc_save_parameters
/*  41 */ "Cannot do g1 with zero feed rate", // convert_straight
/*  42 */ "Cannot do zero repeats of cycle", // convert_cycle
/*  43 */ "Cannot make arc with zero feed rate", // convert_arc
/*  44 */ "Cannot move rotary axes during probing", // convert_probe
/*  45 */ "Cannot open backup file", // rs274ngc_save_parameters
/*  46 */ "Cannot open variable file", // rs274ngc_save_parameters
/*  47 */ "Cannot probe in inverse time feed mode", // convert_probe
/*  48 */ "Cannot probe with cutter radius comp on", // convert_probe
/*  49 */ "Cannot probe with zero feed rate", // convert_probe
/*  50 */ "Cannot put a b in canned cycle", // check_other_codes
/*  51 */ "Cannot put a c in canned cycle", // check_other_codes
/*  52 */ "Cannot put an a in canned cycle", // check_other_codes
/*  53 */ "Cannot turn cutter radius comp on out of xy plane", // convert_cutter_compensation_on
/*  54 */ "Cannot turn cutter radius comp on when on", // convert_cutter_compensation_on
/*  55 */ "Cannot use a word", // read_a
/*  56 */ "Cannot use axis values with g80", // enhance_block
/*  57 */ "Cannot use axis values without a g code that uses them", // enhance_block
/*  58 */ "Cannot use b word", // read_b
/*  59 */ "Cannot use c word", // read_c
/*  60 */ "Cannot use g28 or g30 with cutter radius comp", // convert_home
/*  61 */ "Cannot use g53 incremental", // check_g_codes
/*  62 */ "Cannot use g53 with cutter radius comp", // convert_straight
/*  63 */ "Cannot use two g codes that both use axis values", // enhance_block
/*  64 */ "Cannot use xz plane with cutter radius comp", // convert_set_plane
/*  65 */ "Cannot use yz plane with cutter radius comp", // convert_set_plane
/*  66 */ "Command too long", // read_text, rs274ngc_open
/*  67 */ "Concave corner with cutter radius comp", // convert_arc_comp2, convert_straight_comp2
/*  68 */ "Coordinate system index parameter 5220 out of range", // rs274ngc_init
/*  69 */ "Current point same as end point of arc", // arc_data_r
/*  70 */ "Cutter gouging with cutter radius comp", // convert_arc_comp1, convert_straight_comp1
/*  71 */ "D word with no g41 or g42", // check_other_codes
/*  72 */ "Dwell time missing with g4", // check_g_codes
/*  73 */ "Dwell time p word missing with g82", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  74 */ "Dwell time p word missing with g86", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  75 */ "Dwell time p word missing with g88", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  76 */ "Dwell time p word missing with g89", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  77 */ "Equal sign missing in parameter setting", // read_parameter_setting
/*  78 */ "F word missing with inverse time arc move", // convert_arc
/*  79 */ "F word missing with inverse time g1 move", // convert_straight
/*  80 */ "File ended with no percent sign", // read_text, rs274ngc_open
/*  81 */ "File ended with no percent sign or program end", // read_text
/*  82 */ "File name too long", // rs274ngc_open
/*  83 */ "File not open", // rs274ngc_read
/*  84 */ "G code out of range", // read_g
/*  85 */ "H word with no g43", // check_other_codes
/*  86 */ "I word given for arc in yz plane", // convert_arc
/*  87 */ "I word missing with g87", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  88 */ "I word with no g2 or g3 or g87 to use it", // check_other_codes
/*  89 */ "J word given for arc in xz plane", // convert_arc
/*  90 */ "J word missing with g87", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  91 */ "J word with no g2 or g3 or g87 to use it", // check_other_codes
/*  92 */ "K word given for arc in xy plane", // convert_arc
/*  93 */ "K word missing with g87", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/*  94 */ "K word with no g2 or g3 or g87 to use it", // check_other_codes
/*  95 */ "L word with no canned cycle or g10", // check_other_codes
/*  96 */ "Left bracket missing after slash with atan", // read_atan
/*  97 */ "Left bracket missing after unary operation name", // read_unary
/*  98 */ "Line number greater than 99999", // read_line_number
/*  99 */ "Line with g10 does not have l2", // check_g_codes
/* 100 */ "M code greater than 199", // read_m
/* 101 */ "Mixed radius ijk format for arc", // convert_arc
/* 102 */ "Multiple a words on one line", // read_a
/* 103 */ "Multiple b words on one line", // read_b
/* 104 */ "Multiple c words on one line", // read_c
/* 105 */ "Multiple d words on one line", // read_d
/* 106 */ "Multiple f words on one line", // read_f
/* 107 */ "Multiple h words on one line", // read_h
/* 108 */ "Multiple i words on one line", // read_i
/* 109 */ "Multiple j words on one line", // read_j
/* 110 */ "Multiple k words on one line", // read_k
/* 111 */ "Multiple l words on one line", // read_l
/* 112 */ "Multiple p words on one line", // read_p
/* 113 */ "Multiple q words on one line", // read_q
/* 114 */ "Multiple r words on one line", // read_r
/* 115 */ "Multiple s words on one line", // read_s
/* 116 */ "Multiple t words on one line", // read_t
/* 117 */ "Multiple x words on one line", // read_x
/* 118 */ "Multiple y words on one line", // read_y
/* 119 */ "Multiple z words on one line", // read_z
/* 120 */ "Must use g0 or g1 with g53", // check_g_codes
/* 121 */ "Negative argument to sqrt", // execute_unary
/* 122 */ "Negative d word tool radius index used", // read_d
/* 123 */ "Negative f word used", // read_f
/* 124 */ "Negative g code used", // read_g
/* 125 */ "Negative h word tool length offset index used", // read_h
/* 126 */ "Negative l word used", // read_l
/* 127 */ "Negative m code used", // read_m
/* 128 */ "Negative or zero q value used", // read_q
/* 129 */ "Negative p word used", // read_p
/* 130 */ "Negative spindle speed used", // read_s
/* 131 */ "Negative tool id used", // read_t
/* 132 */ "Nested comment found", // close_and_downcase
/* 133 */ "No characters found in reading real value", // read_real_value
/* 134 */ "No digits found where real number should be", // read_real_number
/* 135 */ "Non integer value for integer", // read_integer_value
/* 136 */ "Null missing after newline", // close_and_downcase
/* 137 */ "Offset index missing", // convert_tool_length_offset
/* 138 */ "P value not an integer with g10 l2", // check_g_codes
/* 139 */ "P value out of range with g10 l2", // check_g_codes
/* 140 */ "P word with no g4 g10 g82 g86 g88 g89", // check_other_codes
/* 141 */ "Parameter file out of order", // rs274ngc_restore_parameters, rs274ngc_save_parameters
/* 142 */ "Parameter number out of range", // read_parameter, read_parameter_setting, rs274ngc_restore_parameters, rs274ngc_save_parameters
/* 143 */ "Q word missing with g83", // convert_cycle_xy, convert_cycle_yz, convert_cycle_zx
/* 144 */ "Q word with no g83", // check_other_codes
/* 145 */ "Queue is not empty after probing", // rs274ngc_read
/* 146 */ "R clearance plane unspecified in cycle", // convert_cycle
/* 147 */ "R i j k words all missing for arc", // convert_arc
/* 148 */ "R less than x in cycle in yz plane", // convert_cycle_yz
/* 149 */ "R less than y in cycle in xz plane", // convert_cycle_zx
/* 150 */ "R less than z in cycle in xy plane", // convert_cycle_xy
/* 151 */ "R word with no g code that uses it", // check_other_codes
/* 152 */ "Radius to end of arc differs from radius to start", // arc_data_comp_ijk, arc_data_ijk
/* 153 */ "Radius too small to reach end point", // arc_data_comp_r
/* 154 */ "Required parameter missing", // rs274ngc_restore_parameters
/* 155 */ "Selected tool slot number too large", // convert_tool_select
/* 156 */ "Slash missing after first atan argument", // read_atan
/* 157 */ "Spindle not turning clockwise in g84", // convert_cycle_g84
/* 158 */ "Spindle not turning in g86", // convert_cycle_g86
/* 159 */ "Spindle not turning in g87", // convert_cycle_g87
/* 160 */ "Spindle not turning in g88", // convert_cycle_g88
/* 161 */ "Sscanf failed", // read_integer_unsigned, read_real_number
/* 162 */ "Start point too close to probe point", // convert_probe
/* 163 */ "Too many m codes on line", // check_m_codes
/* 164 */ "Tool length offset index too big", // read_h
/* 165 */ "Tool max too large", // rs274ngc_load_tool_table
/* 166 */ "Tool radius index too big", // read_d
/* 167 */ "Tool radius not less than arc radius with comp", // arc_data_comp_r, convert_arc_comp2
/* 168 */ "Two g codes used from same modal group", // read_g
/* 169 */ "Two m codes used from same modal group", // read_m
/* 170 */ "Unable to open file", // convert_stop, rs274ngc_open, rs274ngc_restore_parameters
/* 171 */ "Unclosed comment found", // close_and_downcase
/* 172 */ "Unclosed expression", // read_operation
/* 173 */ "Unknown g code used", // read_g
/* 174 */ "Unknown m code used", // read_m
/* 175 */ "Unknown operation", // read_operation
/* 176 */ "Unknown operation name starting with a", // read_operation
/* 177 */ "Unknown operation name starting with m", // read_operation
/* 178 */ "Unknown operation name starting with o", // read_operation
/* 179 */ "Unknown operation name starting with x", // read_operation
/* 180 */ "Unknown word starting with a", // read_operation_unary
/* 181 */ "Unknown word starting with c", // read_operation_unary
/* 182 */ "Unknown word starting with e", // read_operation_unary
/* 183 */ "Unknown word starting with f", // read_operation_unary
/* 184 */ "Unknown word starting with l", // read_operation_unary
/* 185 */ "Unknown word starting with r", // read_operation_unary
/* 186 */ "Unknown word starting with s", // read_operation_unary
/* 187 */ "Unknown word starting with t", // read_operation_unary
/* 188 */ "Unknown word where unary operation could be", // read_operation_unary
/* 189 */ "X and y words missing for arc in xy plane", // convert_arc
/* 190 */ "X and z words missing for arc in xz plane", // convert_arc
/* 191 */ "X value unspecified in yz plane canned cycle", // convert_cycle_yz
/* 192 */ "X y and z words all missing with g38 2", // convert_probe
/* 193 */ "Y and z words missing for arc in yz plane", // convert_arc
/* 194 */ "Y value unspecified in xz plane canned cycle", // convert_cycle_zx
/* 195 */ "Z value unspecified in xy plane canned cycle", // convert_cycle_xy
/* 196 */ "Zero or negative argument to ln", // execute_unary
/* 197 */ "Zero radius arc", // arc_data_ijk
"The End"};
// *INDENT-ON*
