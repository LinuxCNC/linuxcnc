
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

use work.IDROMConst.all;	

entity IDROM is 
	generic (
				idromtype : integer;
				offsettomodules : integer;
				offsettopindesc : integer;						
				boardnamelow : std_logic_vector(31 downto 0);
				boardnamehigh : std_logic_vector(31 downto 0);
				fpgasize : integer;
				fpgapins : integer;
				ioports : integer;
				iowidth : integer;
				portwidth  : integer;	
				clocklow  : integer;
				clockhigh  : integer;
				inststride0  : integer;
				inststride1  : integer;
				regstride0  : integer;
				regstride1  : integer;
				pindesc : PinDescType;
				moduleid : moduleIDType );
	port (
 		clk  : in std_logic; 
		we   : in std_logic; 
		re	  : in std_logic; 
 		radd : in std_logic_vector(7 downto 0);
		wadd : in std_logic_vector(7 downto 0);
 		din  : in std_logic_vector(31 downto 0);  
 		dout : out std_logic_vector(31 downto 0)
		);
end IDROM; 
 
 architecture syn of IDROM is 			-- 256 x 32 spram
 constant empty : std_logic_vector(31 downto 0) := x"00000000";
 type ram_type is array (0 to 255) of std_logic_vector(31 downto 0); 
 signal RAM : ram_type := 
 ( 		
 					
			CONV_STD_LOGIC_VECTOR(idromtype,32),			
			CONV_STD_LOGIC_VECTOR(offsettomodules,32),
			CONV_STD_LOGIC_VECTOR(offsettopindesc,32),
			boardnamelow,
			boardnamehigh,			
			CONV_STD_LOGIC_VECTOR(fpgasize,32),
			CONV_STD_LOGIC_VECTOR(fpgapins,32),
			CONV_STD_LOGIC_VECTOR(ioports,32),
			CONV_STD_LOGIC_VECTOR(iowidth,32),
			CONV_STD_LOGIC_VECTOR(portwidth,32),
			CONV_STD_LOGIC_VECTOR(clocklow,32),
			CONV_STD_LOGIC_VECTOR(clockhigh,32),			
			CONV_STD_LOGIC_VECTOR(inststride0,32),			
			CONV_STD_LOGIC_VECTOR(inststride1,32),			
			CONV_STD_LOGIC_VECTOR(regstride0,32),			
			CONV_STD_LOGIC_VECTOR(regstride1,32),			

--			module IDs starting at doubleword 16
--			32 module id records = 96 doubles total

			moduleid(0).NumInstances&moduleid(0).Clock&moduleid(0).Version&moduleid(0).GTag,
			moduleid(0).Strides&moduleid(0).NumRegisters&moduleid(0).BaseAddr,
			moduleid(0).MultRegs,
	
			moduleid(1).NumInstances&moduleid(1).Clock&moduleid(1).Version&moduleid(1).GTag,
			moduleid(1).Strides&moduleid(1).NumRegisters&moduleid(1).BaseAddr,
			moduleid(1).MultRegs,

			moduleid(2).NumInstances&moduleid(2).Clock&moduleid(2).Version&moduleid(2).GTag,
			moduleid(2).Strides&moduleid(2).NumRegisters&moduleid(2).BaseAddr,
			moduleid(2).MultRegs,

			moduleid(3).NumInstances&moduleid(3).Clock&moduleid(3).Version&moduleid(3).GTag,
			moduleid(3).Strides&moduleid(3).NumRegisters&moduleid(3).BaseAddr,
			moduleid(3).MultRegs,

			moduleid(4).NumInstances&moduleid(4).Clock&moduleid(4).Version&moduleid(4).GTag,
			moduleid(4).Strides&moduleid(4).NumRegisters&moduleid(4).BaseAddr,
			moduleid(4).MultRegs,

			moduleid(5).NumInstances&moduleid(5).Clock&moduleid(5).Version&moduleid(5).GTag,
			moduleid(5).Strides&moduleid(5).NumRegisters&moduleid(5).BaseAddr,
			moduleid(5).MultRegs,

			moduleid(6).NumInstances&moduleid(6).Clock&moduleid(6).Version&moduleid(6).GTag,
			moduleid(6).Strides&moduleid(6).NumRegisters&moduleid(6).BaseAddr,
			moduleid(6).MultRegs,

			moduleid(7).NumInstances&moduleid(7).Clock&moduleid(7).Version&moduleid(7).GTag,
			moduleid(7).Strides&moduleid(7).NumRegisters&moduleid(7).BaseAddr,
			moduleid(7).MultRegs,

			moduleid(8).NumInstances&moduleid(8).Clock&moduleid(8).Version&moduleid(8).GTag,
			moduleid(8).Strides&moduleid(8).NumRegisters&moduleid(8).BaseAddr,
			moduleid(8).MultRegs,

			moduleid(9).NumInstances&moduleid(9).Clock&moduleid(9).Version&moduleid(9).GTag,
			moduleid(9).Strides&moduleid(9).NumRegisters&moduleid(9).BaseAddr,
			moduleid(9).MultRegs,

			moduleid(10).NumInstances&moduleid(10).Clock&moduleid(10).Version&moduleid(10).GTag,
			moduleid(10).Strides&moduleid(10).NumRegisters&moduleid(10).BaseAddr,
			moduleid(10).MultRegs,

			moduleid(11).NumInstances&moduleid(11).Clock&moduleid(11).Version&moduleid(11).GTag,
			moduleid(11).Strides&moduleid(11).NumRegisters&moduleid(11).BaseAddr,
			moduleid(11).MultRegs,

			moduleid(12).NumInstances&moduleid(12).Clock&moduleid(12).Version&moduleid(12).GTag,
			moduleid(12).Strides&moduleid(12).NumRegisters&moduleid(12).BaseAddr,
			moduleid(12).MultRegs,

			moduleid(13).NumInstances&moduleid(13).Clock&moduleid(13).Version&moduleid(13).GTag,
			moduleid(13).Strides&moduleid(13).NumRegisters&moduleid(13).BaseAddr,
			moduleid(13).MultRegs,

			moduleid(14).NumInstances&moduleid(14).Clock&moduleid(14).Version&moduleid(14).GTag,
			moduleid(14).Strides&moduleid(14).NumRegisters&moduleid(14).BaseAddr,
			moduleid(14).MultRegs,

			moduleid(15).NumInstances&moduleid(15).Clock&moduleid(15).Version&moduleid(15).GTag,
			moduleid(15).Strides&moduleid(15).NumRegisters&moduleid(15).BaseAddr,
			moduleid(15).MultRegs,

			moduleid(16).NumInstances&moduleid(16).Clock&moduleid(16).Version&moduleid(16).GTag,
			moduleid(16).Strides&moduleid(16).NumRegisters&moduleid(16).BaseAddr,
			moduleid(16).MultRegs,

			moduleid(17).NumInstances&moduleid(17).Clock&moduleid(17).Version&moduleid(17).GTag,
			moduleid(17).Strides&moduleid(17).NumRegisters&moduleid(17).BaseAddr,
			moduleid(17).MultRegs,

			moduleid(18).NumInstances&moduleid(18).Clock&moduleid(18).Version&moduleid(18).GTag,
			moduleid(18).Strides&moduleid(18).NumRegisters&moduleid(18).BaseAddr,
			moduleid(18).MultRegs,

			moduleid(19).NumInstances&moduleid(19).Clock&moduleid(19).Version&moduleid(19).GTag,
			moduleid(19).Strides&moduleid(19).NumRegisters&moduleid(19).BaseAddr,
			moduleid(19).MultRegs,

			moduleid(20).NumInstances&moduleid(20).Clock&moduleid(20).Version&moduleid(20).GTag,
			moduleid(20).Strides&moduleid(20).NumRegisters&moduleid(20).BaseAddr,
			moduleid(20).MultRegs,

			moduleid(21).NumInstances&moduleid(21).Clock&moduleid(21).Version&moduleid(21).GTag,
			moduleid(21).Strides&moduleid(21).NumRegisters&moduleid(21).BaseAddr,
			moduleid(21).MultRegs,

			moduleid(22).NumInstances&moduleid(22).Clock&moduleid(22).Version&moduleid(22).GTag,
			moduleid(22).Strides&moduleid(22).NumRegisters&moduleid(22).BaseAddr,
			moduleid(22).MultRegs,

			moduleid(23).NumInstances&moduleid(23).Clock&moduleid(23).Version&moduleid(23).GTag,
			moduleid(23).Strides&moduleid(23).NumRegisters&moduleid(23).BaseAddr,
			moduleid(23).MultRegs,

			moduleid(24).NumInstances&moduleid(24).Clock&moduleid(24).Version&moduleid(24).GTag,
			moduleid(24).Strides&moduleid(24).NumRegisters&moduleid(24).BaseAddr,
			moduleid(24).MultRegs,

			moduleid(25).NumInstances&moduleid(25).Clock&moduleid(25).Version&moduleid(25).GTag,
			moduleid(25).Strides&moduleid(25).NumRegisters&moduleid(25).BaseAddr,
			moduleid(25).MultRegs,

			moduleid(26).NumInstances&moduleid(26).Clock&moduleid(26).Version&moduleid(26).GTag,
			moduleid(26).Strides&moduleid(26).NumRegisters&moduleid(26).BaseAddr,
			moduleid(26).MultRegs,

			moduleid(27).NumInstances&moduleid(27).Clock&moduleid(27).Version&moduleid(27).GTag,
			moduleid(27).Strides&moduleid(27).NumRegisters&moduleid(27).BaseAddr,
			moduleid(27).MultRegs,

			moduleid(28).NumInstances&moduleid(28).Clock&moduleid(28).Version&moduleid(28).GTag,
			moduleid(28).Strides&moduleid(28).NumRegisters&moduleid(28).BaseAddr,
			moduleid(28).MultRegs,

			moduleid(29).NumInstances&moduleid(29).Clock&moduleid(29).Version&moduleid(29).GTag,
			moduleid(29).Strides&moduleid(29).NumRegisters&moduleid(29).BaseAddr,
			moduleid(29).MultRegs,

			moduleid(30).NumInstances&moduleid(30).Clock&moduleid(30).Version&moduleid(30).GTag,
			moduleid(30).Strides&moduleid(30).NumRegisters&moduleid(30).BaseAddr,
			moduleid(30).MultRegs,

			moduleid(31).NumInstances&moduleid(31).Clock&moduleid(31).Version&moduleid(31).GTag,
			moduleid(31).Strides&moduleid(31).NumRegisters&moduleid(31).BaseAddr,
			moduleid(31).MultRegs,


--			16 empty doublewords from 112 through 127 
--       may eventually delete this to allow 144 pin cards
			empty,empty,empty,empty,empty,empty,empty,empty,
			empty,empty,empty,empty,empty,empty,empty,empty,
--			pindesc starting at doubleword 128
			
			pindesc(0),pindesc(1),pindesc(2),pindesc(3),pindesc(4),pindesc(5),pindesc(6),pindesc(7),
			pindesc(8),pindesc(9),pindesc(10),pindesc(11),pindesc(12),pindesc(13),pindesc(14),pindesc(15),
			pindesc(16),pindesc(17),pindesc(18),pindesc(19),pindesc(20),pindesc(21),pindesc(22),pindesc(23),
			pindesc(24),pindesc(25),pindesc(26),pindesc(27),pindesc(28),pindesc(29),pindesc(30),pindesc(31),
			pindesc(32),pindesc(33),pindesc(34),pindesc(35),pindesc(36),pindesc(37),pindesc(38),pindesc(39),
			pindesc(40),pindesc(41),pindesc(42),pindesc(43),pindesc(44),pindesc(45),pindesc(46),pindesc(47),
			pindesc(48),pindesc(49),pindesc(50),pindesc(51),pindesc(52),pindesc(53),pindesc(54),pindesc(55),
			pindesc(56),pindesc(57),pindesc(58),pindesc(59),pindesc(60),pindesc(61),pindesc(62),pindesc(63),
			pindesc(64),pindesc(65),pindesc(66),pindesc(67),pindesc(68),pindesc(69),pindesc(70),pindesc(71),
			pindesc(72),pindesc(73),pindesc(74),pindesc(75),pindesc(76),pindesc(77),pindesc(78),pindesc(79),
			pindesc(80),pindesc(81),pindesc(82),pindesc(83),pindesc(84),pindesc(85),pindesc(86),pindesc(87),
			pindesc(88),pindesc(89),pindesc(90),pindesc(91),pindesc(92),pindesc(93),pindesc(94),pindesc(95),
			pindesc(96),pindesc(97),pindesc(98),pindesc(99),pindesc(100),pindesc(101),pindesc(102),pindesc(103),
			pindesc(104),pindesc(105),pindesc(106),pindesc(107),pindesc(108),pindesc(109),pindesc(110),pindesc(111),
			pindesc(112),pindesc(113),pindesc(114),pindesc(115),pindesc(116),pindesc(117),pindesc(118),pindesc(119),
			pindesc(120),pindesc(121),pindesc(122),pindesc(123),pindesc(124),pindesc(124),pindesc(126),pindesc(127)
			
   );


 signal dradd : std_logic_vector(7 downto 0);
 signal readout : std_logic_vector(31 downto 0); 
 
 begin 
 	process (clk,RAM, re) 
 	begin 
 		if (clk'event and clk = '1') then  
 			if (we = '1') then 
 				RAM(conv_integer(wadd)) <= din; 
 			end if;  
 			dradd <= radd;	
 		end if; 
 		readout <= RAM(conv_integer(dradd)); 
		dout <= (others => 'Z');
		if re = '1' then
			dout <= readout;
		end if;
	end process; 
 
end;
 

