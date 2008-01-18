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

entity wordpr is
    generic (
	 		size : integer := 24;
			buswidth : integer := 32
			);
	 port (		
	   	clear: in STD_LOGIC;
			clk: in STD_LOGIC;
			ibus: in STD_LOGIC_VECTOR (buswidth-1 downto 0);
			obus: out STD_LOGIC_VECTOR (buswidth-1 downto 0);
			loadport: in STD_LOGIC;
			loadddr: in STD_LOGIC;
			loadaltdatasrc: in STD_LOGIC;
			loadopendrainmode: in STD_LOGIC;
			loadinvert: in STD_LOGIC;
			readddr: in STD_LOGIC;
			portdata: out STD_LOGIC_VECTOR (size-1 downto 0);
			altdata: in STD_LOGIC_VECTOR (size-1 downto 0)
 			);
end wordpr;

architecture behavioral of wordpr is

signal outreg: std_logic_vector (size-1 downto 0);
signal ddrreg: std_logic_vector (size-1 downto 0);
signal tsoutreg: std_logic_vector (size-1 downto 0);
signal opendrainsel: std_logic_vector (size-1 downto 0);
signal altdatasel: std_logic_vector (size-1 downto 0);
signal invertsel: std_logic_vector (size-1 downto 0);
signal tdata: std_logic_vector (size-1 downto 0);
signal tddr: std_logic_vector (size-1 downto 0);

begin
	awordioport: process (
								clk,
								ibus,
								loadport,
								loadddr,
								readddr,
								outreg,ddrreg)
	begin
		if rising_edge(clk) then
			if loadport = '1'  then
				outreg <= ibus(size-1 downto 0);
			end if; 
			if loadddr = '1' then
				ddrreg <= ibus(size-1 downto 0);
			end if;
			if loadaltdatasrc = '1' then
				altdatasel <= ibus(size-1 downto 0);
			end if;
			if loadopendrainmode = '1' then
				opendrainsel <= ibus(size-1 downto 0);
			end if;
			if loadinvert = '1' then
				invertsel <= ibus(size-1 downto 0);
			end if;
			if clear = '1' then 
				ddrreg <= (others => '0'); 
				opendrainsel <= (others => '0');	
			end if;
		end if; -- clk

		for i in 0 to size-1 loop
			if altdatasel(i) = '0' then
				if invertsel(i) = '0' then			-- normal output data, normal outputs can be inverted
					tdata(i) <= outreg(i);
				else
					tdata(i) <= not outreg(i);
				end if;						
			else
				if invertsel(i) = '0' then			-- alternate output data, alternate outputs can be inverted
					tdata(i) <= altdata(i);
				else
					tdata(i) <= not altdata(i);
				end if;		 
			end if;
			if opendrainsel(i) = '0' then				-- normal DDR	
				if ddrreg(i) = '1' then 
					tsoutreg(i) <= tdata(i);
				else
					tsoutreg(i) <= 'Z';
				end if;
			else	
				if tdata(i) = '0' then 				-- open drain option = active pulldown
					tsoutreg(i) <= '0';
				else
					tsoutreg(i) <= 'Z';
				end if;
			end if;
		end loop;
		
		portdata <= tsoutreg;
		obus <= (others => 'Z');
		if readddr = '1' then
			obus(size-1 downto 0) <= ddrreg;
			obus(buswidth -1 downto size) <= (others => '0');
		end if;

	end process;
end behavioral;
