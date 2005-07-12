library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

--  Uncomment the following lines to use the declarations that are
--  provided for instantiating Xilinx primitive components.
--library UNISIM;
--use UNISIM.VComponents.all;

entity timeout is
    Port ( clk : in std_logic;
           ibus : in std_logic_vector(15 downto 0);
           obus : out std_logic_vector(15 downto 0);
           timeoutload : in std_logic;
           timeoutread : in std_logic;
           timerread : in std_logic;
           reload : in std_logic;
           timerz : out std_logic);
end timeout;

architecture Behavioral of timeout is

signal timer: std_logic_vector (15 downto 0);
signal timerlatch: std_logic_vector (15 downto 0);
signal prescale: std_logic_vector (5 downto 0);

begin
	atimeout: process (clk,timer,timerlatch)
	begin
		if clk'event and clk = '1' then
			
			prescale <= prescale +1;
			if prescale = 32 then
				prescale <= "000000";
			end if;
			
			if prescale = 0 then
				if timer /= 0 then 
					timer <= timer -1;
				end if;
			end if;

			if timeoutload = '1' then
				timerlatch <= ibus;
				timer <= ibus;
			end if;

			if reload = '1' then
				timer <= timerlatch;
			end if;
		end if; -- clk
		if timerread = '0' and timeoutread = '1' then
			obus <= timerlatch;
		elsif timerread = '1' and timeoutread = '0' then
			obus <= timer;
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;
		if timer = 0 then
			timerz <= '1';
		else
			timerz <= '0';
		end if;
	end process;

end Behavioral;
