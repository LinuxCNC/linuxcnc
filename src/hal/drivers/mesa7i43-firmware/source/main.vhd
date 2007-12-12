
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

entity main is
	 generic
		(  Nports : integer := 6 
		);
		
	Port (	 CLK : in std_logic;
				LEDS : out std_logic_vector(7 downto 0);
				IOBITS : inout std_logic_vector(47 downto 0);
				EPP_DATABUS : inout std_logic_vector(7 downto 0);
				EPP_DSTROBE : in std_logic;
				EPP_ASTROBE : in std_logic;
				EPP_WAIT : out std_logic;
				EPP_READ : in std_logic;
				RECONFIG : out std_logic;
				PARACONFIG : out std_logic;
				SPICLK : out std_logic;
				SPIIN : in std_logic;
				SPIOUT : out std_logic;
				SPICS : out std_logic
		 );
end main;

architecture Behavioral of main is

	function OneOfSixDecode(ena1 : std_logic;ena2 : std_logic; dec : std_logic_vector(2 downto 0)) return std_logic_vector is
	 variable result	: std_logic_vector(5 downto 0);
	 begin
		if ena1 = '1' and ena2 = '1' then
			case dec is
				when "000"  => result := "000001";
				when "001"  => result := "000010";
				when "010"  => result := "000100";
				when "011"  => result := "001000";
				when "100"  => result := "010000";
				when "101"  => result := "100000";				
				when others => result := "000000";
			end case;  
		 else
			result := "000000";
		end if;
		return result;
	  end OneOfSixDecode;
	
signal afcnt : std_logic_vector(1 downto 0);
signal afilter : std_logic_vector(1 downto 0);
signal dfcnt : std_logic_vector(1 downto 0);
signal dfilter : std_logic_vector(1 downto 0);
signal waitpipe : std_logic_vector(1 downto 0);
signal alatch : std_logic_vector(7 downto 0); 
signal seladd : std_logic_vector(7 downto 0);
signal aread : std_logic;
signal awrite : std_logic;
signal dread : std_logic;
signal dwrite : std_logic; 
signal dwritete : std_logic; 
signal depp_dstrobe : std_logic;
signal depp_astrobe : std_logic;


signal PortSel : std_logic;
signal PortDDRSel : std_logic;
signal ReconfigSel : std_logic;
signal idata: std_logic_vector(7 downto 0);

signal LoadPortCmd : std_logic_vector(5 downto 0);
signal LoadDDRCmd : std_logic_vector(5 downto 0);
signal ReadDDRCmd : std_logic_vector(5 downto 0);
signal ReadPortCmd : std_logic_vector(5 downto 0);		
signal ReConfigreg : std_logic := '0';

signal LoadSPIReg : std_logic;
signal ReadSPIReg : std_logic;
signal LoadSPICS : std_logic;
signal ReadSPICS : std_logic;

begin

	makeoports: for i in 0 to NPorts-1 generate
		oportx: entity ioport 
		generic map (Width => 8)		
		port map 
		(
			clear => '0',
			clk => CLK,
			ibus => EPP_DATABUS,
			obus => idata,
			loadport => LoadPortCmd(i),
			loadddr => LoadDDRCmd(i),
			readddr => ReadDDRCmd(i),
			portdata => IOBITS((((i+1)*8) -1) downto (i*8))
		);	
	end generate;	


	makeiports: for i in 0 to Nports -1 generate
		iportx: entity ioportrb			
		generic map (Width => 8)
		port map 
		(
			obus => idata,
			readport => ReadPortCmd(i),
			portdata => IOBITS((((i+1)*8) -1) downto (i*8))
		 );	
	end generate;

	asimplspi: entity simplespi
		generic map
		(
			buswidth => 8,
			div => 4,
			bits => 8
		)	
		port map 
		( 
			clk  => CLK,
			ibus  => EPP_DATABUS,
			obus  => idata,
			loaddata  => LoadSPIReg,
			readdata  => ReadSPIReg,
			loadcs  => LoadSPICS,
			readcs  => ReadSPICS,
			spiclk  => SPIClk,
			spiin  => SPIIn,
			spiout  => SPIOut,
			spics  =>SPICS 
		 );



	EPPInterface: process(clk, waitpipe, alatch, afilter, dfilter,  
								 EPP_READ, EPP_DSTROBE, EPP_ASTROBE)
	begin

		if rising_edge(CLK) then
			depp_dstrobe <= EPP_DSTROBE;			-- async so one level of FF before anything else
			depp_astrobe <= EPP_ASTROBE;
			afilter(1) <= afilter(0);
			dfilter(1) <= dfilter(0);
			waitpipe(1) <= waitpipe(0);
			if depp_dstrobe = '0' or depp_astrobe = '0' then
				waitpipe(0) <= '1';
			else
				waitpipe(0) <= '0';
			end if;
		

			if  depp_astrobe = '0' then
				if afcnt /= "11" then 
					afcnt <= afcnt +1;
				end if;
			else
				if afcnt /= "00" then 
					afcnt <= afcnt -1;
				end if;
			end if;
			if afcnt = "11" then	 
				afilter(0) <= '1';
			end if;
			if afcnt = "00" then 
				afilter(0) <= '0';
			end if;

			if  depp_dstrobe = '0' then
				if dfcnt /= "11" then 
					dfcnt <= dfcnt +1;
				end if;
			else
				if dfcnt /= "00" then 
					dfcnt <= dfcnt -1;
				end if;
			end if;
			if dfcnt = "11" then	 
				dfilter(0) <= '1';
			end if;
			if dfcnt = "00" then 
				dfilter(0) <= '0';
			end if;

		
			if awrite = '1' then 
				alatch <= EPP_DATABUS;
			end if;
		
			if dwritete = '1' and alatch(7) = '1' then	-- auto increment address on data access if address MSB is 1
				alatch(6 downto 0) <= alatch(6 downto 0) +1;
			end if;	
			
		end if; -- clk	
		
		EPP_WAIT <= waitpipe(1);
		
		if dfilter = "01" and EPP_READ = '0' then	  -- generate write 80 ns after leading edge of strobe
			dwrite <= '1';
		else
			dwrite <= '0';
		end if;

		if dfilter = "10" then								-- generate writete 80 ns after trailng edge of strobe
			dwritete <= '1';
		else
			dwritete <= '0';
		end if;

		if afilter = "01" and EPP_READ  = '0' then	-- generate write 80 ns after leading edge of strobe
			awrite <= '1';
		else
			awrite <= '0';
		end if;
		
		if EPP_READ = '1' and depp_dstrobe = '0' then
			dread <= '1';
		else
			dread <= '0';
		end if;
		
		if EPP_READ = '1' and depp_astrobe = '0' then
			aread <= '1';
		else
			aread <= '0';
		end if;		
		
	end process EPPInterface;

	iodecode: process(alatch,dwrite,dread)
	begin
		seladd <= '0' & alatch(6 downto 0);
		
		if seladd(7 downto 4) = x"1" then	-- 0x10 through 0x1F (0x15 max used)
			PortSel <= '1';
		else
			PortSel <= '0';
		end if;

		if seladd(7 downto 4) = x"2" then	 -- 0x20 through 0x2F (0x25 max used)
			PortDDRSel <= '1';
		else
			PortDDRSel <= '0';
		end if;

		if seladd = x"7D" and dwrite = '1'then
			LoadSPICS <= '1';
		else
			LoadSPICS <= '0';		
		end if;	

		if seladd = x"7D" and dread = '1'then
			ReadSPICS <= '1';
		else
			ReadSPICS <= '0';		
		end if;	

		if seladd = x"7E" and dwrite = '1'then
			LoadSPIReg <= '1';
		else
			LoadSPIReg <= '0';		
		end if;	

		if seladd = x"7E" and dread = '1'then
			ReadSPIReg <= '1';
		else
			ReadSPIReg <= '0';		
		end if;	

		
		if seladd = x"7F" then
			ReconfigSel <= '1';
		else
			ReconfigSel <= '0';		
		end if;	


		LoadPortCmd <= OneOfSixDecode(PortSel,dwrite,alatch(2 downto 0));
		ReadPortCmd <= OneOfSixDecode(PortSel,dread,alatch(2 downto 0));	 

		LoadDDRCmd <= OneOfSixDecode(PortDDRSel,dwrite,alatch(2 downto 0)); 
		ReadDDRCmd <= OneOfSixDecode(PortDDRSel,dread,alatch(2 downto 0)); 

	end process iodecode;
	
	doreconfig: process (CLK,ReConfigreg)
	begin
		if rising_edge(CLK) then
			if dwrite = '1' and ReconfigSel = '1' then
				if EPP_DATABUS = x"5A" then
					ReConfigreg <= '1';
				end if;
			end if;
		end if;		
		RECONFIG <= not ReConfigreg;
	end process doreconfig;
	
	BusDrive: process (aread,dread,idata,alatch)
	begin
		EPP_DATABUS <= "ZZZZZZZZ";
		if dread = '1' then 
			EPP_DATABUS <= idata;
		end if;
		if aread = '1' then 
			EPP_DATABUS <= alatch;
		end if;
		LEDS <= not alatch;
		PARACONFIG <= '1';
	end process BusDrive;	

end Behavioral;
