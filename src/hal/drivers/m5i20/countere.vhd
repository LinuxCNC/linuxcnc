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


entity counter is
   port ( 
	obus: out STD_LOGIC_VECTOR (31 downto 0);
	ibus: in STD_LOGIC_VECTOR (31 downto 0);
	quada: in STD_LOGIC;
	quadb: in STD_LOGIC;
	index: in STD_LOGIC;
	ccrloadcmd: in STD_LOGIC;
	ccrreadcmd: in STD_LOGIC;
	countoutreadcmd: in STD_LOGIC;
	countlatchcmd: in STD_LOGIC;
	countclearcmd: in STD_LOGIC;
	countenable: in STD_LOGIC;
	indexmask: in STD_LOGIC;
	nads: in STD_LOGIC;
	clk: in STD_LOGIC
	);
end counter;

architecture behavioral of counter is

signal count: STD_LOGIC_VECTOR (31 downto 0);
signal up: STD_LOGIC;
signal down: STD_LOGIC;
signal countoutlatch: STD_LOGIC_VECTOR (31 downto 0);
signal outlatchdel1: STD_LOGIC;
signal outlatchdel2: STD_LOGIC;
signal quada1: STD_LOGIC;
signal quada2: STD_LOGIC;
signal quadacnt: STD_LOGIC_VECTOR (3 downto 0);
signal quadafilt: STD_LOGIC;
signal quadb1: STD_LOGIC;
signal quadb2: STD_LOGIC;
signal quadbcnt: STD_LOGIC_VECTOR (3 downto 0);
signal quadbfilt: STD_LOGIC;
signal index1: STD_LOGIC;
signal index2: STD_LOGIC;
signal indexcnt: STD_LOGIC_VECTOR (3 downto 0);
signal indexfilt: STD_LOGIC;
signal qcountup: STD_LOGIC; 
signal qcountdown: STD_LOGIC;
signal udcountup: STD_LOGIC; 
signal udcountdown: STD_LOGIC;
signal autocount: STD_LOGIC;
signal doclear: STD_LOGIC;
signal clearonindex: STD_LOGIC;	-- ccr register bits...
signal clearonce: STD_LOGIC;
signal indexgate: STD_LOGIC;
signal indexsrc: STD_LOGIC;
signal latchonread: STD_LOGIC;
signal quadfilter: STD_LOGIC;
signal countermode: STD_LOGIC;
signal indexpol: STD_LOGIC;
signal ccrloadcmd1: STD_LOGIC;
signal ccrloadcmd2: STD_LOGIC;
signal localhold: STD_LOGIC;
signal localclear: STD_LOGIC;
signal indexmaskenable: STD_LOGIC;
signal indexmaskpol: STD_LOGIC;
signal fixedindexmask: STD_LOGIC;
signal latchonce: STD_LOGIC;
signal latchonindex: STD_LOGIC;
signal flimit: STD_LOGIC_VECTOR (3 downto 0);


begin
	acounter: process 	(clk, countoutlatch)
	begin
		if clk'event and clk = '1' then

			outlatchdel1 <= countlatchcmd;
			outlatchdel2 <= outlatchdel1;	
			
			if indexgate = '0' then
				indexsrc	<= index;
			else
				if indexpol = '1' then
					indexsrc <=	not quada and not quadb and index;
				else
					indexsrc <= (quada or quadb) and index;
				end if;
			end if;

			if indexmaskpol = '1' then
				fixedindexmask <= indexmask;
			else
				fixedindexmask <= not indexmask;
			end if;
						
			if quadfilter = '1' then
				flimit <= "1111";
			else
				flimit <= "0011";
			end if;	
						
			quada1 <= quadafilt;
			quada2 <= quada1;			
			quadb1 <= quadbfilt;			
			quadb2 <= quadb1;		 					
			index1 <= indexfilt;			
			index2 <= index1;	

			-- deadended counter for A input filter --
			if (quada = '1') and (quadacnt < flimit) then 
				quadacnt <= quadacnt + 1;
			end if;
			if (quada = '0') and (quadacnt /= 0) then 
			   quadacnt <= quadacnt -1;
			end if;
			if quadacnt >= flimit then
			  quadafilt<= '1';
			end if;
			if quadacnt = 0 then
			  quadafilt<= '0';
			end if;
				
			-- deadended counter for B input filter --
			if (quadb = '1') and (quadbcnt < flimit ) then 
			   quadbcnt <= quadbcnt + 1;
			end if;
			if (quadb = '0') and (quadbcnt /= 0) then 
			   quadbcnt <= quadbcnt -1;
			end if;
			if quadbcnt >= flimit then
			  quadbfilt<= '1';
			end if;
			if quadbcnt = 0 then
			  quadbfilt <= '0';
			end if;	

			-- deadended counter for index input filter --
			if (indexsrc = '1') and (indexcnt < flimit ) then 
			   indexcnt <= indexcnt + 1;
			end if;
			if (indexsrc = '0') and (indexcnt /= 0) then 
			   indexcnt <= indexcnt -1;
			end if;
			if indexcnt >= flimit then
			  indexfilt<= '1';
			end if;
			if indexcnt = 0 then
			  indexfilt<= '0';
			end if;					

			if (countclearcmd = '1') or (localclear = '1') or 
			((clearonindex = '1')  and (index1 = '1') and (index2 = '0') and (indexpol = '1') and (indexmaskenable = '0')) or	-- rising edge of index
			((clearonindex = '1')  and (index1 = '0') and (index2 = '1') and (indexpol = '0') and (indexmaskenable = '0')) or	-- falling edge of index
			((clearonindex = '1')  and (index1 = '1') and (index2 = '0') and (indexpol = '1') and (indexmaskenable = '1') and (fixedindexmask = '1')) or		-- rising edge of index	when masked
			((clearonindex = '1')  and (index1 = '0') and (index2 = '1') and (indexpol = '0') and (indexmaskenable = '1') and (fixedindexmask = '1')) then	-- falling edge of index when masked				
				doclear <= '1';
				if clearonce = '1' then
					clearonindex <= '0';
				end if;
			else
				doclear <= '0';
			end if;		

			if ((latchonread = '1') and (nads = '0')) or		-- (let the synthesizer factor this out...)
			((outlatchdel2 = '0') and (outlatchdel1 = '1') and (latchonindex = '0')) or
			((latchonindex = '1')  and (index1 = '1') and (index2 = '0') and (indexpol = '1') and (indexmaskenable = '0')) or	-- rising edge of index
			((latchonindex = '1')  and (index1 = '0') and (index2 = '1') and (indexpol = '0') and (indexmaskenable = '0')) or	-- falling edge of index
			((latchonindex = '1')  and (index1 = '1') and (index2 = '0') and (indexpol = '1') and (indexmaskenable = '1') and (fixedindexmask = '1')) or		-- rising edge of index	when masked
			((latchonindex = '1')  and (index1 = '0') and (index2 = '1') and (indexpol = '0') and (indexmaskenable = '1') and (fixedindexmask = '1')) then	-- falling edge of index when masked				
	
				countoutlatch <= count;
				if latchonce = '1' then
					latchonindex <= '0';
				end if;
			end if;

			
			if countermode = '0' and countenable = '1' and localhold ='0' and doclear = '0' and (  
			(quada2 = '0' and quada1 = '1' and quadb2 = '0' and quadb1 = '0')  or
			(quada2 = '0' and quada1 = '0' and quadb2 = '1' and quadb1 = '0')  or
			(quada2 = '1' and quada1 = '1' and quadb2 = '0' and quadb1 = '1')  or
			(quada2 = '1' and quada1 = '0' and quadb2 = '1' and quadb1 = '1')) then		   
				qcountup <= '1';
			else 
				qcountup <= '0';
			end if;			

			if (countermode = '1' and countenable = '1' and localhold ='0' and doclear = '0' and 
			quadb2 = '1' and quada2 = '0' and quada1 = '1') then
				udcountup <= '1';
			else
				udcountup <= '0';
			end if;
				
			if countermode = '0' and countenable = '1' and localhold ='0' and doclear = '0' and ( 
			(quada2 = '0' and quada1 = '0' and quadb2 = '0' and quadb1 = '1')  or
			(quada2 = '0' and quada1 = '1' and quadb2 = '1' and quadb1 = '1')  or
			(quada2 = '1' and quada1 = '0' and quadb2 = '0' and quadb1 = '0')  or
			(quada2 = '1' and quada1 = '1' and quadb2 = '1' and quadb1 = '0')) then
				qcountdown <= '1';
			else
				qcountdown <= '0';
			end if;		
	
			if (countermode = '1' and countenable = '1' and localhold ='0' and doclear = '0' and  
			quadb2 = '0' and quada2 = '0' and quada1 = '1') then
				udcountdown <= '1';
			else
			   udcountdown <= '0';
			end if;		

			if up /= down then
				if up = '1' then 
					count <= count + 1; 
				else
					count <= count - 1;
				end if;
			end if;	

			if doclear = '1' then
				count <= x"00000000";
			end if;

			if ccrloadcmd = '1'  then		-- load ccr
				indexmaskpol <= ibus(15);
				indexmaskenable <= ibus(14);
				latchonce <= ibus(13);
				latchonindex <= ibus(12);
				autocount <= ibus(11);
				countermode <= ibus(10);
				quadfilter <= ibus(9);
				localhold <= ibus(8);
				indexgate <= ibus(7);
				clearonce <= ibus(6);
				clearonindex <= ibus(5);
				indexpol <= ibus(4);
				latchonread <= ibus(3);
				localclear <= ibus(2);
			end if;
			if localclear = '1' then											-- once were done clearing,, dont stick around
				localclear <= '0';
			end if;
		end if; --(clock edge)

		if (qcountup = '1' or udcountup = '1' or Autocount = '1') and localhold= '0' and doclear = '0' then
			up <= '1';
		else
		  	up <= '0';
		end if;
   
		if (qcountdown = '1' or udcountdown = '1' ) and Autocount = '0' and localhold ='0' and doclear = '0'  then
			down <= '1';
		else
		  	down <= '0';
		end if;	 
			
		obus <= "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ";
		if (countoutreadcmd = '1')  and (ccrreadcmd = '0') then
			obus <= countoutlatch;
		end if;
	
		if (ccrreadcmd = '1') and (countoutreadcmd = '0') then
				obus(15) <= indexmaskpol;
				obus(14) <= indexmask;
				obus(13) <= latchonce;
				obus(12) <= latchonindex;				
				obus(11) <= autocount;
				obus(10) <= countermode;
				obus(9) <= quadfilter;
				obus(8) <= localhold;
				obus(7) <= indexgate;	
				obus(6) <= clearonce;							
				obus(5) <= clearonindex;
				obus(4) <= indexpol;
				obus(3) <= latchonread;
				obus(2) <= index1;
				obus(1) <= quadb1;
				obus(0) <= quada1;

		end if;
	end process;
end behavioral;
