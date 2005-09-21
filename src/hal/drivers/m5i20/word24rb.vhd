library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity word24rb is
    Port (			
	 		obus: out STD_LOGIC_VECTOR (23 downto 0);
			readport: in STD_LOGIC;
			portdata: in STD_LOGIC_VECTOR (23 downto 0) );
end word24rb;

architecture behavioral of word24rb is

begin
	awordiorb: process (portdata,readport)
	begin
		if readport = '1' then
			obus <= portdata;
 	   else
			obus <= "ZZZZZZZZZZZZZZZZZZZZZZZZ";
		end if;
	end process;

end behavioral;
