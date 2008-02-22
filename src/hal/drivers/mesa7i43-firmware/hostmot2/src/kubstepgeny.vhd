library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;


--
-- Copyright (C) 2007, Peter C. Wallace, Mesa Electronics
-- http://www.mesanet.com
--
-- This program is is licensed under a disjunctive dual license giving you
-- the choice of one of the two following sets of free software/open source
-- licensing terms:
--
--    * GNU General Public License (GPL), version 2.0 or later
--    * 3-clause BSD License
-- 
--
-- The GNU GPL License:
-- 
--     This program is free software; you can redistribute it and/or modify
--     it under the terms of the GNU General Public License as published by
--     the Free Software Foundation; either version 2 of the License, or
--     (at your option) any later version.
-- 
--     This program is distributed in the hope that it will be useful,
--     but WITHOUT ANY WARRANTY; without even the implied warranty of
--     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--     GNU General Public License for more details.
-- 
--     You should have received a copy of the GNU General Public License
--     along with this program; if not, write to the Free Software
--     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
-- 
-- 
-- The 3-clause BSD License:
-- 
--     Redistribution and use in source and binary forms, with or without
--     modification, are permitted provided that the following conditions
--     are met:
-- 
--         * Redistributions of source code must retain the above copyright
--           notice, this list of conditions and the following disclaimer.
-- 
--         * Redistributions in binary form must reproduce the above
--           copyright notice, this list of conditions and the following
--           disclaimer in the documentation and/or other materials
--           provided with the distribution.
-- 
--         * Neither the name of Mesa Electronics nor the names of its
--           contributors may be used to endorse or promote products
--           derived from this software without specific prior written
--           permission.
-- 
-- 
-- Disclaimer:
-- 
--     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
--     "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
--     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
--     FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
--     COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
--     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
--     BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
--     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
--     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
--     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
--     ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
--     POSSIBILITY OF SUCH DAMAGE.
-- 


entity stepgen is
        generic (
			buswidth : integer;
			timersize : integer;
			tablewidth : integer;
			asize : integer;
			rsize : integer
			);
			Port ( clk : in std_logic;
	 		  ibus : in std_logic_vector(buswidth-1 downto 0);
           obus : out std_logic_vector(buswidth-1 downto 0);
           loadsteprate : in std_logic;
			  loadaccum : in std_logic;
           loadstepmode : in std_logic;
			  loaddirsetuptime : in std_logic;
			  loaddirholdtime : in std_logic;
			  loadpulseactivetime : in std_logic;
			  loadpulseidletime : in std_logic;
			  loadtable : in std_logic;	
			  loadtablemax : in std_logic;	
           readsteprate : in std_logic;
			  readaccum : in std_logic;
           readstepmode : in std_logic;
			  readdirsetuptime : in std_logic;
			  readdirholdtime : in std_logic;
			  readpulseactivetime : in std_logic;
			  readpulseidletime : in std_logic;			  
			  readtable : in std_logic;	
			  readtablemax : in std_logic;	
           basicrate : in std_logic;
           hold : in std_logic;
           stout : out std_logic_vector(tablewidth-1 downto 0)
          );
end stepgen;

architecture Behavioral of stepgen is


-- Step Generator	related signals

	signal stepaccum: std_logic_vector(asize-1 downto 0);
	signal nextaccum: std_logic_vector(asize-1 downto 0);
	alias stepmsbs: std_logic_vector(1 downto 0) is stepaccum(rsize-1 downto rsize -2);
	alias stepmsb: std_logic is stepaccum(rsize -1);
	alias nextmsb: std_logic is nextaccum(rsize -1);
	signal dstepmsb : std_logic;
	signal ddshold : std_logic;	 
	signal steprate: std_logic_vector(rsize -1 downto 0);
	alias  stepdir: std_logic is steprate(rsize -1);
	signal dstepdir  : std_logic;
	signal stepdirout  : std_logic;
	signal pulsewait : std_logic;
	signal steppulse : std_logic := '0';
	signal dpulsewait : std_logic := '0';
	signal dirsetupwait  : std_logic;
	signal dirholdwait  : std_logic;
	signal ddirholdwait  : std_logic;
	signal dirshwait  : std_logic;
	signal dirhold  : std_logic;
	signal dirshcount: std_logic_vector(timersize-1 downto 0);
	signal pulsewidthcount: std_logic_vector(timersize-1 downto 0);
	signal dirsetuptime: std_logic_vector(timersize -1 downto 0);
	signal dirholdtime: std_logic_vector(timersize -1 downto 0);
	signal pulseactivetime: std_logic_vector(timersize -1 downto 0);
	signal pulseidletime: std_logic_vector(timersize -1 downto 0);
	signal stepmode: std_logic_vector(1 downto 0);
	signal localout: std_logic_vector(tablewidth-1 downto 0);
	signal wewouldcount : std_logic;
 	signal dirchange : std_logic;
 	signal waitforhold : std_logic;
 	signal waitforpulse : std_logic;
	signal tableptr: std_logic_vector(3 downto 0);
	signal tablemax: std_logic_vector(3 downto 0);
	signal tabledata: std_logic_vector(tablewidth-1 downto 0);
	
	component SRL16E
--
    generic (INIT : bit_vector);


--
    port (D   : in  std_logic;
          CE  : in  std_logic;
          CLK : in  std_logic;
          A0  : in  std_logic;
          A1  : in  std_logic;
          A2  : in  std_logic;
          A3  : in  std_logic;
          Q   : out std_logic); 
	end component;
	
			
begin

	steptable: for i in 0 to tablewidth -1 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => ibus(i),
          CE  => loadtable,
          CLK => clk,
          A0  => tableptr(0),
          A1  => tableptr(1),
          A2  => tableptr(2),
          A3  => tableptr(3),
          Q   => tabledata(i)
			);	
  	end generate;

	astepgen: process (clk,stepdirout, steprate, nextaccum, stepaccum,
	                   dirholdwait, ddirholdwait, pulsewait, dpulsewait,
							 dirsetupwait, pulsewidthcount, dirshcount, dirhold,
							 readaccum, tabledata, stepmode, steppulse, stepmsbs)
	begin
		if rising_edge(clk) then

			if basicrate = '1' and hold = '0' and ddshold = '0' then	 	-- our basic step rate DDS
				stepaccum <= nextaccum;
				dstepdir <= stepdir;					-- only updated when we add
			end if;

			if pulsewait = '1' then 							  							-- our two timers
				pulsewidthcount <= pulsewidthcount -1;									-- we share dirshcount between setup and hold functions 
			end if;				
			
			if (pulsewait = '0') and (dpulsewait = '1') and (steppulse = '1') then			
				pulsewidthcount <= pulseidletime;										-- output pulse idle time
				steppulse <= '0';																-- clear our output pulse
			end if;
			
			if dirshwait = '1' then 
				dirshcount <= dirshcount -1;
			end if;				

			if (stepmsb = '0' and dstepmsb = '1' and dstepdir = '0') 			-- we counted up
			or (stepmsb = '1' and dstepmsb = '0' and dstepdir = '1') then 		-- we counted down		  		-- the output of the DDS
				pulsewidthcount <= pulseactivetime;										-- output pulse active time								
				steppulse <= '1';
				dirshcount <= dirholdtime;													-- set pulse to dir change hold timer
				dirhold <= '1';	 															--	set our flag to indicate			
			else
				if dirholdwait = '0'  then  												-- no change during hold time 
					if stepdirout /= stepdir then  										-- we changed the external direction signal
						dirshcount <= dirsetuptime;										-- set dir change to next pulse setup time
						dirhold <= '0';														-- set our flag to indicate
						stepdirout <= stepdir;
					end if;																		-- our timer is for setup time
				end if;			
			end if;																				-- our timer is for hold time			

			
			if (stepmsb = '0' and dstepmsb = '1' and dstepdir = '0') then 		-- we counted up		
				if (tableptr = tablemax) then
					tableptr <= x"0";
				else
					tableptr <= tableptr +1;
				end if;
			end if;
			if (stepmsb = '1' and dstepmsb = '0' and dstepdir = '1') then		-- we counted down
				if (tableptr = x"0") then
					tableptr <= tablemax;
				else
					tableptr <= tableptr -1;
				end if;
			end if;	
			
			
			if loadstepmode = '1' then					   								-- our register writes
				stepmode <= ibus(1 downto 0);
			end if;
			if loadsteprate = '1' then
				steprate <= ibus(rsize -1 downto 0);
			end if;
			if loadaccum = '1' then
				stepaccum(asize -1 downto asize-buswidth) <= ibus;
				steppulse <= '0';
			end if;
			if loaddirsetuptime = '1' then					   
				dirsetuptime <= ibus(timersize -1 downto 0);
			end if;
			if loaddirholdtime = '1' then					   
				dirholdtime <= ibus(timersize -1 downto 0);
			end if;
			if loadpulseactivetime = '1' then					   
				pulseactivetime <= ibus(timersize -1 downto 0);
			end if;
			if loadpulseidletime = '1' then					   
				pulseidletime <= ibus(timersize -1 downto 0);
			end if;			

			if loadtablemax = '1' then					   
				tablemax <= ibus(3 downto 0);
				tableptr <= x"0";
			end if;
			
			dpulsewait <= pulsewait;
			dstepmsb <= stepmsb;
			ddirholdwait <= dirholdwait;			-- ddirholdwait needed to cover case where dirhold wait has become 0
		end if; -- clk									-- but setup timer has not started yet (Probably more elegant to use state machine)

		dirchange <= stepdirout xor stepdir;			
		wewouldcount <= nextmsb xor stepmsb;
		waitforhold <= (dirholdwait or ddirholdwait) and dirchange;
		waitforpulse <= pulsewait or dpulsewait;

		nextaccum <= signed(stepaccum)+ signed(steprate);				-- to lookahead

		if (wewouldcount = '1') and 
		(((waitforhold  = '1') or (dirsetupwait = '1') or (waitforpulse = '1')))
		then					-- need to pause
			ddshold <= '1';
		else
			ddshold <= '0';												
		end if;			
		
		if pulsewidthcount = 0  then
			pulsewait <= '0';
		else
			pulsewait <= '1';
		end if;

		if dirshcount = 0 then
			dirshwait <= '0';
		else
			dirshwait <= '1';
		end if;

		dirholdwait <= (dirhold and dirshwait);
		dirsetupwait <= (not dirhold) and dirshwait;
		
      obus <= (others => 'Z');     

--		if readsteprate =  '1' then
--			obus(rsize -1 downto 0) <= steprate;
--		end if;

		if readaccum =  '1' then
			obus <= stepaccum(asize -1 downto asize-buswidth);
		end if;

--		if readstepmode =  '1' then							-- register readbacks commented out for size
--			obus(3 downto 0) <= stepmode;
--			obus(31 downto 4) <= (others => '0');
--		end if;

--		if readdirsetuptime =  '1' then
--			obus(timersize -1 downto 0) <= dirsetuptime;
--			obus(31 downto timersize) <= (others => '0');
--		end if;
--		if readdirholdtime =  '1' then
--			obus(timersize -1 downto 0) <= dirholdtime;
--			obus(31 downto timersize) <= (others => '0');
--		end if;
--		if readpulseactivetime =  '1' then
--			obus(timersize -1 downto 0) <= pulsewidth;
--			obus(31 downto timersize) <= (others => '0');
--		end if;
--		if readpulseidletime =  '1' then
--			obus(timersize -1 downto 0) <= pulseidle;
--			obus(31 downto timersize) <= (others => '0');
--		end if;
		localout <= tabledata;  -- this is the default unless: 
		case stepmode is
			when "00"  =>
				localout(0) <= steppulse; 								-- step
				localOut(1) <= stepdirout;	 							-- dir
			when "01" =>
				localout(0) <= steppulse and (not stepdirout); 	-- count up
				localOut(1) <= steppulse and stepdirout;	   	-- count down
			when  "10" =>
				case stepmsbs is
					when "00" => localout(1 downto 0) <= "00";					-- quadrature
					when "01" => localout(1 downto 0) <= "01";			
					when "10" => localout (1 downto 0)<= "11";
					when "11" => localout(1 downto 0) <= "10";
					when others => null; 
				end case;
			when others => null;
		end case;
		stout <= localout;

	end process astepgen;
	
end Behavioral;
