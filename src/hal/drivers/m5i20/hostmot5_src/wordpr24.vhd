library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity wordpr24 is
    Port (		
	   	clear: in STD_LOGIC;
			clk: in STD_LOGIC;
			ibus: in STD_LOGIC_VECTOR (23 downto 0);
			obus: out STD_LOGIC_VECTOR (23 downto 0);
			loadport: in STD_LOGIC;
			loadddr: in STD_LOGIC;
			readddr: in STD_LOGIC;
			portdata: out STD_LOGIC_VECTOR (23 downto 0)
 			);
end wordpr24;

architecture behavioral of wordpr24 is

signal outreg: STD_LOGIC_VECTOR (23 downto 0);
signal ddrreg: STD_LOGIC_VECTOR (23 downto 0);
signal tsoutreg: STD_LOGIC_VECTOR (23 downto 0);

begin
	awordioport: process (
								clk,
								ibus,
								loadport,
								loadddr,
								readddr,
								outreg,ddrreg)
	begin
		if clk'event and clk = '1' then
			if loadport = '1'  then
				outreg <= ibus;
			end if; 
			if loadddr = '1' then
				ddrreg <= ibus;
			end if;
			if clear = '1' then ddrreg <= x"000000"; end if;
		end if; -- clk

		for i in 0 to 23 loop
			if ddrreg(i) = '1' then 
				tsoutreg(i) <= outreg(i);
			else
				tsoutreg(i) <= 'Z';
			end if;
		end loop;
		
		portdata <= tsoutreg;
		
		if readddr = '1' then
			obus <= ddrreg;
 	   else
			obus <= "ZZZZZZZZZZZZZZZZZZZZZZZZ";
		end if;

	end process;
end behavioral;
