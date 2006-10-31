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

entity pwmref is
    Port (
	 		clk: in STD_LOGIC;
			refcount: out STD_LOGIC_VECTOR (9 downto 0);
			irqgen: out STD_LOGIC;
			ibus: in STD_LOGIC_VECTOR (15 downto 0);
			obus: out STD_LOGIC_VECTOR (15 downto 0);
			irqdivload: in STD_LOGIC;
			irqdivread: in STD_LOGIC;
			phaseload: in STD_LOGIC;
			phaseread: in STD_LOGIC
			);
end pwmref;

architecture behavioral of pwmref is

signal count: STD_LOGIC_VECTOR (9 downto 0);
signal irqdivisor: STD_LOGIC_VECTOR (7 downto 0);
signal irqcounter: STD_LOGIC_VECTOR (7 downto 0);
signal phaseacc: STD_LOGIC_VECTOR (16 downto 0);
alias  phasemsb: std_logic is phaseacc(16);
signal oldphasemsb: std_logic;
signal phaselatch: STD_LOGIC_VECTOR (15 downto 0);

begin
	apwmref: process  (clk,
							irqdivload,
							count,
							irqcounter,
							irqdivisor,
							ibus,
							irqdivread,
							phaseread) 
	begin
		if clk'event and clk = '1' then
			phaseacc <= phaseacc + phaselatch;
			oldphasemsb <= phasemsb;
			if oldphasemsb /= phasemsb then
				count <= count + 1;							
				if count = 0 then
					irqcounter <= irqcounter -1;
					if irqcounter = 0 then	
						irqgen <= '1'; 
						irqcounter <= irqdivisor;
					else 
						irqgen <= '0';
					end if; -- irqcounter = 0
				end if;	-- count = 0		
			end if;		-- old /= new
			
			if irqdivload = '1' then
 		   	irqdivisor <= ibus(7 downto 0);
				irqcounter <= irqdivisor;
			end if;
			if phaseload = '1' then
 		   	phaselatch <= ibus;
			end if;				

		end if;  -- clk
		if irqdivread = '1' and phaseread = '0' then
			obus(7 downto 0) <= irqdivisor;
			obus(15 downto 8) <= x"00";
		elsif phaseread = '1' and irqdivread = '0' then
			obus <= phaselatch;
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;
		refcount <= count;
	end process;

end behavioral;

