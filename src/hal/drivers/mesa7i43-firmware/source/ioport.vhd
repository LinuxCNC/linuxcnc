library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity ioport is
	 generic(Width : integer);
	 port 
			(		
			clear: in STD_LOGIC;
			clk: in STD_LOGIC;
			ibus: in STD_LOGIC_VECTOR (Width -1 downto 0);
			obus: out STD_LOGIC_VECTOR (Width -1 downto 0);
			loadport: in STD_LOGIC;
			loadddr: in STD_LOGIC;
			readddr: in STD_LOGIC;
			portdata: out STD_LOGIC_VECTOR (Width -1 downto 0)
			 );
end ioport;

architecture behavioral of ioport is

signal outreg: STD_LOGIC_VECTOR (Width -1 downto 0);
signal ddrreg: STD_LOGIC_VECTOR (Width -1 downto 0);
signal tsoutreg: STD_LOGIC_VECTOR (Width -1 downto 0);

begin
	anioport: process (clk,readddr,tsoutreg,outreg,ddrreg)
	begin
		if rising_edge(clk) then
			if loadport = '1'  then
				outreg <= ibus;
			end if; 
			if loadddr = '1' then
				ddrreg <= ibus;
			end if;
			if clear = '1' then 
				ddrreg <= (others => '0');
			end if;
		end if; -- clk

		for i in 0 to Width -1 loop
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
			obus <= (others => 'Z');
		end if;

	end process;
end behavioral;
