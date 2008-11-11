library IEEE;
use IEEE.std_logic_1164.ALL;
use IEEE.std_logic_ARITH.ALL;
use IEEE.std_logic_UNSIGNED.ALL;
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

entity pwmrefh is
			generic (
				buswidth : integer;
				refwidth : integer
				);
			Port (
				clk: in std_logic;
				hclk: in std_logic;
				refcount: out std_logic_vector (refwidth-1 downto 0);
				ibus: in std_logic_vector (buswidth -1 downto 0);
				pdmrate: out std_logic;
				pwmrateload: in std_logic;
				pdmrateload: in std_logic
			);
end pwmrefh;

architecture behavioral of pwmrefh is

signal count: std_logic_vector (refwidth -1 downto 0);
signal pwmrateacc: std_logic_vector (16 downto 0);
alias  pwmratemsb: std_logic is pwmrateacc(16);
signal oldpwmratemsb: std_logic;
signal pwmratelatch: std_logic_vector (15 downto 0);
signal prepwmratelatch: std_logic_vector (15 downto 0);
signal pwmratelatchloadreq: std_logic;
signal oldpwmratelatchloadreq: std_logic;
signal olderpwmratelatchloadreq: std_logic;
signal pdmrateacc: std_logic_vector (16 downto 0);
alias  pdmratemsb: std_logic is pdmrateacc(16);
signal oldpdmratemsb: std_logic;
signal pdmratelatch: std_logic_vector (15 downto 0);
signal prepdmratelatch: std_logic_vector (15 downto 0);
signal pdmratelatchloadreq: std_logic;
signal oldpdmratelatchloadreq: std_logic;
signal olderpdmratelatchloadreq: std_logic;
signal prate: std_logic;


begin
	apwmref: process  (clk,
                      olderpwmratelatchloadreq, 
							 olderpdmratelatchloadreq, 
							 prate,
							 hclk,
							 count,
							 ibus) 
	begin
		if rising_edge(hclk) then	  			-- 100 Mhz high speed clock
			
			if oldpwmratelatchloadreq = '1' and olderpwmratelatchloadreq = '1' then
				pwmratelatch <= prepwmratelatch;
			end if;
			oldpwmratelatchloadreq	<= pwmratelatchloadreq;
			olderpwmratelatchloadreq	<= oldpwmratelatchloadreq;
			pwmrateacc <= pwmrateacc + pwmratelatch;
			oldpwmratemsb <= pwmratemsb;
			if oldpwmratemsb /= pwmratemsb then
				count <= count + 1;	
			end if;		-- old /= new
			
			
			if oldpdmratelatchloadreq = '1' and olderpdmratelatchloadreq = '1' then
				pdmratelatch <= prepdmratelatch;
			end if;
			oldpdmratelatchloadreq	<= pdmratelatchloadreq;
			olderpdmratelatchloadreq	<= oldpdmratelatchloadreq;
			pdmrateacc <= pdmrateacc + pdmratelatch;
			oldpdmratemsb <= pdmratemsb;
			if oldpdmratemsb /= pdmratemsb then
				prate <= '1';
			else
				prate <= '0';
			end if;		-- old /= new			
		
		end if;  -- hclk

		if rising_edge(clk) then	  -- 33/48/50 Mhz local bus clock
			if pwmrateload = '1' then
 		   	prepwmratelatch <= ibus;
				pwmratelatchloadreq	<= '1';
			end if;				
			if pdmrateload = '1' then
 		   	prepdmratelatch <= ibus;
				pdmratelatchloadreq	<= '1';
			end if;				
		end if;  -- clk
		
		if olderpwmratelatchloadreq = '1' then	-- asyncronous request clear
			pwmratelatchloadreq	<= '0';
		end if;
		if olderpdmratelatchloadreq = '1' then	-- asyncronous request clear
			pdmratelatchloadreq	<= '0';
		end if;
		refcount <= count;
		pdmrate <= prate;
	end process;

end behavioral;

