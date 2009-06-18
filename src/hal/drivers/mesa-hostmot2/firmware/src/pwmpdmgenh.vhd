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

entity pwmpdmgenh is
	generic (
		buswidth : integer;
		refwidth : integer
		);    
	port (
		clk: in std_logic;
		hclk: in std_logic;
		refcount: in std_logic_vector (refwidth-1 downto 0);
		ibus: in std_logic_vector (buswidth -1 downto 0);
		loadpwmval: in std_logic;
		pcrloadcmd: std_logic;
		pdmrate : in std_logic;
		pwmouta: out std_logic;
		pwmoutb: out std_logic
	);
end pwmpdmgenh;

architecture behavioral of pwmpdmgenh is

signal pwmval: std_logic_vector (refwidth-2 downto 0);
signal prepwmval: std_logic_vector (refwidth-2 downto 0);
signal fixedrefcount: std_logic_vector (refwidth-2 downto 0);
signal pdmaccum: std_logic_vector (refwidth-1 downto 0);
alias  pdmbit: std_logic is pdmaccum(refwidth-1);
signal maskedrefcount: std_logic_vector (refwidth-2 downto 0);
signal pwm: std_logic;
signal dir: std_logic;
signal toggle: std_logic;
signal mask: std_logic_vector (refwidth-2 downto 0);  
signal predir: std_logic;
signal premodereg: std_logic_vector(5 downto 0);
signal modereg: std_logic_vector(5 downto 0);
alias  pwmwidth: std_logic_vector(1 downto 0) is modereg(1 downto 0);
alias  pwmmode: std_logic is modereg(2);
alias  pwmoutmode: std_logic_vector(1 downto 0) is modereg(4 downto 3);
alias  doublebuf: std_logic is modereg(5);
signal loadpwmreq: std_logic;
signal oldloadpwmreq: std_logic;
signal olderloadpwmreq: std_logic;
signal loadpcrreq: std_logic;
signal oldloadpcrreq: std_logic;
signal olderloadpcrreq: std_logic;
signal oldtoggle: std_logic;
signal cyclestart: std_logic;

begin
	apwmgen: process  (clk,hclk,refcount,ibus,loadpwmval,pwmval,pwm,
	                   fixedrefcount,mask,pwmmode,toggle,oldtoggle,
							 pwmwidth,olderloadpwmreq,olderloadpcrreq,
							 pwmoutmode,dir,pdmaccum
							)
							
	begin
		if rising_edge(hclk) then	  		
			if pdmrate = '1' then
				pdmaccum <= ('0'&pdmaccum(refwidth-2 downto 0)) + ('0'&pwmval);
			end if;	
			if oldloadpwmreq = '1'  and olderloadpwmreq = '1' then
				pwmval <= prepwmval;
				dir <= predir;
				oldloadpwmreq <= '0';
			end if;  		
			if oldloadpcrreq = '1' and olderloadpcrreq ='1' then
 		   	modereg <= premodereg;			
			end if;
			
			olderloadpwmreq <= oldloadpwmreq;
			olderloadpcrreq <= oldloadpcrreq;
			if (loadpwmreq and (cyclestart or (not doublebuf))) = '1' then
				oldloadpwmreq <= '1';
			end if;

--			oldloadpwmreq <= loadpwmreq and (cyclestartflag or (not doublebuf));			
			oldloadpcrreq <= loadpcrreq;			
			-- was combinatorial but now pipelined to meet 100 MHz timing 	
			if (UNSIGNED(maskedrefcount) < UNSIGNED(pwmval)) then 
				pwm <= '1'; 
			else 
				pwm <= '0';
			end if;
			case pwmmode is
				when '0' => fixedrefcount <= refcount(refwidth-2 downto 0);
				when '1' =>
					if toggle = '1' then		-- symmetrical mode
						fixedrefcount <=	(not refcount(refwidth-2 downto 0));
						else
						fixedrefcount <= refcount(refwidth-2 downto 0);
	  				end if;
				when others => null;	
			end case;
			oldtoggle <= toggle;
		end if; -- hclk	
		
		maskedrefcount <= fixedrefcount and mask;

		if pwmmode = '1' then
			cyclestart <= ((not toggle) and oldtoggle); -- falling edge of toggle sym mode
		else
			cyclestart <= toggle xor oldtoggle;         -- both edges of toggle ramp mode   
		end if;
		
		case pwmwidth is
			when "00" => 
							mask(refwidth-2 downto refwidth-4) <= "000";
							mask(refwidth-5 downto 0) <= ( others => '1');
							toggle <= refcount(refwidth-4); 		
			when "01" =>
							mask(refwidth-2 downto refwidth-3) <= "00";
							mask(refwidth-4 downto 0) <= ( others => '1');
							toggle <= refcount(refwidth-3);	 	
			when "10" =>
							mask(refwidth-2) <= '0';
							mask(refwidth-3 downto 0) <= ( others => '1');
							toggle <= refcount(refwidth-2);	 	
			when "11" =>
							mask <= (others => '1');
							toggle <= refcount(refwidth-1); 		
		 	when others => null;
		end case;

		if rising_edge(clk) then -- 33/48/50 mhz local bus clock			
			if loadpwmval = '1' then			 
				prepwmval <= ibus((refwidth-2)+16 downto 16); -- Fixme! only works for buswidth 32	
				predir <= ibus(BusWidth -1);
				loadpwmreq <= '1';
			end if;	
			if pcrloadcmd = '1' then
				premodereg <= ibus(5 downto 0);
				loadpcrreq <= '1';
			end if;	
		end if; -- clk

		if olderloadpwmreq = '1' then -- asynchronous request clear, could use flancter but dont need async clear 
			loadpwmreq <= '0';
		end if;		

		if olderloadpcrreq = '1' then -- asynchronous request clear ""
			loadpcrreq <= '0';
		end if;	
		
		case pwmoutmode is
			when "00" =>
				pwmouta <= pwm; 			-- normal sign magnitude
				pwmoutb <= dir;
			when "01" =>
				pwmouta <= dir; 			-- reversed pwm/dir = locked antiphase
				pwmoutb <= pwm;
			when "10" =>
				if dir = '1' then			-- up/down mode
					pwmouta <= pwm;
					pwmoutb <= '0';
				else
					pwmouta <= '0';
					pwmoutb <= pwm;
				end if;
			when "11" =>	
				pwmouta <= pdmbit;
				pwmoutb <= dir;
			when others => null;
		end case;		
--	pwmoutc <= ena;
	end process;
end behavioral;

