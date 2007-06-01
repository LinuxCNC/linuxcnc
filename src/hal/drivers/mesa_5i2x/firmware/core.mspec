[pin.constants]
# an ID code of zero assures that pin data comes before
# everything else in the RAM.  By putting it at a fixed
# location, we can avoid having 72 id_codes and 72 instance
# numbers eating up RAM space
id_code : 0
num_regs : 0
vhdl_package : pindriver

[pin.pins]
[pin.post-vhdl-vars]
mode : 0 ! { "disabled":0, "enabled":1, "tri-state":2, "open-collector":3 } ! "pin driver mode for output?"
invert : 0 ! { "active high":0, "active low":1 } ! "pin polarity?"
source : 0 ! { "GPIO":0, "dedicated":1 } ! "pin driver source?"
[pin.pre-vhdl-vars]
pre : 1

[pin.templates]
sigs :
vhdl :
ram :
 ${mode} | (${source}<<2) | (${invert}<<4)


[gpio.constants]
id_code : 2
# 4 bits per pin: two for output source, and two for mode
# thus 8 pins per 32-bit register, and 3 registers per port
num_regs : 3
vhdl_package : pindriver

[gpio.pins]
[gpio.post-vhdl-vars]
[gpio.pre-vhdl-vars]

[gpio.templates]
sigs :

vhdl :
 !gpio_${instnum}: gpio
 !    port map (
 !        clock => lclk,
 !        enable => one,
 !        ibus => wr_bus,
 !        obus => rd_bus,
 !        sel => ${cs},
 !        write => write,
 !        read => read,
 !        addr => addr(3 downto 2),
 !   );

ram :
 ${id_code}
 ${instnum}
 ${baseaddr}>>8
 ${baseaddr}
 ${step_type} | (${ctrl_type}<<2) | (${enable}<<3)
 ${step_up_pha_pin}
 ${dir_down_phb_pin}


