library IEEE;
use IEEE.STD_LOGIC_1164.ALL;



entity idreadback is
    Generic ( id : std_logic_vector(31 downto 0);
	           mc : std_logic_vector(31 downto 0));

	 Port ( readid : in std_logic;
           readmc : in std_logic;
           obus : out std_logic_vector(31 downto 0));
end idreadback;

architecture Behavioral of idreadback is
begin

areadidprocess: process (readid, readmc)

begin
	if readid = '1' and readmc = '0' then 
		obus <= id; 
	 elsif readmc = '1' and readid = '0' then 
		obus <= mc; 
	else
		obus <= (others => 'Z');
	end if;
end process areadidprocess;

end Behavioral;
