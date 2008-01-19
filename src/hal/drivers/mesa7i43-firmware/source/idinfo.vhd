----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    15:38:30 12/17/2007 
-- Design Name: 
-- Module Name:    idinfo - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity idinfo is
	 generic (
		width : integer;	
		cookie : std_logic_vector (31 downto 0);
		namelow : std_logic_vector (31 downto 0);
		namehigh : std_logic_vector (31 downto 0);
		version : std_logic_vector (31 downto 0)
		);
    port ( 
		obus : out  std_logic_vector (width-1 downto 0);
		addr : in  std_logic_vector (width-1 downto 0);
		read : in  std_logic);
end idinfo;

architecture Behavioral of idinfo is
begin

	idinfoproc: process (read,addr)
	begin
		obus <= (others => 'Z');
		if addr (6 downto 2) = "00000" and read = '1' then
			case addr (1 downto 0) is
				when "00" => obus <= Cookie(7 downto 0);
				when "01" => obus <= Cookie(15 downto 8);
				when "10" => obus <= Cookie(23 downto 16);
				when "11" => obus <= Cookie(31 downto 24);
				when others => null;
			end case;
		end if;
		if addr (6 downto 2) = "00001" and read = '1' then
			case addr (1 downto 0) is
				when "00" => obus <= NameLow(31 downto 24);
				when "01" => obus <= NameLow(23 downto 16);
				when "10" => obus <= NameLow(15 downto 8);
				when "11" => obus <= NameLow(7 downto 0);
				when others => null;
			end case;
		end if;	
		if addr (6 downto 2) = "00010" and read = '1' then
			case addr (1 downto 0) is
				when "00" => obus <= NameHigh(31 downto 24);
				when "01" => obus <= NameHigh(23 downto 16);
				when "10" => obus <= NameHigh(15 downto 8);
				when "11" => obus <= NameHigh(7 downto 0);
				when others => null;
			end case;
		end if;	
		if addr (6 downto 2) = "00011" and read = '1' then
			case addr (1 downto 0) is
				when "00" => obus <= Version(7 downto 0);
				when "01" => obus <= Version(15 downto 8);
				when "10" => obus <= Version(23 downto 16);
				when "11" => obus <= Version(31 downto 24);
				when others => null;
			end case;
		end if;			
	end process;
	
end Behavioral;
