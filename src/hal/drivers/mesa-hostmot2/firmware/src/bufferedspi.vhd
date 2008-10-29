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

entity bufferedspi is
    generic (
		cswidth : integer := 4
		);    
	port ( 
		clk : in std_logic;
		ibus : in std_logic_vector(31 downto 0);
      obus : out std_logic_vector(31 downto 0);
		addr : in std_logic_vector(3 downto 0);
		hostpush : in std_logic;
		hostpop : in std_logic;
		loadasend : in std_logic;
		loaddesc : in std_logic;
		clear : in std_logic;
		readcount : in std_logic;
      spiclk : out std_logic;
      spiin : in std_logic;
		spiout: out std_logic;
		spiframe: out std_logic;
		spicsout: out std_logic_vector(cswidth-1 downto 0)
       );
end bufferedspi;

architecture behavioral of bufferedspi is

constant DivWidth: integer := 8;


-- spi interface related signals

signal RateDiv : std_logic_vector(DivWidth -1 downto 0);
signal ModeReg : std_logic_vector(31 downto 0);
signal LoadData : std_logic;
signal StartCycle : std_logic;
alias  BitcountReg : std_logic_vector(5 downto 0) is ModeReg(5 downto 0);
alias CPOL : std_logic is ModeReg(6);
alias CPHA : std_logic is ModeReg(7);
alias RateDivReg : std_logic_vector(DivWidth -1 downto 0) is ModeReg(15 downto 8);
alias CSReg : std_logic_vector(cswidth -1 downto 0) is  ModeReg(cswidth-1 +16 downto 16);
alias CSTimerReg : std_logic_vector(4 downto 0) is ModeReg(28 downto 24);
alias DontEcho : std_logic is ModeReg(31);
signal BitCount : std_logic_vector(5 downto 0);
signal ClockFF: std_logic; 
signal SPISreg: std_logic_vector(31 downto 0);
signal Frame: std_logic; 
signal Dav: std_logic; 
signal SPIInLatch: std_logic;
signal FirstLeadingEdge: std_logic;
signal CSTimer: std_logic_vector(4 downto 0);
alias CSTimerDone: std_logic is CSTimer(4);

-- input FIFO related signals
	signal ipopadd: std_logic_vector(3 downto 0) := x"f";
	signal ipopdata: std_logic_vector(31 downto 0);
	signal idatacounter: std_logic_vector(4 downto 0);
	signal ipush: std_logic;   
	signal ififohasdata: std_logic; 

-- output FIFO related signals
	signal opushdata: std_logic_vector(35 downto 0);
	signal opopadd: std_logic_vector(3 downto 0) := x"f";
	signal opopdata: std_logic_vector(35 downto 0);
	signal odatacounter: std_logic_vector(4 downto 0); 
	signal opop: std_logic;  
	signal ofifohasdata: std_logic; 
 
-- autosend table related signals
	
	signal autosenddata: std_logic_vector(35 downto 0);
	signal autosendadd: std_logic_vector(3 downto 0);
	signal autosendlength: std_logic_vector(3 downto 0);
	
	
-- channel descriptor related signals
	signal desc: std_logic_vector(31 downto 0);
	alias CSTimerFromDesc : std_logic_vector(4 downto 0) is desc(28 downto 24);
	signal descptr: std_logic_vector(3 downto 0);
	
	

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

	ofifo: for i in 0 to 35 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => opushdata(i),
          CE  => hostpush,
          CLK => clk,
          A0  => opopadd(0),
          A1  => opopadd(1),
          A2  => opopadd(2),
          A3  => opopadd(3),
          Q   => opopdata(i)
			);	
  	end generate;

	ififo: for i in 0 to 31 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => SPISReg(i),
          CE  => ipush,
          CLK => clk,
          A0  => ipopadd(0),
          A1  => ipopadd(1),
          A2  => ipopadd(2),
          A3  => ipopadd(3),
          Q   => ipopdata(i)
			);	
  	end generate;

	autosendtable: for i in 0 to 35 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => opushdata(i),
          CE  => loadasend,
          CLK => clk,
          A0  => autosendadd(0),
          A1  => autosendadd(1),
          A2  => autosendadd(2),
          A3  => autosendadd(3),
          Q   => autosenddata(i)
			);	
  	end generate;

	chandesc: for i in 0 to 31 generate
		asr16e: SRL16E generic map (x"0000") port map(
 			 D	  => ibus(i),
          CE  => loaddesc,
          CLK => clk,
          A0  => descptr(0),			-- the address that was pushed
          A1  => descptr(1),
          A2  => descptr(2),
          A3  => descptr(3),
          Q   => desc(i)
			);	
  	end generate;	

	outfifo: process (clk,opopdata,odatacounter)
	begin
		if rising_edge(clk) then
			
			if hostpush = '1'  and opop = '0'  then
				if odatacounter /= 16 then	-- a push
					-- always increment the data counter if not full
					odatacounter <= odatacounter +1;
					opopadd <= opopadd +1;						-- popadd must follow data down shiftreg
				end if;	
			end if;		 		
						   
			if (opop = '1') and (hostpush = '0') and (ofifohasdata = '1') then	-- a pop
				-- always decrement the data counter if not empty
				odatacounter <= odatacounter -1;
				opopadd <= opopadd -1;
			end if;

-- if both push and pop are asserted we dont change either counter
	  
			if clear = '1' then -- a clear fifo
				opopadd  <= (others => '1');
				odatacounter <= (others => '0');
			end if;	
	
		end if; -- clk rise
		if odatacounter = 0 then
			ofifohasdata <= '0';
		else
			ofifohasdata <= '1';
		end if;
	end process outfifo;

	infifo: process (clk,ipopdata,odatacounter)
	begin
		if rising_edge(clk) then
			
			if ipush = '1'  and hostpop = '0'  then
				if idatacounter /= 16 then	-- a push
					-- always increment the data counter if not full
					idatacounter <= idatacounter +1;
					ipopadd <= ipopadd +1;						-- popadd must follow data down shiftreg
				end if;	
			end if;		 		
						   
			if (hostpop = '1') and (ipush = '0') and (ififohasdata = '1') then	-- a pop
				-- always decrement the data counter if not empty
				idatacounter <= idatacounter -1;
				ipopadd <= ipopadd -1;
			end if;

-- if both push and pop are asserted we dont change either counter
	  
			if clear = '1' then -- a clear fifo
				ipopadd  <= (others => '1');
				idatacounter <= (others => '0');
			end if;	
	
		end if; -- clk rise
		if idatacounter = 0 then
			ififohasdata <= '0';
		else
			ififohasdata <= '1';
		end if;
	end process infifo;


	aspiinterface: process (clk,  ModeReg, ClockFF, Frame,
	                        SPISreg,  BitcountReg, opopdata,
									Dav,RateDivReg,addr, ibus, 
									hostpop, ipopdata, readcount, 
									idatacounter, odatacounter)
	begin
		if rising_edge(clk) then
			if StartCycle = '0' and ofifohasdata = '1' and opop = '0'
			and CSTimerDone= '1' and Frame = '0' and Dav = '0' and loaddata = '0' then				
			-- if SPI shift register is free and we have data and CS setup/hold time is passed
				ModeReg <= desc;
				CSTimer <= CSTimerFromDesc; -- load early for pre-frame delay		
				StartCycle <= '1';	
			end if;
			if StartCycle = '1' then			
				if CSTimerDone = '1' then 
					StartCycle <= '0';
					LoadData <= '1';
				end if;
			end if;
			
			if Dav = '1' then 
				if DontEcho = '0' then
					ipush <= '1';			-- push SPI recieve data on ififo
				end if;
				Dav <= '0';
			end if;
			
			if ipush = '1' then
				ipush <= '0';
			end if;
			
			if loaddata = '1' then 
				SPISreg <= opopdata(31 downto 0);
				BitCount <= BitCountReg;
				Frame <= '1';
				ClockFF <= '0';
				FirstLeadingEdge <= '1';
				RateDiv <= RateDivReg;
				loaddata <= '0';
				opop <= '1';
			end if;
			if opop = '1' then 
				opop <= '0';
			end if;
			
			if CSTimerDone = '0' then
				CSTimer <= CSTimer -1;
			end if;	
			
			if Frame = '1' then 										-- single shift register SPI
				if RateDiv = 0 then									-- maybe update to dual later to allow
					RateDiv <= RateDivReg;							-- receive data skew adjustment
					SPIInLatch <= spiin;
					if ClockFF = '0' then
						if BitCount(5) = '1' then
							Frame <= '0';								-- frame cleared 1/2 SPI clock after GO
							Dav <= '1';
							CSTimer <= CSTimerReg;					-- load timer from copy for post frame delay
						else						
							ClockFF <= '1';
						end if;	
						if CPHA = '1'  and FirstLeadingEdge = '0' then				-- shift out on leading edge for CPHA = 1 case
							SPISreg <= SPISreg(30 downto 0) & (SPIInLatch);
						end if;
						FirstLeadingEdge <= '0';						
					else										-- clock was high
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

			if clear = '1' then 
				Frame <= '0';
				ClockFF <= '0';
				Dav <= '0';
				LoadData <= '0';
				StartCycle <= '0';
				CSTimerDone <= '0';
				ipush <= '0';
			end if;
			
		end if; -- clk

		opushdata <= addr & ibus;					--  push address to select descriptor at far end of FIFO
		descptr <= opopdata(35 downto 32);		--  here!

		obus <= (others => 'Z');
      if hostpop =  '1' then
			obus <= ipopdata;
		end if;
		if	readcount =  '1' then
			obus(4 downto 0) <= idatacounter;
			obus(7 downto 5)  <= (others => '0');
			obus(12 downto 8) <= odatacounter;
			obus(31 downto 13) <= (others => '0');
		end if;
			
		spiclk <= ClockFF xor CPOL;
		spiframe <= not Frame;
		spicsout <= CSReg;
--		for i in 0 to 31 loop
--			if i = BitCountReg then
--				spiout <= SPISReg(i);
--			end if;
--		end loop;	
	spiout <= SPISReg(conv_integer(BitCountReg(4 downto 0)));
	end process aspiinterface;
	
end Behavioral;
