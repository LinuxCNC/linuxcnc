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
-- modify it under the terms of version 2.1 of the GNU General
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
