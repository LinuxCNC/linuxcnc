[pin.symbols]
# an ID code of zero assures that pin data comes before
# everything else in the RAM.  By putting it at a fixed
# location, we can avoid having 72 id_codes and 72 instance
# numbers eating up RAM space
id_code : {"type":"constant", "value":0}
num_regs : {"type":"constant", "value":0}
vhdl_package : {"type":"constant", "value":"pindriver"}
mode : { "type":"enum_postroute", "default":0, "options":{ "disabled":0, "enabled":1, "tri-state":2, "open-collector":3 }, "question":"pin driver mode for output?"}
invert : { "type":"enum_postroute", "default":0, "options":{ "active high":0, "active low":1 }, "question":"pin polarity?"}
source : { "type":"enum_postroute", "default":0, "options":{ "GPIO":0, "dedicated":1 }, "question":"pin driver source?"}

[pin.templates]
sigs :
vhdl :
ram :
 ${mode} | (${source}<<2) | (${invert}<<4)


[gpio.symbols]
id_code : {"type":"constant", "value":1}
# 4 bits per pin: two for output source, and two for mode
# thus 8 pins per 32-bit register, and 3 registers per port
num_regs : {"type":"constant", "value":3}
vhdl_package : {"type":"constant", "value":"pindriver"}

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


