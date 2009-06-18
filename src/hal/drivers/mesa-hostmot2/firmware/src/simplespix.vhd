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

entity simplespi is
    generic (
		buswidth : integer
		);    
	port ( 
		clk : in std_logic;
		ibus : in std_logic_vector(buswidth-1 downto 0);
      obus : out std_logic_vector(buswidth-1 downto 0);
		loadbitcount : in std_logic;
		loadbitrate : in std_logic;
		loaddata : in std_logic;
		readdata : in std_logic;           
		readbitcount : in std_logic;
		readbitrate : in std_logic;
      spiclk : out std_logic;
      spiin : in std_logic;
		spiout: out std_logic;
		spiframe: out std_logic;
		davout: out std_logic
       );
end simplespi;

architecture behavioral of simplespi is

constant DivWidth: integer := 8;
-- ssi interface related signals

signal RateDivReg : std_logic_vector(DivWidth -1 downto 0);
signal RateDiv : std_logic_vector(DivWidth -1 downto 0);
signal ModeReg : std_logic_vector(8 downto 0);
alias BitcountReg : std_logic_vector(5 downto 0) is ModeReg(5 downto 0);
alias CPOL : std_logic is ModeReg(6);
alias CPHA : std_logic is ModeReg(7);
alias DontClearFrame : std_logic is ModeReg(8);
signal BitCount : std_logic_vector(5 downto 0);
signal ClockFF: std_logic; 
signal SPISreg: std_logic_vector(buswidth-1 downto 0);
signal Frame: std_logic;
signal EFrame: std_logic;  
signal Dav: std_logic; 
signal SPIInLatch: std_logic;
signal FirstLeadingEdge: std_logic;
 
begin 

	aspiinterface: process (clk, readdata, ModeReg, ClockFF, Frame,
	                        SPISreg, readbitcount, BitcountReg,
									Dav, readbitrate, RateDivReg)
	begin
		if rising_edge(clk) then

			if loaddata = '1' then 
				SPISreg <= ibus;
				BitCount <= BitCountReg;
				Frame <= '1';
				EFrame <= '1';
				Dav <= '0';
				ClockFF <= '0';
				FirstLeadingEdge <= '1';
				RateDiv <= RateDivReg;
			end if;

			if Frame = '1' then 
				if RateDiv = 0 then
					RateDiv <= RateDivReg;
					SPIInLatch <= spiin;
					if ClockFF = '0' then
						if BitCount(5) = '1' then
							Frame <= '0';								-- frame cleared 1/2 SPI clock after GO
							if DontClearFrame = '0' then
								EFrame <= '0';
							end if;	
							Dav <= '1';
						else						
							ClockFF <= '1';
						end if;	
						if CPHA = '1'  and FirstLeadingEdge = '0' then				-- shift out on leading edge for CPHA = 1 case
							SPISreg <= SPISreg(30 downto 0) & (SPIInLatch);
						end if;
						FirstLeadingEdge <= '0';						
					else
						ClockFF <= '0';
						BitCount <= BitCount -1;
						if CPHA = '0' then				-- shift out on trailing edge for CPHA = 0 case
							SPISreg <= SPISreg(30 downto 0) & (SPIInLatch);
						end if;	
					end if;	
				else					
					RateDiv <= RateDiv -1;
				end if;
			end if;


			if loadbitcount =  '1' then 
				ModeReg <= ibus(8 downto 0);
			end if;
			if loadbitrate =  '1' then 
				RateDivReg <= ibus(DivWidth -1 downto 0);				 
			end if;

		end if; -- clk

		obus <= (others => 'Z');
      if readdata =  '1' then
			obus <= SPISReg;
		end if;
		if	readbitcount =  '1' then
			obus(8 downto 0) <= ModeReg;
			obus(buswidth -1) <= Dav;			
		end if;
      if readbitrate =  '1' then
			obus(DivWidth-1 downto 0) <= RateDivReg;
		end if;
		spiclk <= ClockFF xor CPOL;
		spiframe <= not EFrame;
		davout <= Dav;
--		for i in 0 to buswidth -1 loop
--			if i = BitCountReg then
--				spiout <= SPISReg(i);
--			end if;
--		end loop;	
		spiout <= SPISReg(conv_integer(BitCountReg(4 downto 0))); -- select the MSB of the current size	
	end process aspiinterface;
	
end Behavioral;
