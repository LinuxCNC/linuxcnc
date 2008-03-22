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


-- Simple UART with 32 bit bus interface
-- 16 byte deep receive FIFO
-- Can read 4,3,2,1 bytes from FIFO depending on data register read offset
-- Base Address = 1 byte
-- Base Address +1 = 2 bytes
-- Base Address +2 = 3 bytes
-- Base Address +3 = 4 bytes
entity uartr is
	port (
		clk : in std_logic;
	 	ibus : in std_logic_vector(31 downto 0);
      obus : out std_logic_vector(31 downto 0);
		addr : in std_logic_vector(1 downto 0);
      popfifo : in std_logic;
		loadbitrate : in std_logic;
		readbitrate : in std_logic;
      clrfifo : in std_logic;
		readfifocount : in std_logic;
		loadmode : in std_logic;
		readmode : in std_logic;
		fifohasdata : out std_logic;
		rxmask : in std_logic;
      rxdata : in std_logic
		);
end uartr;

architecture Behavioral of uartr is

-- FIFO related signals
	signal pushdata: std_logic_vector(7 downto 0);
	signal popadd0: std_logic_vector(3 downto 0) := x"f";
	signal popadd1: std_logic_vector(3 downto 0) := x"f";
	signal popadd2: std_logic_vector(3 downto 0) := x"f";
	signal popadd3: std_logic_vector(3 downto 0) := x"f";
	
	signal popdata: std_logic_vector(31 downto 0);
	signal datacounter: std_logic_vector(4 downto 0);
	signal push: std_logic;  
	signal pop: std_logic;  
	signal popsize: std_logic_vector(2 downto 0);
	signal clear: std_logic;
	signal lfifoempty: std_logic; 
	signal lfifohasdata: std_logic; 

-- uart interface related signals

constant DDSWidth : integer := 16;

signal BitrateDDSReg : std_logic_vector(DDSWidth-1 downto 0);
signal BitrateDDSAccum : std_logic_vector(DDSWidth-1 downto 0);
alias  DDSMSB : std_logic is BitrateDDSAccum(15);
signal OldDDSMSB: std_logic;  
signal SampleTime: std_logic; 
signal BitCount : std_logic_vector(3 downto 0);
signal BytePointer : std_logic_vector(2 downto 0) := "000";
signal SReg: std_logic_vector(9 downto 0);
alias  SregData: std_logic_vector(7 downto 0)is SReg(8 downto 1);
alias  StartBit: std_logic is Sreg(0);
alias  StopBit: std_logic is Sreg(9);
signal RXPipe : std_logic_vector(1 downto 0);
signal Go: std_logic; 
signal DAV: std_logic;
signal ModeReg: std_logic_vector(3 downto 0);
alias FalseStart: std_logic is ModeReg(0); 
alias OverRun: std_logic is ModeReg(1);
alias RXMaskEn: std_logic is ModeReg(3); 

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

	fifosrl0: for i in 0 to 7 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => pushdata(i),
          CE  => push,
          CLK => clk,
          A0  => popadd0(0),
          A1  => popadd0(1),
          A2  => popadd0(2),
          A3  => popadd0(3),
          Q   => popdata(i)
			);	
  	end generate;
	
	fifosrl1: for i in 0 to 7 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => pushdata(i),
          CE  => push,
          CLK => clk,
          A0  => popadd1(0),
          A1  => popadd1(1),
          A2  => popadd1(2),
          A3  => popadd1(3),
          Q   => popdata(8+i)
			);	
  	end generate;
	
	fifosrl2: for i in 0 to 7 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => pushdata(i),
          CE  => push,
          CLK => clk,
          A0  => popadd2(0),
          A1  => popadd2(1),
          A2  => popadd2(2),
          A3  => popadd2(3),
          Q   => popdata(16+i)
			);	
  	end generate;
	
	fifosrl3: for i in 0 to 7 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => pushdata(i),
          CE  => push,
          CLK => clk,
          A0  => popadd3(0),
          A1  => popadd3(1),
          A2  => popadd3(2),
          A3  => popadd3(3),
          Q   => popdata(24+i)
			);	
  	end generate;
	
	afifo: process (clk,popdata,datacounter)
	begin
		if rising_edge(clk) then
			
			if push = '1'  and pop = '0' and datacounter /= 16 then	-- a push
		 		-- always increment the data counter if not full
				datacounter <= datacounter +1;
				popadd0 <= popadd0 +1;						-- popadd must follow data down shiftreg
			end if;		 		
						   
			if  (pop = '1') and (push = '0') then		-- a pop
				datacounter <= datacounter - popsize;
				popadd0 <= popadd0 - popsize;
			end if;

			if  (pop = '1') and (push = '1') then		-- simultaneaous pop and push
				datacounter <= datacounter - (addr);
				popadd0 <= popadd0 - (addr);
			end if;

			if clear = '1' then -- a clear fifo
				popadd0  <= (others => '1');
				datacounter <= (others => '0');
			end if;	
	

		end if; -- clk rise
		-- The way this mess works is that we have 4 byte wide FIFOs with duplicated data
		-- but the readout point is shifted by one byte for each succeeding FIFO so we can read up to
		-- 32 bits (4 bytes) at once. Wasteful, but SRL16s are cheap


		popadd1 <= popadd0 -1;		-- note that these are not forced to 0 on underflow
		popadd2 <= popadd0 -2;		-- so unused bytes of a less than 4 byte read
		popadd3 <= popadd0 -3;		-- will be stale recv data - not good for security reasons!
		                           -- if this matters, force to 0 on underflow will result in duplicated
											-- current data on unused bytes.
			
		popsize <= ('0'&addr) +1;
		
		if datacounter = 0 then
			lfifoempty <= '1';
		else
			lfifoempty <= '0';
		end if;
		fifohasdata <= not lfifoempty;		 
	end process afifo;


	asimpleuartrx: process (clk)
	begin
		if rising_edge(clk) then
			RXPipe <= RXPipe(0) & rxdata;  			-- Two stage rx data pipeline to compensate for
																-- two clock delay from start bit detection to acquire loop startup
																		
			if Go = '1' then 
				BitRateDDSAccum <= BitRateDDSAccum + BitRateDDSReg;
				if SampleTime = '1' then
					
					if BitCount = 0 then
						Go <= '0';
						DAV <= '1';
						if RXPipe(1) = '0' then
							OverRun <= '1';
						end if;	
					end if;	
					
					if BitCount = "1001" then	-- false start bit check
						if RXPipe(1) = '1' then
							Go <= '0';
							FalseStart <= '1';
						end if;
					end if;	
					
					SReg <= RXPipe(1) & SReg(9 downto 1);		-- right shift = LSb first
					BitCount <= BitCount -1;
					
				end if;	
			else
				BitRateDDSAccum <= (others => '0'); 
				BitCount <= "1001";
			end if;
			
			if Go = '0' and rxdata = '0' and (rxmask and RXMaskEn) = '0' then		-- start bit detection
				Go <= '1';
			end if;	
			
			if DAV = '1' then								-- DAV is just one clock wide
				DAV <= '0';
			end if;	
			
			OldDDSMSB <= DDSMSB;							-- for Phase accumulator MSB edge detection

			if loadbitrate =  '1' then 
				BitRateDDSReg <= ibus(DDSWidth-1 downto 0);				 
			end if;
			
			if loadmode=  '1' then 
				ModeReg <= ibus(3 downto 0);
			end if;

		end if; -- clk
		
		SampleTime <= (not OldDDSMSB) and DDSMSB;		-- sample on rising edge of DDS MSB
      pushdata <= SRegData;						
		push <= DAV;	
		pop <= popfifo;
		clear <= clrfifo;

		obus <= (others => 'Z');
		if	readfifocount =  '1' then
			obus(4 downto 0) <= datacounter;
--			obus(31 downto 5) <= (others => '0');
		end if;
      if readbitrate =  '1' then
			obus(DDSWidth-1 downto 0) <= BitRateDDSReg;
		end if;
		if popfifo =  '1' then
			obus <= popdata;
		end if;
		if readmode =  '1' then
			obus(3 downto 0) <= ModeReg;
			obus(6) <= rxmask;
			obus(7) <= not lfifoempty;
			
		end if;
		fifohasdata <= not lfifoempty;
			
	end process asimpleuartrx;
	
end Behavioral;
