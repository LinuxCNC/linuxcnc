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
entity globalcontrolreg is
   port (
	clk: in STD_LOGIC;
	ibus: in STD_LOGIC_VECTOR (15 downto 0);
	reset: in STD_LOGIC;
	loadgcr: in STD_LOGIC;
	ctrclear: out STD_LOGIC;
	ctrlatch: out STD_LOGIC;
	pwmclear: out STD_LOGIC;
	irqclear: out STD_LOGIC;
	reloadwd: out STD_LOGIC
	);
end globalcontrolreg;

architecture behavioral of globalcontrolreg is

signal ctrclearreg: STD_LOGIC;
signal ctrlatchreg: STD_LOGIC;
signal pwmclearreg: STD_LOGIC;
signal irqclearreg: STD_LOGIC;
signal reloadwdreg: STD_LOGIC;
begin
	agcrreg: process (clk, 
							ibus,
							reset,
							loadgcr,
							ctrclearreg,
							ctrlatchreg,
							pwmclearreg,
							irqclearreg,
							reloadwdreg
							)
							
	begin
		if clk'event and clk = '1' then
			
			if loadgcr = '1' then
 		   	ctrclearreg <= ibus(0);
				ctrlatchreg <= ibus(1);
				pwmclearreg <= ibus(2);
				irqclearreg <= ibus(3);
				reloadwdreg	<= ibus(4);
			end if;	
			if ctrclearreg = '1' then 
				ctrclearreg <= '0';
			end if;
			if ctrlatchreg = '1' then 
				ctrlatchreg <= '0';
			end if;
			if pwmclearreg = '1' then 
				pwmclearreg <= '0';
			end if;
			if irqclearreg = '1' then 
				irqclearreg <= '0';
			end if;
			if reloadwdreg = '1' then 
				reloadwdreg <= '0';
			end if;
		end if;
		if ctrclearreg = '1' or reset = '1' then
			ctrclear <= '1';
		else 
			ctrclear <= '0';
		end if;
		if ctrlatchreg = '1' or reset = '1' then
			ctrlatch <= '1';
		else 
			ctrlatch <= '0';
		end if;
		if pwmclearreg = '1' or reset = '1' then
			pwmclear <= '1';
		else 
			pwmclear <= '0';
		end if;
		if irqclearreg = '1' or reset = '1' then
			irqclear <= '1';
		else 
			irqclear <= '0';
		end if;
		if reloadwdreg = '1' or reset = '1' then
			reloadwd <= '1';
		else 
			reloadwd <= '0';
		end if;
	
	end process;
end behavioral;

