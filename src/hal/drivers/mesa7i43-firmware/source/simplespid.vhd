
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

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

--  Uncomment the following lines to use the declarations that are
--  provided for instantiating Xilinx primitive components.
--library UNISIM;
--use UNISIM.VComponents.all;

entity simplespi is
    generic (
      buswidth : integer;
      div : integer;
      bits : integer
      );   
   port ( 
      clk : in std_logic;
      ibus : in std_logic_vector(buswidth-1 downto 0);
      obus : out std_logic_vector(buswidth-1 downto 0);
      loaddata : in std_logic;
      readdata : in std_logic;
      loadcs : in std_logic;
      readcs : in std_logic;
      spiclk : out std_logic;
      spiin : in std_logic;
      spiout: out std_logic;
      spics: out std_logic
       );
end simplespi;

architecture behavioral of simplespi is

-- ssi interface related signals

signal BitCount: integer range 0 to 8 ;
signal RateDiv : integer range 0 to 7;
signal ClockFF: std_logic; 
signal SPISreg: std_logic_vector(buswidth-1 downto 0);
signal Go: std_logic; 
signal Dav: std_logic; 
signal SPIOutDel: std_logic; 
signal CS : std_logic := '1'; 

begin 

   aspiinterface: process (clk, readdata, Go, DAV,
                           SPISreg, readcs, CS, ClockFF)
   begin
      if rising_edge(clk) then
         SPIOutDel <= SPISReg(bits-1); -- delay output 1 clock to meet SPI EEPROM hold time
         if loaddata = '1' then 
            SPISreg <= ibus;
            BitCount <= (bits -1);
            Go <= '1';
            Dav <= '0';
            ClockFF <= '0';
            RateDiv <= div;
         end if;
   
         if loadcs = '1' then
            CS <= ibus(0);
         end if;
         
         if Go = '1' then 
            if RateDiv = 0 then
               RateDiv <= div;
               if ClockFF = '0' then
                  ClockFF <= '1';
                  SPISreg <= SPISreg(6 downto 0) & spiin;
               else
                  ClockFF <= '0';
                  BitCount <= BitCount -1;
                  if BitCount = 0 then
                     Go <= '0';
                     Dav <= '1';
                  end if;   
               end if;   
            else               
               RateDiv <= RateDiv -1;
            end if;
         end if;

      end if; -- clk

      obus <= (others => 'Z');
      if readdata =  '1' then
         obus <= SPISReg;
      end if;
      if   readcs =  '1' then
         obus(0) <= CS;
         obus(1) <= Go;
         obus(2) <= Dav;         
      end if;
 
      spiclk <= ClockFF;
      spiout <= SPIOutDel;
      spics <= CS;
   end process aspiinterface;

end Behavioral;
