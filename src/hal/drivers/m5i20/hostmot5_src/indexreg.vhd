library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity indexreg is
   port (
	clk: in STD_LOGIC;
	ibus: in STD_LOGIC_VECTOR (15 downto 0);
	obus: out STD_LOGIC_VECTOR (15 downto 0);
	loadindex: in STD_LOGIC;
	readindex: in STD_LOGIC;
	index: out STD_LOGIC_VECTOR (7 downto 0)
	);
end indexreg;

architecture behavioral of indexreg is


signal indexreg: STD_LOGIC_VECTOR (7 downto 0);

begin
	aindexreg: process(clk, 
	 					
							ibus,
							loadindex,
							readindex,
							indexreg
							)
							
	begin
		if clk'event and clk = '1' then
			if loadindex = '1' then
 		   	indexreg <=  ibus(7 downto 0);
			end if;	
		end if;
		if readindex = '1' then
			obus(7 downto 0) <= indexreg;
			obus(15 downto 8) <= "ZZZZZZZZ";
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;
	index <= indexreg;
	end process;
end behavioral;

