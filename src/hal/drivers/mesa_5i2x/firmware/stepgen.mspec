[stepgen.constants]
id_code : 10
num_regs : 4
vhdl_package : stepgen

[stepgen.pins]
step_up_phA_pin : "step, up, or phaseA (depends on step type)"
dir_down_phB_pin : "dir, down, or phaseB (depends on step type)"

[stepgen.post-vhdl-vars]
enable : { "no":0, "yes":1 } : "enable step generator $INSTANCE?"
ctrl_type : { "velocity":0, "position":1 } : "position or velocity control mode?"
step_type : { "step-dir":0, "up-down":1, "quadrature":2 } : "step type?" CHANGEABLE

[stepgen.pre-vhdl-vars]

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
 !        out0 => pins_out(${step_up_pha_pin}),
 !        out1 => pins_out(${dir_down_phb_pin})
 !   );

ram :
 ${id_code}
 ${instnum}
 ${baseaddr}>>8
 ${baseaddr}
 ${step_type} | (${ctrl_type}<<2) | (${enable}<<3)
 ${step_up_pha_pin}
 ${dir_down_phb_pin}


