library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.STD_LOGIC_ARITH.all;
use IEEE.STD_LOGIC_UNSIGNED.all;
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

-- Created from usb.obj
-- On12/ 3/2008

entity usbrom is
	port (
	addr: in std_logic_vector(9 downto 0);
	clk: in std_logic;
	din: in std_logic_vector(15 downto 0);
	dout: out std_logic_vector(15 downto 0);
	we: in std_logic);
end usbrom;

architecture syn of usbrom is
   type ram_type is array (0 to 1023) of std_logic_vector(15 downto 0);
   signal RAM : ram_type := 
   (
   x"0000", x"0000", x"63BD", x"0100", x"E7EC", x"203E", x"0000", x"0000",
   x"0000", x"0101", x"B07B", x"0100", x"B7EC", x"0100", x"B7EF", x"0109",
   x"B7F0", x"013D", x"B7F1", x"0100", x"B7F2", x"0101", x"B07B", x"707B",
   x"97D5", x"A7D1", x"3031", x"0100", x"B07B", x"0000", x"0000", x"0000",
   x"0000", x"0000", x"707C", x"B7DA", x"0101", x"B07B", x"77DA", x"C416",
   x"B416", x"0100", x"B7EF", x"0109", x"B7F0", x"013D", x"B7F1", x"0100",
   x"B7F2", x"77EF", x"E7D1", x"B7EF", x"77F0", x"F7D0", x"B7F0", x"77F1",
   x"F7D0", x"B7F1", x"77F2", x"F7D0", x"B7F2", x"4015", x"63BD", x"0100",
   x"B07A", x"01EE", x"E06F", x"2047", x"0000", x"01FF", x"B7FD", x"0150",
   x"0800", x"0104", x"0A00", x"0100", x"B7FC", x"0100", x"B416", x"0100",
   x"B417", x"0100", x"B7DD", x"635C", x"77DA", x"B7D7", x"01C0", x"A7DA",
   x"304D", x"0000", x"01FF", x"B7FC", x"011F", x"A7D7", x"B7E9", x"01C0",
   x"A7D7", x"B7E8", x"0000", x"0000", x"01C0", x"E7E8", x"206F", x"0000",
   x"0000", x"01FF", x"E7D7", x"206F", x"0000", x"0000", x"1047", x"0180",
   x"E7E8", x"208E", x"013F", x"A7D7", x"B7EA", x"01FF", x"B7DD", x"0000",
   x"77EA", x"C7D0", x"0200", x"0200", x"0200", x"0200", x"0200", x"0B00",
   x"0300", x"A7DF", x"0900", x"0700", x"A7E0", x"C7E1", x"0B00", x"0000",
   x"0000", x"7900", x"B7D7", x"0D01", x"0000", x"0000", x"77D7", x"B800",
   x"0C01", x"01C0", x"A7D7", x"B7E8", x"013F", x"A7D7", x"B7EA", x"0108",
   x"A7D7", x"B7F3", x"0104", x"A7D7", x"B7D8", x"0103", x"A7D7", x"B7D9",
   x"0120", x"A7D7", x"B7D6", x"0110", x"A7E9", x"B7DE", x"01C0", x"E7E8",
   x"20B6", x"0120", x"E7D6", x"20B6", x"0100", x"E7DD", x"30B5", x"0000",
   x"0000", x"7900", x"B800", x"0C01", x"10B6", x"6326", x"0140", x"E7E8",
   x"2122", x"0104", x"E7D8", x"20CA", x"0100", x"E7DD", x"30C8", x"0000",
   x"0000", x"7900", x"B800", x"7901", x"B801", x"0C02", x"0D02", x"10CA",
   x"6326", x"6326", x"0120", x"E7D6", x"2122", x"0100", x"E7D9", x"20DA",
   x"0110", x"E7DE", x"20D9", x"0000", x"7900", x"B800", x"0C01", x"0D01",
   x"10DA", x"6326", x"0101", x"E7D9", x"20EA", x"0110", x"E7DE", x"20E8",
   x"0000", x"7900", x"B800", x"7901", x"B801", x"0C02", x"0D02", x"10EA",
   x"6326", x"6326", x"0102", x"E7D9", x"2100", x"0110", x"E7DE", x"20FC",
   x"0000", x"7900", x"B800", x"7901", x"B801", x"7902", x"B802", x"7903",
   x"B803", x"0C04", x"0D04", x"1100", x"6326", x"6326", x"6326", x"6326",
   x"0103", x"E7D9", x"2122", x"0110", x"E7DE", x"211A", x"0000", x"7900",
   x"B800", x"7901", x"B801", x"7902", x"B802", x"7903", x"B803", x"7904",
   x"B804", x"7905", x"B805", x"7906", x"B806", x"7907", x"B807", x"0C08",
   x"0D08", x"1122", x"6326", x"6326", x"6326", x"6326", x"6326", x"6326",
   x"6326", x"6326", x"0100", x"E7DD", x"2128", x"0000", x"0100", x"B7D7",
   x"0100", x"B800", x"0100", x"E7DD", x"313E", x"0000", x"7900", x"B7D7",
   x"0D01", x"B800", x"0C01", x"0400", x"E7DB", x"0600", x"F7DC", x"513E",
   x"0000", x"0000", x"0110", x"8411", x"B411", x"1047", x"77D7", x"2091",
   x"0000", x"0100", x"B7DD", x"0100", x"B800", x"0150", x"0800", x"0104",
   x"0A00", x"0000", x"0000", x"7800", x"B7D7", x"0C01", x"0000", x"0000",
   x"01C0", x"A7D7", x"B7E8", x"0108", x"A7D7", x"B7F3", x"0104", x"A7D7",
   x"B7D8", x"0103", x"A7D7", x"B7D9", x"0120", x"A7D7", x"B7D6", x"01C0",
   x"E7E8", x"2250", x"011F", x"A7D7", x"B7E9", x"0120", x"A7D7", x"B7D6",
   x"0500", x"B7E4", x"0700", x"B7E5", x"010F", x"E7E9", x"5188", x"0000",
   x"0000", x"0110", x"0900", x"0104", x"0B00", x"0500", x"C7E9", x"0900",
   x"0700", x"D7D0", x"0B00", x"0120", x"E7D6", x"2184", x"0000", x"0000",
   x"7800", x"0C01", x"B900", x"1187", x"0000", x"7900", x"6390", x"124C",
   x"0120", x"E7D6", x"21EA", x"0000", x"0000", x"01F7", x"E7D7", x"2195",
   x"0000", x"7800", x"0C01", x"B07A", x"11E9", x"01F8", x"E7D7", x"21A7",
   x"0100", x"E41A", x"21A2", x"0000", x"0000", x"0000", x"7800", x"0C01",
   x"B7E2", x"11A6", x"0000", x"7800", x"0C01", x"B7E6", x"11E9", x"01F9",
   x"E7D7", x"21B9", x"0100", x"E41A", x"21B4", x"0000", x"0000", x"0000",
   x"7800", x"0C01", x"B7E3", x"11B8", x"0000", x"7800", x"0C01", x"B7E7",
   x"11E9", x"01FA", x"E7D7", x"21D1", x"0100", x"E41A", x"21C8", x"0000",
   x"7800", x"0C01", x"C7E2", x"B7E2", x"77E3", x"D7D0", x"B7E3", x"11D0",
   x"0000", x"7800", x"0C01", x"C7E6", x"B7E6", x"77E7", x"D7D0", x"B7E7",
   x"11E9", x"01FD", x"E7D7", x"21D9", x"0000", x"7800", x"0C01", x"B7F9",
   x"11E9", x"01FE", x"E7D7", x"21E9", x"0000", x"7800", x"0C01", x"B7DA",
   x"0000", x"0000", x"015A", x"E7DA", x"21E8", x"0000", x"0000", x"103E",
   x"11E9", x"124C", x"01D0", x"E7D7", x"21F1", x"0000", x"0137", x"6390",
   x"124C", x"01D1", x"E7D7", x"21F8", x"0000", x"0149", x"6390", x"124C",
   x"01D2", x"E7D7", x"21FF", x"0000", x"0134", x"6390", x"124C", x"01D3",
   x"E7D7", x"2206", x"0000", x"0133", x"6390", x"124C", x"01DA", x"E7D7",
   x"220D", x"0000", x"0101", x"6390", x"124C", x"01DB", x"E7D7", x"2214",
   x"0000", x"742B", x"6390", x"124C", x"01DD", x"E7D7", x"221B", x"0000",
   x"0100", x"6390", x"124C", x"01DE", x"E7D7", x"2222", x"0000", x"0104",
   x"6390", x"124C", x"01DC", x"E7D7", x"2229", x"0000", x"0110", x"6390",
   x"124C", x"01D8", x"E7D7", x"2237", x"0000", x"0100", x"E41A", x"2234",
   x"0000", x"0000", x"77E2", x"1235", x"77E6", x"6390", x"124C", x"01D9",
   x"E7D7", x"2245", x"0000", x"0100", x"E41A", x"2242", x"0000", x"0000",
   x"77E3", x"1243", x"77E7", x"6390", x"124C", x"01DF", x"E7D7", x"224C",
   x"0000", x"015A", x"6390", x"124C", x"77E4", x"0900", x"77E5", x"0B00",
   x"013F", x"A7D7", x"B7EA", x"0140", x"E7E8", x"231F", x"0104", x"E7D8",
   x"2269", x"0100", x"E41A", x"2263", x"0000", x"7800", x"B7E2", x"7801",
   x"B7E3", x"0C02", x"1269", x"0000", x"7800", x"B7E6", x"7801", x"B7E7",
   x"0C02", x"0100", x"E41A", x"227F", x"0000", x"0000", x"77FD", x"2276",
   x"0000", x"77E2", x"0900", x"77E3", x"0B00", x"127E", x"77E2", x"B068",
   x"0160", x"0900", x"77E3", x"B069", x"0100", x"0B00", x"1284", x"77E6",
   x"0900", x"77E7", x"C7E1", x"0B00", x"0100", x"E41A", x"2290", x"0000",
   x"0000", x"77FD", x"228F", x"01FF", x"E7E2", x"0103", x"F7E3", x"1294",
   x"01FF", x"E7E6", x"0103", x"F7E7", x"429E", x"0000", x"0000", x"0100",
   x"0900", x"0104", x"0B00", x"0120", x"8411", x"B411", x"0120", x"E7D6",
   x"22E1", x"0100", x"E7D9", x"22AA", x"0000", x"7800", x"B900", x"0C01",
   x"77D1", x"63A4", x"0101", x"E7D9", x"22B5", x"0000", x"7800", x"B900",
   x"7801", x"B901", x"0C02", x"77D2", x"63A4", x"0102", x"E7D9", x"22C4",
   x"0000", x"7800", x"B900", x"7801", x"B901", x"7802", x"B902", x"7803",
   x"B903", x"0C04", x"77D3", x"63A4", x"0103", x"E7D9", x"22DB", x"0000",
   x"7800", x"B900", x"7801", x"B901", x"7802", x"B902", x"7803", x"B903",
   x"7804", x"B904", x"7805", x"B905", x"7806", x"B906", x"7807", x"B907",
   x"0C08", x"77D4", x"63A4", x"77FD", x"32E0", x"0000", x"0000", x"B06E",
   x"131F", x"77FD", x"32E6", x"0000", x"0000", x"B06D", x"0000", x"0000",
   x"0000", x"0100", x"E7D9", x"22F1", x"0000", x"7900", x"6390", x"77D1",
   x"63A4", x"0101", x"E7D9", x"22FB", x"0000", x"7900", x"6390", x"7901",
   x"6390", x"77D2", x"63A4", x"0102", x"E7D9", x"2309", x"0000", x"7900",
   x"6390", x"7901", x"6390", x"7902", x"6390", x"7903", x"6390", x"77D3",
   x"63A4", x"0103", x"E7D9", x"231F", x"0000", x"7900", x"6390", x"7901",
   x"6390", x"7902", x"6390", x"7903", x"6390", x"7904", x"6390", x"7905",
   x"6390", x"7906", x"6390", x"7907", x"6390", x"77D4", x"63A4", x"7800",
   x"214B", x"0000", x"0000", x"1047", x"0000", x"0000", x"013B", x"B7F9",
   x"0111", x"B7FA", x"741B", x"B7FB", x"77F9", x"E7D1", x"B7F9", x"77FA",
   x"F7D0", x"B7FA", x"4343", x"0000", x"013B", x"B7F9", x"0111", x"B7FA",
   x"77FB", x"E7D1", x"B7FB", x"4343", x"0000", x"77FC", x"3343", x"0140",
   x"8411", x"B411", x"1047", x"0101", x"B07B", x"707B", x"97D5", x"A7D1",
   x"332C", x"0000", x"0100", x"B07B", x"0000", x"0000", x"0000", x"0000",
   x"0000", x"707C", x"B7DA", x"0101", x"B07B", x"77DA", x"C416", x"B416",
   x"77DA", x"B800", x"0C01", x"1800", x"013B", x"B7F9", x"0111", x"B7FA",
   x"741B", x"B7FB", x"77F9", x"E7D1", x"B7F9", x"77FA", x"F7D0", x"B7FA",
   x"4379", x"0000", x"013B", x"B7F9", x"0111", x"B7FA", x"77FB", x"E7D1",
   x"B7FB", x"4379", x"0000", x"77FC", x"3379", x"0140", x"8411", x"B411",
   x"1047", x"0101", x"B07B", x"707B", x"97D5", x"A7D1", x"3362", x"0000",
   x"0100", x"B07B", x"0000", x"0000", x"0000", x"0000", x"0000", x"707C",
   x"B7DA", x"0101", x"B07B", x"77DA", x"C416", x"B416", x"0000", x"1800",
   x"B7DA", x"0101", x"B07B", x"707B", x"97D5", x"A7D2", x"3391", x"0000",
   x"0000", x"0105", x"B07B", x"0107", x"B07B", x"77DA", x"B07C", x"C417",
   x"B417", x"0101", x"B07B", x"1800", x"B7F4", x"77F3", x"3800", x"0500",
   x"C7F4", x"0900", x"0700", x"D7D0", x"0B00", x"0100", x"E41A", x"23B7",
   x"0000", x"0000", x"0500", x"B7E2", x"0700", x"B7E3", x"13BC", x"0500",
   x"B7E6", x"0700", x"E7E1", x"B7E7", x"1800", x"0100", x"B7F4", x"B7D6",
   x"B7D7", x"B7D8", x"B7D9", x"B7DA", x"B7E9", x"B7EA", x"B7EB", x"B7F3",
   x"B7DD", x"B7DE", x"B7E8", x"B410", x"B411", x"B412", x"B413", x"B415",
   x"B416", x"B417", x"B418", x"B419", x"B41A", x"B42B", x"B7F9", x"B7D0",
   x"01FF", x"B41B", x"0101", x"B7D1", x"0102", x"B7D2", x"0104", x"B7D3",
   x"0108", x"B7D4", x"01FF", x"B7D5", x"01F0", x"B7DF", x"0103", x"B7E0",
   x"010C", x"B7E1", x"01C0", x"B7E8", x"017A", x"B7E2", x"0100", x"B7E3",
   x"0100", x"B7E6", x"0100", x"B7E7", x"01BE", x"B7DB", x"0107", x"B7DC",
   x"1800", x"0000", x"0000", x"0000", x"0000", x"0000", x"0000", x"0000");

signal daddr: std_logic_vector(9 downto 0);

begin
   ausbrom: process (clk)
   begin
      if (clk'event and clk = '1') then
         if (we = '1') then
            RAM(conv_integer(addr)) <= din;
         end if;
         daddr <= addr;
      end if; -- clk 
   end process;

   dout <= RAM(conv_integer(daddr));
end;
