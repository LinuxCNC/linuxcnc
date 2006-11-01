-------------------------------------------------------------------------------
--
-- Copyright (C) 2005 Peter C. Wallace <pcw AT mesanet DOT com>
--
-- $RCSfile$
-- $Author$
-- $Locker$
-- $Revision$
-- $State$
-- $Date$
--
-------------------------------------------------------------------------------
--
-- This program is free software; you can redistribute it and/or
-- modify it under the terms of version 2 of the GNU General
-- Public License as published by the Free Software Foundation.
-- This library is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public
-- License along with this library; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
--
-- THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
-- ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
-- TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
-- harming persons must have provisions for completely removing power
-- from all motors, etc, before persons enter any danger area.  All
-- machinery must be designed to comply with local and national safety
-- codes, and the authors of this software can not, and do not, take
-- any responsibility for such compliance.
--
-- This code was written as part of the EMC HAL project.  For more
-- information, go to www.linuxcnc.org.
--
-------------------------------------------------------------------------------

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
