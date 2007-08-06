
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


