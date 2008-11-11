
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

entity watchdog is
	generic (
		buswidth : integer
		);    
    port ( clk : in  std_logic;
           ibus : in  std_logic_vector (buswidth-1 downto 0);
           obus : out  std_logic_vector (buswidth-1 downto 0);
           loadtime : in  std_logic;
           readtime : in  std_logic;
			  loadstatus: in  std_logic;
			  readstatus: in  std_logic; 
			  cookie: in  std_logic; 
           wdbite : out  std_logic;
			  wdlatchedbite : out std_logic );
end watchdog;

architecture Behavioral of watchdog is
constant otherz: std_logic_vector (buswidth-2 downto 0) := (others => '0');
signal wdtimer: std_logic_vector (buswidth-1 downto 0) := '1' & otherz; 
signal wdtime: std_logic_vector (buswidth-1 downto 0); 
alias  wdtimermsb: std_logic is  wdtimer(buswidth-1);
signal wdstatus: std_logic := '0';
signal oldwdtimermsb: std_logic := '0';

begin
	atimeout: process (clk,wdtimer,wdstatus,
	                   readtime, readstatus, 
							 wdtimermsb, oldwdtimermsb)
	begin
		if rising_edge(clk) then
			
			oldwdtimermsb <= wdtimermsb;

			if wdtimermsb /= '1' then 
				wdtimer <= wdtimer -1;
			end if;

			if loadtime = '1' then
				wdtimer <= ibus;
			   wdtime <= ibus;
			end if;

			if cookie = '1' then
				if ibus(buswidth-1 downto buswidth-8) = x"5A" then
					wdtimer <= wdtime;
				end if;
			end if;	
			 
			if loadstatus = '1' then
				wdstatus <= ibus(0);
			end if;
			if wdtimermsb = '1' and oldwdtimermsb = '0' then  -- edge triggered
				wdbite <= '1';
				wdstatus <= '1';
			else
				wdbite <= '0';
			end if;			
		end if; -- clk
		obus <= (others =>'Z');

		if readtime = '1' then
			obus <= wdtimer;
		end if;
		if readstatus = '1' then
			obus(0) <= wdstatus;
			obus(buswidth -1 downto 1) <= (others => '0');
		end if;
		

		wdlatchedbite <= wdstatus;
	end process;

end Behavioral;
