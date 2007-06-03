[stepgen.symbols]
id_code : {"type":"constant", "value":10}
num_regs : {"type":"constant", "value":3}
vhdl_package : {"type":"constant", "value":"stepgen"}
step_up_phA : {"type":"pin", "description":"step, up, or phaseA (depends on step type)"}
dir_down_phB : {"type":"pin", "description":"dir, down, or phaseB (depends on step type)"}
enable : { "type":"bool_postroute", "default":1, "question":"enable step generator $instnum?" }
ctrl_type : { "type":"enum_postroute", "default":1, "options":{ "velocity":0, "position":1 }, "question":"position or velocity control mode?" }
step_type : { "type":"enum_postroute", "default":0, "options":{ "step-dir":0, "up-down":1, "quadrature":2 }, "question":"step type?" }
clk_source : { "type":"enum_preroute", "default":0, "options":{ "33MHz":0, "16MHz":1 }, "question":"clock source for stepgen?" }

[stepgen.templates]

# The mess of ' !' below is a bit of ugly hackery to support multiple-line
# template values.  The python parser library that reads this code uses
# leading whitespace on a line as an indicator that it is a continuation
# of the preceding line and not another item, so leading space is required.
# The parser also strips any leading whitespace it encounters, so some
# non-space character is needed to preserve indenting.  The '!' was chosen
# because its unlikely to appear in VHDL code.

sigs :

vhdl :
 !stepgen_${instnum}: stepgen
 !    port map (
 !        clock => lclk,
 !        enable => one,
 !        ibus => wr_bus,
 !        obus => rd_bus,
 !        sel => ${cs},
 !        write => write,
 !        read => read,
 !        addr => addr(3 downto 2),
 !        out0 => pins_out(${step_up_pha}),
 !        out1 => pins_out(${dir_down_phb})
 !   );

ram :
 ${id_code}
 ${instnum}
 ${baseaddr}>>8
 ${baseaddr}
 ${step_type} | (${ctrl_type}<<2) | (${enable}<<3)
 ${step_up_pha}
 ${dir_down_phb}


