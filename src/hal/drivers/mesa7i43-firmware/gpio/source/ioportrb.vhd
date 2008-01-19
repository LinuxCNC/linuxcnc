library IEEE;
use IEEE.std_logic_1164.ALL;
use IEEE.std_logic_ARITH.ALL;
use IEEE.std_logic_UNSIGNED.ALL;

entity ioportrb is
	 generic (Width : integer);
	 port (			
			obus: out std_logic_vector (Width-1 downto 0);
			readport: in std_logic;
			portdata: in std_logic_vector (Width-1 downto 0) 
			);
end ioportrb;

architecture behavioral of ioportrb is

begin
	aioportrb: process (portdata,readport)
	begin
		if readport = '1' then
			obus <= portdata;
		 else
			obus <= (others => 'Z');
		end if;
	end process;

end behavioral;
