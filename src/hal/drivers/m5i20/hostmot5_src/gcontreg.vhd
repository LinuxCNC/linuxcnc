library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
entity globalcontrolreg is
   port (
	clk: in STD_LOGIC;
	ibus: in STD_LOGIC_VECTOR (15 downto 0);
	reset: in STD_LOGIC;
	loadgcr: in STD_LOGIC;
	ctrclear: out STD_LOGIC;
	ctrlatch: out STD_LOGIC;
	pwmclear: out STD_LOGIC;
	irqclear: out STD_LOGIC;
	reloadwd: out STD_LOGIC
	);
end globalcontrolreg;

architecture behavioral of globalcontrolreg is

signal ctrclearreg: STD_LOGIC;
signal ctrlatchreg: STD_LOGIC;
signal pwmclearreg: STD_LOGIC;
signal irqclearreg: STD_LOGIC;
signal reloadwdreg: STD_LOGIC;
begin
	agcrreg: process (clk, 
							ibus,
							reset,
							loadgcr,
							ctrclearreg,
							ctrlatchreg,
							pwmclearreg,
							irqclearreg,
							reloadwdreg
							)
							
	begin
		if clk'event and clk = '1' then
			
			if loadgcr = '1' then
 		   	ctrclearreg <= ibus(0);
				ctrlatchreg <= ibus(1);
				pwmclearreg <= ibus(2);
				irqclearreg <= ibus(3);
				reloadwdreg	<= ibus(4);
			end if;	
			if ctrclearreg = '1' then 
				ctrclearreg <= '0';
			end if;
			if ctrlatchreg = '1' then 
				ctrlatchreg <= '0';
			end if;
			if pwmclearreg = '1' then 
				pwmclearreg <= '0';
			end if;
			if irqclearreg = '1' then 
				irqclearreg <= '0';
			end if;
			if reloadwdreg = '1' then 
				reloadwdreg <= '0';
			end if;
		end if;
		if ctrclearreg = '1' or reset = '1' then
			ctrclear <= '1';
		else 
			ctrclear <= '0';
		end if;
		if ctrlatchreg = '1' or reset = '1' then
			ctrlatch <= '1';
		else 
			ctrlatch <= '0';
		end if;
		if pwmclearreg = '1' or reset = '1' then
			pwmclear <= '1';
		else 
			pwmclear <= '0';
		end if;
		if irqclearreg = '1' or reset = '1' then
			irqclear <= '1';
		else 
			irqclear <= '0';
		end if;
		if reloadwdreg = '1' or reset = '1' then
			reloadwd <= '1';
		else 
			reloadwd <= '0';
		end if;
	
	end process;
end behavioral;

