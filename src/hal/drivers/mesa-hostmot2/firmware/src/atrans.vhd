
library IEEE;
use IEEE.STD_LOGIC_1164.all;  			
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

entity atrans is 
	generic (
				width : integer;
				depth: integer );
	port (
 		clk  : in std_logic; 
		wea  : in std_logic; 
		rea  : in std_logic; 
		reb  : in std_logic; 		
 		adda : in std_logic_vector(depth-1 downto 0);
		addb : in std_logic_vector(depth-1 downto 0);
 		din  : in std_logic_vector(width-1 downto 0);  
 		douta : out std_logic_vector(width-1 downto 0);
		doutb: out std_logic_vector(width-1 downto 0)
		);
end atrans; 
 
 architecture syn of atrans is 			
 type ram_type is array (0 to 2**depth-1) of std_logic_vector(width-1 downto 0); 
 signal RAM : ram_type; 


 signal dadda : std_logic_vector(depth-1 downto 0);
 signal daddb : std_logic_vector(depth-1 downto 0);
 signal outa : std_logic_vector(width-1 downto 0); 
 signal outb : std_logic_vector(width-1 downto 0); 
 
 begin 
 	process (clk) 
 	begin 
 		if (clk'event and clk = '1') then  
 			if (wea = '1') then 
 				RAM(conv_integer(adda)) <= din; 
 			end if;  
 			dadda <= adda;
			daddb <= addb;
 		end if; 
 		outa <= RAM(conv_integer(dadda)); 
 		outb <= RAM(conv_integer(daddb)); 
		douta <= (others => 'Z');
		if rea = '1' then
			douta <= outa;
		end if;
		doutb <= (others => 'Z');
		if reb = '1' then
			doutb <= outb;
		end if;
		
	end process; 
 
end;
 

