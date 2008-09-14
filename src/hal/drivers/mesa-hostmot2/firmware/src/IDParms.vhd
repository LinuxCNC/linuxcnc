
library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.std_logic_UNSIGNED.ALL;
use IEEE.std_logic_ARITH.ALL;
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

package IDROMParms is

	constant NullAddr : std_logic_vector(7 downto 0) := x"00";
	constant ReadIDAddr : std_logic_vector(7 downto 0) := x"01";
	constant LEDAddr : std_logic_vector(7 downto 0) := x"02";	
	constant LEDNumRegs : std_logic_vector(7 downto 0) := x"01";
	constant LEDMPBitMask : std_logic_vector(31 downto 0) := x"00000000";

	constant IDROMAddr : std_logic_vector(7 downto 0) := x"04";
	constant Cookie : std_logic_vector(31 downto 0) := x"55AACAFE";
	constant HostMotNameLow : std_logic_vector(31 downto 0) := x"54534F48"; 	-- HOST
	constant HostMotNameHigh : std_logic_vector(31 downto 0) := x"32544F4D"; 	-- MOT2
	
	constant BoardNameMesa : std_logic_vector(31 downto 0) := x"4153454D";		-- MESA
	constant BoardName4I65 : std_logic_vector(31 downto 0) := x"35364934";		-- 4I65
	constant BoardName4I68 : std_logic_vector(31 downto 0) := x"38364934";		-- 4I68
	constant BoardName5I20 : std_logic_vector(31 downto 0) := x"30324935";		-- 5I20
	constant BoardName5I22 : std_logic_vector(31 downto 0) := x"32324935";		-- 5I22
	constant BoardName5I23 : std_logic_vector(31 downto 0) := x"33324935";		-- 5I23
	constant BoardName7I43 : std_logic_vector(31 downto 0) := x"33344937";		-- 7I43
	constant BoardName7I60 : std_logic_vector(31 downto 0) := x"30364937";		-- 7I60
	
	constant IDROMOffset : std_logic_vector(31 downto 0) := x"0000"&IDROMAddr&x"00"; -- note need to change if pitch changed
	constant IDROMWEnAddr : std_logic_vector(7 downto 0) := x"08";

	constant IRQDivAddr  : std_logic_vector(7 downto 0) := x"09";
	constant IRQStatusAddr : std_logic_vector(7 downto 0) := x"0A";
	constant ClearIRQAddr : std_logic_vector(7 downto 0) := x"0B"; 
	constant IRQNumRegs : std_logic_vector(7 downto 0) := x"03";
	constant IRQMPBitMask : std_logic_vector(31 downto 0) := x"00000000";
	
	constant WatchdogTimeAddr : std_logic_vector(7 downto 0) := x"0C";
	constant WatchDogStatusAddr : std_logic_vector(7 downto 0) := x"0D";
	constant WatchDogCookieAddr : std_logic_vector(7 downto 0) := x"0E";
	constant WatchDogNumRegs : std_logic_vector(7 downto 0) := x"03";
	constant WatchDogMPBitMask : std_logic_vector(31 downto 0) := x"00000000";

	constant	PortAddr : std_logic_vector(7 downto 0) := x"10";
	constant	DDRAddr : std_logic_vector(7 downto 0) := x"11";	
	constant	AltDataSrcAddr : std_logic_vector(7 downto 0) := x"12";
	constant	OpenDrainModeAddr : std_logic_vector(7 downto 0) := x"13";		
	constant OutputInvAddr : std_logic_vector(7 downto 0) := x"14";	
	constant IOPortNumRegs : std_logic_vector(7 downto 0) := x"05";
	constant IOPortMPBitMask : std_logic_vector(31 downto 0) := x"0000001F";

	constant StepGenRateAddr : std_logic_vector(7 downto 0) := x"20";	
	constant StepGenAccumAddr : std_logic_vector(7 downto 0) := x"21";		
	constant StepGenModeAddr : std_logic_vector(7 downto 0) := x"22";
	constant StepGenDSUTimeAddr : std_logic_vector(7 downto 0) := x"23";
	constant StepGenDHLDTimeAddr : std_logic_vector(7 downto 0) := x"24";
	constant StepGenPulseATimeAddr : std_logic_vector(7 downto 0) := x"25";
	constant StepGenPulseITimeAddr : std_logic_vector(7 downto 0) := x"26";
	constant StepGenTableAddr : std_logic_vector(7 downto 0) := x"27";
	constant StepGenTableMaxAddr : std_logic_vector(7 downto 0) := x"28";
	constant StepGenBasicRateAddr : std_logic_vector(7 downto 0) := x"29";
	constant StepGenNumRegs : std_logic_vector(7 downto 0) := x"0A";
	constant StepGenMPBitMask : std_logic_vector(31 downto 0) := x"000001FF";

	constant QCounterAddr : std_logic_vector(7 downto 0) := x"30";
	constant QCounterCCRAddr : std_logic_vector(7 downto 0) := x"31";
	constant TSDivAddr : std_logic_vector(7 downto 0) := x"32";
	constant TSAddr : std_logic_vector(7 downto 0) := x"33";
	constant QCRateAddr : std_logic_vector(7 downto 0) := x"34";
	constant QCounterNumRegs : std_logic_vector(7 downto 0) := x"05";
	constant QCounterMPBitMask : std_logic_vector(31 downto 0) := x"00000003";

	constant PWMValAddr : std_logic_vector(7 downto 0) := x"40";
	constant PWMCRAddr : std_logic_vector(7 downto 0) := x"41";
	constant PWMRateAddr : std_logic_vector(7 downto 0) := x"42";
	constant PDMRateAddr : std_logic_vector(7 downto 0) := x"43";
	constant PWMEnasAddr : std_logic_vector(7 downto 0) := x"44";
	constant PWMNumRegs : std_logic_vector(7 downto 0) := x"05";
	constant PWMMPBitMask : std_logic_vector(31 downto 0) := x"00000003";

	constant SPIDataAddr : std_logic_vector(7 downto 0) := x"50";
	constant SPIBitCountAddr : std_logic_vector(7 downto 0) := x"51";
	constant SPIBitrateAddr : std_logic_vector(7 downto 0) := x"52";
	constant SPINumRegs : std_logic_vector(7 downto 0) := x"03";
	constant SPIMPBitMask : std_logic_vector(31 downto 0) := x"00000007";
	
	constant UARTXDataAddr : std_logic_vector(7 downto 0) := x"60";	
	constant UARTXFIFOCountAddr : std_logic_vector(7 downto 0) := x"61";
	constant UARTXBitrateAddr: std_logic_vector(7 downto 0) := x"62";
	constant UARTXModeRegAddr : std_logic_vector(7 downto 0) := x"63";	
	constant UARTXNumRegs : std_logic_vector(7 downto 0) := x"04";
	constant UARTXMPBitMask : std_logic_vector(31 downto 0) := x"0000000F";

	constant UARTRDataAddr : std_logic_vector(7 downto 0) := x"70";
	constant UARTRFIFOCountAddr : std_logic_vector(7 downto 0) := x"71";
	constant UARTRBitrateAddr : std_logic_vector(7 downto 0) := x"72";
	constant UARTRModeAddr : std_logic_vector(7 downto 0) := x"73";
	constant UARTRNumRegs : std_logic_vector(7 downto 0) := x"04";
	constant UARTRMPBitMask : std_logic_vector(31 downto 0) := x"0000000F";
	
	constant TranslateRamAddr : std_logic_vector(7 downto 0) := x"78";
	constant TranslateRegionAddr : std_logic_vector(7 downto 0) := x"7C";
	constant TranslateNumRegs : std_logic_vector(7 downto 0) := x"04";
	constant TranslateMPBitMask : std_logic_vector(31 downto 0) := x"00000000";

	

	constant ClockLow20: integer :=  33333333;  	-- 5I20/4I65 low speed clock
	constant ClockLow22: integer :=  48000000;	-- 5I22/5I23 low speed clock
	constant ClockLow43: integer :=  50000000;	-- 7I43 low speed clock
	constant ClockLow68: integer :=  48000000;	-- 4I68 low speed clock
	
	constant ClockHigh20: integer    := 100000000;	-- 5I20/4I65 high speed clock
	constant ClockHigh22: integer    := 96000000;	-- 5I22/5I23 high speed clock
	constant ClockHigh43: integer    := 100000000;	-- 7I43 high speed clock
	constant ClockHigh68: integer    := 96000000;	-- 4I68 high speed clock
	
	constant ClockLowTag: std_logic_vector(7 downto 0) := x"01";
	constant ClockHighTag: std_logic_vector(7 downto 0) := x"02";
	
	constant NullTag : std_logic_vector(7 downto 0) := x"00";
	constant IRQLogicTag : std_logic_vector(7 downto 0) := x"01";
	constant WatchDogTag : std_logic_vector(7 downto 0) := x"02";
	constant IOPortTag : std_logic_vector(7 downto 0) := x"03";
	constant	QCountTag : std_logic_vector(7 downto 0) := x"04";
		constant QCountQAPin : std_logic_vector(7 downto 0) := x"01";
		constant QCountQBPin : std_logic_vector(7 downto 0) := x"02";
		constant QCountIdxPin : std_logic_vector(7 downto 0) := x"03";
		constant QCountIdxMaskPin : std_logic_vector(7 downto 0) := x"04";
	constant	StepGenTag : std_logic_vector(7 downto 0) := x"05";
		constant	StepGenStepPin : std_logic_vector(7 downto 0) := x"81";
		constant	StepGenDirPin : std_logic_vector(7 downto 0) := x"82";
		constant	StepGenTable2Pin : std_logic_vector(7 downto 0) := x"83";
		constant	StepGenTable3Pin : std_logic_vector(7 downto 0) := x"84";
		constant	StepGenTable4Pin : std_logic_vector(7 downto 0) := x"85";
		constant	StepGenTable5Pin : std_logic_vector(7 downto 0) := x"86";
	constant PWMTag : std_logic_vector(7 downto 0) := x"06";
		constant PWMAOutPin : std_logic_vector(7 downto 0) := x"81";
		constant PWMBDirPin : std_logic_vector(7 downto 0) := x"82";
		constant PWMCEnaPin : std_logic_vector(7 downto 0) := x"83";	
	constant SPITag : std_logic_vector(7 downto 0) := x"07";
		constant SPIFramePin : std_logic_vector(7 downto 0) := x"81";
		constant SPIOutPin : std_logic_vector(7 downto 0) := x"82";
		constant SPIClkPin : std_logic_vector(7 downto 0) := x"83";
		constant SPIInPin : std_logic_vector(7 downto 0) := x"04";
	constant SSITag : std_logic_vector(7 downto 0) := x"08";
	constant UARTTXTag : std_logic_vector(7 downto 0) := x"09";
		constant UTXDataPin : std_logic_vector(7 downto 0) := x"81";
		constant UTDrvEnPin : std_logic_vector(7 downto 0) := x"82";		
	constant UARTRXTag : std_logic_vector(7 downto 0) := x"0A";
		constant URXDataPin : std_logic_vector(7 downto 0) := x"01";	
	constant AddrXTag : std_logic_vector(7 downto 0) := x"0B";
	constant LEDTag : std_logic_vector(7 downto 0) := x"80";
	
	
	constant emptypin : std_logic_vector(31 downto 0) := x"00000000";
	constant empty : std_logic_vector(31 downto 0) := x"00000000";
	constant PadT : std_logic_vector(7 downto 0) := x"00";
	constant MaxModules : integer := 32;			-- maximum number of module types 
	constant MaxPins : integer := 128;				-- maximum number of I/O pins
	type PinDescType is array(0 to MaxPins -1) of std_logic_vector(31 downto 0);
	type ModuleRecord is 
	record	
		GTag : std_logic_vector(7 downto 0);
		Version : std_logic_vector(7 downto 0);
		Clock : std_logic_vector(7 downto 0);
		NumInstances : std_logic_vector(7 downto 0);
		BaseAddr : std_logic_vector(15 downto 0);
		NumRegisters : std_logic_vector(7 downto 0);
		Strides : std_logic_vector(7 downto 0);
		MultRegs : std_logic_vector(31 downto 0);
	end record; 
	
	
	


	type ModuleIDType is array(0 to MaxModules-1) of ModuleRecord;



-- These messy constants must remain until I make a script 
-- to generate them based on configuration parameters

	constant ModuleID_3xi30 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"0C",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"0C",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	
	
	constant PinDesc_3xi30 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",
		IOPortTag & x"06" & PWMTag & x"83",
					
		IOPortTag & x"09" & QCountTag & x"02",
		IOPortTag & x"09" & QCountTag & x"01",
		IOPortTag & x"08" & QCountTag & x"02",
		IOPortTag & x"08" & QCountTag & x"01",
		IOPortTag & x"09" & QCountTag & x"03",
		IOPortTag & x"08" & QCountTag & x"03",
		IOPortTag & x"09" & PWMTag & x"81",
		IOPortTag & x"08" & PWMTag & x"81",
		IOPortTag & x"09" & PWMTag & x"82",
		IOPortTag & x"08" & PWMTag & x"82",
		IOPortTag & x"09" & PWMTag & x"83",
		IOPortTag & x"08" & PWMTag & x"83",
		IOPortTag & x"0B" & QCountTag & x"02",
		IOPortTag & x"0B" & QCountTag & x"01",
		IOPortTag & x"0A" & QCountTag & x"02",
		IOPortTag & x"0A" & QCountTag & x"01",
		IOPortTag & x"0B" & QCountTag & x"03",
		IOPortTag & x"0A" & QCountTag & x"03",
		IOPortTag & x"0B" & PWMTag & x"81",
		IOPortTag & x"0A" & PWMTag & x"81",
		IOPortTag & x"0B" & PWMTag & x"82",
		IOPortTag & x"0A" & PWMTag & x"82",
		IOPortTag & x"0B" & PWMTag & x"83",
		IOPortTag & x"0A" & PWMTag & x"83",
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);


	constant ModuleID_2xi30 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"02",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"08",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(AddrXTag,		x"00",	ClockLowTag,	x"01",	TranslateRAMAddr&PadT,		TranslateNumRegs,		x"00",	TranslateMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	
	
	constant PinDesc_2xi30 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",
		IOPortTag & x"06" & PWMTag & x"83",
					
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_SVST8_4 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"08",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"04",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	
	
	constant PinDesc_SVST8_4 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",
		IOPortTag & x"06" & PWMTag & x"83",
					
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"00" & StepGenTag & x"83",
		IOPortTag & x"00" & StepGenTag & x"84",
		IOPortTag & x"00" & StepGenTag & x"85",
		IOPortTag & x"00" & StepGenTag & x"86",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"83",
		IOPortTag & x"01" & StepGenTag & x"84",
		IOPortTag & x"01" & StepGenTag & x"85",
		IOPortTag & x"01" & StepGenTag & x"86",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"83",
		IOPortTag & x"02" & StepGenTag & x"84",
		IOPortTag & x"02" & StepGenTag & x"85",
		IOPortTag & x"02" & StepGenTag & x"86",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"83",
		IOPortTag & x"03" & StepGenTag & x"84",
		IOPortTag & x"03" & StepGenTag & x"85",
		IOPortTag & x"03" & StepGenTag & x"86",
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_SVST4_4 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"02",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"04",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"04",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"04",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(AddrXTag,		x"00",	ClockLowTag,	x"01",	TranslateRAMAddr&PadT,		TranslateNumRegs,		x"00",	TranslateMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);

		
	
	constant PinDesc_SVST4_4 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
										
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"00" & StepGenTag & x"83",
		IOPortTag & x"00" & StepGenTag & x"84",
		IOPortTag & x"00" & StepGenTag & x"85",
		IOPortTag & x"00" & StepGenTag & x"86",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"83",
		IOPortTag & x"01" & StepGenTag & x"84",
		IOPortTag & x"01" & StepGenTag & x"85",
		IOPortTag & x"01" & StepGenTag & x"86",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"83",
		IOPortTag & x"02" & StepGenTag & x"84",
		IOPortTag & x"02" & StepGenTag & x"85",
		IOPortTag & x"02" & StepGenTag & x"86",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"83",
		IOPortTag & x"03" & StepGenTag & x"84",
		IOPortTag & x"03" & StepGenTag & x"85",
		IOPortTag & x"03" & StepGenTag & x"86",
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_SVST4_6 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"02",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"04",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"04",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"06",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(AddrXTag,		x"00",	ClockLowTag,	x"01",	TranslateRAMAddr&PadT,		TranslateNumRegs,		x"00",	TranslateMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);

		
	
	constant PinDesc_SVST4_6 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
										
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"00" & StepGenTag & x"83",
		IOPortTag & x"00" & StepGenTag & x"84",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"83",
		IOPortTag & x"01" & StepGenTag & x"84",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"83",
		IOPortTag & x"02" & StepGenTag & x"84",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"83",
		IOPortTag & x"03" & StepGenTag & x"84",
		IOPortTag & x"04" & StepGenTag & x"81",
		IOPortTag & x"04" & StepGenTag & x"82",
		IOPortTag & x"04" & StepGenTag & x"83",
		IOPortTag & x"04" & StepGenTag & x"84",
		IOPortTag & x"05" & StepGenTag & x"81",
		IOPortTag & x"05" & StepGenTag & x"82",
		IOPortTag & x"05" & StepGenTag & x"83",
		IOPortTag & x"05" & StepGenTag & x"84",
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);





	constant ModuleID_4xi30 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"10",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"10",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		

	constant PinDesc_4xi30 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",
		IOPortTag & x"06" & PWMTag & x"83",
					
		IOPortTag & x"09" & QCountTag & x"02",
		IOPortTag & x"09" & QCountTag & x"01",
		IOPortTag & x"08" & QCountTag & x"02",
		IOPortTag & x"08" & QCountTag & x"01",
		IOPortTag & x"09" & QCountTag & x"03",
		IOPortTag & x"08" & QCountTag & x"03",
		IOPortTag & x"09" & PWMTag & x"81",
		IOPortTag & x"08" & PWMTag & x"81",
		IOPortTag & x"09" & PWMTag & x"82",
		IOPortTag & x"08" & PWMTag & x"82",
		IOPortTag & x"09" & PWMTag & x"83",
		IOPortTag & x"08" & PWMTag & x"83",
		IOPortTag & x"0B" & QCountTag & x"02",
		IOPortTag & x"0B" & QCountTag & x"01",
		IOPortTag & x"0A" & QCountTag & x"02",
		IOPortTag & x"0A" & QCountTag & x"01",
		IOPortTag & x"0B" & QCountTag & x"03",
		IOPortTag & x"0A" & QCountTag & x"03",
		IOPortTag & x"0B" & PWMTag & x"81",
		IOPortTag & x"0A" & PWMTag & x"81",
		IOPortTag & x"0B" & PWMTag & x"82",
		IOPortTag & x"0A" & PWMTag & x"82",
		IOPortTag & x"0B" & PWMTag & x"83",
		IOPortTag & x"0A" & PWMTag & x"83",

		IOPortTag & x"0D" & QCountTag & x"02",
		IOPortTag & x"0D" & QCountTag & x"01",
		IOPortTag & x"0C" & QCountTag & x"02",
		IOPortTag & x"0C" & QCountTag & x"01",
		IOPortTag & x"0D" & QCountTag & x"03",
		IOPortTag & x"0C" & QCountTag & x"03",
		IOPortTag & x"0D" & PWMTag & x"81",
		IOPortTag & x"0C" & PWMTag & x"81",
		IOPortTag & x"0D" & PWMTag & x"82",
		IOPortTag & x"0C" & PWMTag & x"82",
		IOPortTag & x"0D" & PWMTag & x"83",
		IOPortTag & x"0C" & PWMTag & x"83",
		IOPortTag & x"0F" & QCountTag & x"02",
		IOPortTag & x"0F" & QCountTag & x"01",
		IOPortTag & x"0E" & QCountTag & x"02",
		IOPortTag & x"0E" & QCountTag & x"01",
		IOPortTag & x"0F" & QCountTag & x"03",
		IOPortTag & x"0E" & QCountTag & x"03",
		IOPortTag & x"0F" & PWMTag & x"81",
		IOPortTag & x"0E" & PWMTag & x"81",
		IOPortTag & x"0F" & PWMTag & x"82",
		IOPortTag & x"0E" & PWMTag & x"82",
		IOPortTag & x"0F" & PWMTag & x"83",
		IOPortTag & x"0E" & PWMTag & x"83",
				
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);
				
	constant ModuleID_24xQCtrOnly : ModuleIDType :=( 
		
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"10",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	constant PinDesc_24xQCtrOnly : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"03",

		IOPortTag & x"08" & QCountTag & x"01",
		IOPortTag & x"08" & QCountTag & x"02",
		IOPortTag & x"08" & QCountTag & x"03",
		IOPortTag & x"09" & QCountTag & x"01",
		IOPortTag & x"09" & QCountTag & x"02",
		IOPortTag & x"09" & QCountTag & x"03",
		IOPortTag & x"0A" & QCountTag & x"01",
		IOPortTag & x"0A" & QCountTag & x"02",
		IOPortTag & x"0A" & QCountTag & x"03",
		IOPortTag & x"0B" & QCountTag & x"01",
		IOPortTag & x"0B" & QCountTag & x"02",
		IOPortTag & x"0B" & QCountTag & x"03",
		IOPortTag & x"0C" & QCountTag & x"01",
		IOPortTag & x"0C" & QCountTag & x"02",
		IOPortTag & x"0C" & QCountTag & x"03",
		IOPortTag & x"0D" & QCountTag & x"01",
		IOPortTag & x"0D" & QCountTag & x"02",
		IOPortTag & x"0D" & QCountTag & x"03",
		IOPortTag & x"0E" & QCountTag & x"01",
		IOPortTag & x"0E" & QCountTag & x"02",
		IOPortTag & x"0E" & QCountTag & x"03",
		IOPortTag & x"0F" & QCountTag & x"01",
		IOPortTag & x"0F" & QCountTag & x"02",
		IOPortTag & x"0F" & QCountTag & x"03",

		IOPortTag & x"10" & QCountTag & x"01",
		IOPortTag & x"10" & QCountTag & x"02",
		IOPortTag & x"10" & QCountTag & x"03",
		IOPortTag & x"11" & QCountTag & x"01",
		IOPortTag & x"11" & QCountTag & x"02",
		IOPortTag & x"11" & QCountTag & x"03",
		IOPortTag & x"12" & QCountTag & x"01",
		IOPortTag & x"12" & QCountTag & x"02",
		IOPortTag & x"12" & QCountTag & x"03",
		IOPortTag & x"13" & QCountTag & x"01",
		IOPortTag & x"13" & QCountTag & x"02",
		IOPortTag & x"13" & QCountTag & x"03",
		IOPortTag & x"14" & QCountTag & x"01",
		IOPortTag & x"14" & QCountTag & x"02",
		IOPortTag & x"14" & QCountTag & x"03",
		IOPortTag & x"15" & QCountTag & x"01",
		IOPortTag & x"15" & QCountTag & x"02",
		IOPortTag & x"15" & QCountTag & x"03",
		IOPortTag & x"16" & QCountTag & x"01",
		IOPortTag & x"16" & QCountTag & x"02",
		IOPortTag & x"16" & QCountTag & x"03",
		IOPortTag & x"17" & QCountTag & x"01",
		IOPortTag & x"17" & QCountTag & x"02",
		IOPortTag & x"17" & QCountTag & x"03",
		
					
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,			
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);
								
	constant ModuleID_SVST4_8 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"04",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"04",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"08",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	
	
	constant PinDesc_SVST4_8 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
					
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"00" & StepGenTag & x"83",
		IOPortTag & x"00" & StepGenTag & x"84",
		IOPortTag & x"00" & StepGenTag & x"85",
		IOPortTag & x"00" & StepGenTag & x"86",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"83",
		IOPortTag & x"01" & StepGenTag & x"84",
		IOPortTag & x"01" & StepGenTag & x"85",
		IOPortTag & x"01" & StepGenTag & x"86",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"83",
		IOPortTag & x"02" & StepGenTag & x"84",
		IOPortTag & x"02" & StepGenTag & x"85",
		IOPortTag & x"02" & StepGenTag & x"86",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"83",
		IOPortTag & x"03" & StepGenTag & x"84",
		IOPortTag & x"03" & StepGenTag & x"85",
		IOPortTag & x"03" & StepGenTag & x"86",
		
		IOPortTag & x"04" & StepGenTag & x"81",
		IOPortTag & x"04" & StepGenTag & x"82",
		IOPortTag & x"04" & StepGenTag & x"83",
		IOPortTag & x"04" & StepGenTag & x"84",
		IOPortTag & x"04" & StepGenTag & x"85",
		IOPortTag & x"04" & StepGenTag & x"86",
		IOPortTag & x"05" & StepGenTag & x"81",
		IOPortTag & x"05" & StepGenTag & x"82",
		IOPortTag & x"05" & StepGenTag & x"83",
		IOPortTag & x"05" & StepGenTag & x"84",
		IOPortTag & x"05" & StepGenTag & x"85",
		IOPortTag & x"05" & StepGenTag & x"86",
		IOPortTag & x"06" & StepGenTag & x"81",
		IOPortTag & x"06" & StepGenTag & x"82",
		IOPortTag & x"06" & StepGenTag & x"83",
		IOPortTag & x"06" & StepGenTag & x"84",
		IOPortTag & x"06" & StepGenTag & x"85",
		IOPortTag & x"06" & StepGenTag & x"86",
		IOPortTag & x"07" & StepGenTag & x"81",
		IOPortTag & x"07" & StepGenTag & x"82",
		IOPortTag & x"07" & StepGenTag & x"83",
		IOPortTag & x"07" & StepGenTag & x"84",
		IOPortTag & x"07" & StepGenTag & x"85",
		IOPortTag & x"07" & StepGenTag & x"86",
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					

	constant ModuleID_SVST2_8 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"02",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"02",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"08",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
			
	constant PinDesc_SVST2_8 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
		IOPortTag & x"00" & NullTag & x"00",
					
					
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"00" & StepGenTag & x"83",
		IOPortTag & x"00" & StepGenTag & x"84",
		IOPortTag & x"00" & StepGenTag & x"85",
		IOPortTag & x"00" & StepGenTag & x"86",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"83",
		IOPortTag & x"01" & StepGenTag & x"84",
		IOPortTag & x"01" & StepGenTag & x"85",
		IOPortTag & x"01" & StepGenTag & x"86",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"83",
		IOPortTag & x"02" & StepGenTag & x"84",
		IOPortTag & x"02" & StepGenTag & x"85",
		IOPortTag & x"02" & StepGenTag & x"86",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"83",
		IOPortTag & x"03" & StepGenTag & x"84",
		IOPortTag & x"03" & StepGenTag & x"85",
		IOPortTag & x"03" & StepGenTag & x"86",
		
		IOPortTag & x"04" & StepGenTag & x"81",
		IOPortTag & x"04" & StepGenTag & x"82",
		IOPortTag & x"04" & StepGenTag & x"83",
		IOPortTag & x"04" & StepGenTag & x"84",
		IOPortTag & x"04" & StepGenTag & x"85",
		IOPortTag & x"04" & StepGenTag & x"86",
		IOPortTag & x"05" & StepGenTag & x"81",
		IOPortTag & x"05" & StepGenTag & x"82",
		IOPortTag & x"05" & StepGenTag & x"83",
		IOPortTag & x"05" & StepGenTag & x"84",
		IOPortTag & x"05" & StepGenTag & x"85",
		IOPortTag & x"05" & StepGenTag & x"86",
		IOPortTag & x"06" & StepGenTag & x"81",
		IOPortTag & x"06" & StepGenTag & x"82",
		IOPortTag & x"06" & StepGenTag & x"83",
		IOPortTag & x"06" & StepGenTag & x"84",
		IOPortTag & x"06" & StepGenTag & x"85",
		IOPortTag & x"06" & StepGenTag & x"86",
		IOPortTag & x"07" & StepGenTag & x"81",
		IOPortTag & x"07" & StepGenTag & x"82",
		IOPortTag & x"07" & StepGenTag & x"83",
		IOPortTag & x"07" & StepGenTag & x"84",
		IOPortTag & x"07" & StepGenTag & x"85",
		IOPortTag & x"07" & StepGenTag & x"86",
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					



	constant ModuleID_ST12 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"0C",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
		
	constant PinDesc_ST12 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
					
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"00" & StepGenTag & x"83",
		IOPortTag & x"00" & StepGenTag & x"84",
		IOPortTag & x"00" & StepGenTag & x"85",
		IOPortTag & x"00" & StepGenTag & x"86",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"83",
		IOPortTag & x"01" & StepGenTag & x"84",
		IOPortTag & x"01" & StepGenTag & x"85",
		IOPortTag & x"01" & StepGenTag & x"86",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"83",
		IOPortTag & x"02" & StepGenTag & x"84",
		IOPortTag & x"02" & StepGenTag & x"85",
		IOPortTag & x"02" & StepGenTag & x"86",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"83",
		IOPortTag & x"03" & StepGenTag & x"84",
		IOPortTag & x"03" & StepGenTag & x"85",
		IOPortTag & x"03" & StepGenTag & x"86",
		
		IOPortTag & x"04" & StepGenTag & x"81",
		IOPortTag & x"04" & StepGenTag & x"82",
		IOPortTag & x"04" & StepGenTag & x"83",
		IOPortTag & x"04" & StepGenTag & x"84",
		IOPortTag & x"04" & StepGenTag & x"85",
		IOPortTag & x"04" & StepGenTag & x"86",
		IOPortTag & x"05" & StepGenTag & x"81",
		IOPortTag & x"05" & StepGenTag & x"82",
		IOPortTag & x"05" & StepGenTag & x"83",
		IOPortTag & x"05" & StepGenTag & x"84",
		IOPortTag & x"05" & StepGenTag & x"85",
		IOPortTag & x"05" & StepGenTag & x"86",
		IOPortTag & x"06" & StepGenTag & x"81",
		IOPortTag & x"06" & StepGenTag & x"82",
		IOPortTag & x"06" & StepGenTag & x"83",
		IOPortTag & x"06" & StepGenTag & x"84",
		IOPortTag & x"06" & StepGenTag & x"85",
		IOPortTag & x"06" & StepGenTag & x"86",
		IOPortTag & x"07" & StepGenTag & x"81",
		IOPortTag & x"07" & StepGenTag & x"82",
		IOPortTag & x"07" & StepGenTag & x"83",
		IOPortTag & x"07" & StepGenTag & x"84",
		IOPortTag & x"07" & StepGenTag & x"85",
		IOPortTag & x"07" & StepGenTag & x"86",

		IOPortTag & x"08" & StepGenTag & x"81",
		IOPortTag & x"08" & StepGenTag & x"82",
		IOPortTag & x"08" & StepGenTag & x"83",
		IOPortTag & x"08" & StepGenTag & x"84",
		IOPortTag & x"08" & StepGenTag & x"85",
		IOPortTag & x"08" & StepGenTag & x"86",
		IOPortTag & x"09" & StepGenTag & x"81",
		IOPortTag & x"09" & StepGenTag & x"82",
		IOPortTag & x"09" & StepGenTag & x"83",
		IOPortTag & x"09" & StepGenTag & x"84",
		IOPortTag & x"09" & StepGenTag & x"85",
		IOPortTag & x"09" & StepGenTag & x"86",
		IOPortTag & x"0A" & StepGenTag & x"81",
		IOPortTag & x"0A" & StepGenTag & x"82",
		IOPortTag & x"0A" & StepGenTag & x"83",
		IOPortTag & x"0A" & StepGenTag & x"84",
		IOPortTag & x"0A" & StepGenTag & x"85",
		IOPortTag & x"0A" & StepGenTag & x"86",
		IOPortTag & x"0B" & StepGenTag & x"81",
		IOPortTag & x"0B" & StepGenTag & x"82",
		IOPortTag & x"0B" & StepGenTag & x"83",
		IOPortTag & x"0B" & StepGenTag & x"84",
		IOPortTag & x"0B" & StepGenTag & x"85",
		IOPortTag & x"0B" & StepGenTag & x"86",
		
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					



	constant ModuleID_SVST8_8 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"04",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"08",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"08",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
			
	constant PinDesc_SVST8_8 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",
		IOPortTag & x"06" & PWMTag & x"83",
		
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"00" & StepGenTag & x"83",
		IOPortTag & x"00" & StepGenTag & x"84",
		IOPortTag & x"00" & StepGenTag & x"85",
		IOPortTag & x"00" & StepGenTag & x"86",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"83",
		IOPortTag & x"01" & StepGenTag & x"84",
		IOPortTag & x"01" & StepGenTag & x"85",
		IOPortTag & x"01" & StepGenTag & x"86",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"83",
		IOPortTag & x"02" & StepGenTag & x"84",
		IOPortTag & x"02" & StepGenTag & x"85",
		IOPortTag & x"02" & StepGenTag & x"86",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"83",
		IOPortTag & x"03" & StepGenTag & x"84",
		IOPortTag & x"03" & StepGenTag & x"85",
		IOPortTag & x"03" & StepGenTag & x"86",
		
		IOPortTag & x"04" & StepGenTag & x"81",
		IOPortTag & x"04" & StepGenTag & x"82",
		IOPortTag & x"04" & StepGenTag & x"83",
		IOPortTag & x"04" & StepGenTag & x"84",
		IOPortTag & x"04" & StepGenTag & x"85",
		IOPortTag & x"04" & StepGenTag & x"86",
		IOPortTag & x"05" & StepGenTag & x"81",
		IOPortTag & x"05" & StepGenTag & x"82",
		IOPortTag & x"05" & StepGenTag & x"83",
		IOPortTag & x"05" & StepGenTag & x"84",
		IOPortTag & x"05" & StepGenTag & x"85",
		IOPortTag & x"05" & StepGenTag & x"86",
		IOPortTag & x"06" & StepGenTag & x"81",
		IOPortTag & x"06" & StepGenTag & x"82",
		IOPortTag & x"06" & StepGenTag & x"83",
		IOPortTag & x"06" & StepGenTag & x"84",
		IOPortTag & x"06" & StepGenTag & x"85",
		IOPortTag & x"06" & StepGenTag & x"86",
		IOPortTag & x"07" & StepGenTag & x"81",
		IOPortTag & x"07" & StepGenTag & x"82",
		IOPortTag & x"07" & StepGenTag & x"83",
		IOPortTag & x"07" & StepGenTag & x"84",
		IOPortTag & x"07" & StepGenTag & x"85",
		IOPortTag & x"07" & StepGenTag & x"86",
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					

	constant ModuleID_SVST8_24 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"04",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"08",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"18",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
	constant PinDesc_SVST8_24 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",
		IOPortTag & x"06" & PWMTag & x"83",
		
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"04" & StepGenTag & x"81",
		IOPortTag & x"04" & StepGenTag & x"82",
		IOPortTag & x"05" & StepGenTag & x"81",
		IOPortTag & x"05" & StepGenTag & x"82",
		IOPortTag & x"06" & StepGenTag & x"81",
		IOPortTag & x"06" & StepGenTag & x"82",
		IOPortTag & x"07" & StepGenTag & x"81",
		IOPortTag & x"07" & StepGenTag & x"82",
		IOPortTag & x"08" & StepGenTag & x"81",
		IOPortTag & x"08" & StepGenTag & x"82",
		IOPortTag & x"09" & StepGenTag & x"81",
		IOPortTag & x"09" & StepGenTag & x"82",
		IOPortTag & x"0A" & StepGenTag & x"81",
		IOPortTag & x"0A" & StepGenTag & x"82",
		IOPortTag & x"0B" & StepGenTag & x"81",
		IOPortTag & x"0B" & StepGenTag & x"82",
		
		IOPortTag & x"0C" & StepGenTag & x"81",
		IOPortTag & x"0C" & StepGenTag & x"82",
		IOPortTag & x"0D" & StepGenTag & x"81",
		IOPortTag & x"0D" & StepGenTag & x"82",
		IOPortTag & x"0E" & StepGenTag & x"81",
		IOPortTag & x"0E" & StepGenTag & x"82",
		IOPortTag & x"0F" & StepGenTag & x"81",
		IOPortTag & x"0F" & StepGenTag & x"82",
		IOPortTag & x"10" & StepGenTag & x"81",
		IOPortTag & x"10" & StepGenTag & x"82",
		IOPortTag & x"11" & StepGenTag & x"81",
		IOPortTag & x"11" & StepGenTag & x"82",
		IOPortTag & x"12" & StepGenTag & x"81",
		IOPortTag & x"12" & StepGenTag & x"82",
		IOPortTag & x"13" & StepGenTag & x"81",
		IOPortTag & x"13" & StepGenTag & x"82",
		IOPortTag & x"14" & StepGenTag & x"81",
		IOPortTag & x"14" & StepGenTag & x"82",
		IOPortTag & x"15" & StepGenTag & x"81",
		IOPortTag & x"15" & StepGenTag & x"82",
		IOPortTag & x"16" & StepGenTag & x"81",
		IOPortTag & x"16" & StepGenTag & x"82",
		IOPortTag & x"17" & StepGenTag & x"81",
		IOPortTag & x"17" & StepGenTag & x"82",
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					

	constant ModuleID_SVSTSP8_12_6 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"04",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"08",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"0C",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(SPITag,			x"00",	ClockLowTag,	x"06",	SPIDataAddr&PadT,				SPINumRegs,				x"00",	SPIMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
	constant PinDesc_SVSTSP8_12_6 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"02" & PWMTag & x"83",
					
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",
		IOPortTag & x"06" & PWMTag & x"83",
		
		IOPortTag & x"00" & StepGenTag & x"81",
		IOPortTag & x"00" & StepGenTag & x"82",
		IOPortTag & x"01" & StepGenTag & x"81",
		IOPortTag & x"01" & StepGenTag & x"82",
		IOPortTag & x"02" & StepGenTag & x"81",
		IOPortTag & x"02" & StepGenTag & x"82",
		IOPortTag & x"03" & StepGenTag & x"81",
		IOPortTag & x"03" & StepGenTag & x"82",
		IOPortTag & x"04" & StepGenTag & x"81",
		IOPortTag & x"04" & StepGenTag & x"82",
		IOPortTag & x"05" & StepGenTag & x"81",
		IOPortTag & x"05" & StepGenTag & x"82",
		IOPortTag & x"06" & StepGenTag & x"81",
		IOPortTag & x"06" & StepGenTag & x"82",
		IOPortTag & x"07" & StepGenTag & x"81",
		IOPortTag & x"07" & StepGenTag & x"82",
		IOPortTag & x"08" & StepGenTag & x"81",
		IOPortTag & x"08" & StepGenTag & x"82",
		IOPortTag & x"09" & StepGenTag & x"81",
		IOPortTag & x"09" & StepGenTag & x"82",
		IOPortTag & x"0A" & StepGenTag & x"81",
		IOPortTag & x"0A" & StepGenTag & x"82",
		IOPortTag & x"0B" & StepGenTag & x"81",
		IOPortTag & x"0B" & StepGenTag & x"82",
		
		IOPortTag & x"00" & SPITag & x"81",
		IOPortTag & x"00" & SPITag & x"82",
		IOPortTag & x"00" & SPITag & x"83",
		IOPortTag & x"00" & SPITag & x"04",
		IOPortTag & x"01" & SPITag & x"81",
		IOPortTag & x"01" & SPITag & x"82",
		IOPortTag & x"01" & SPITag & x"83",
		IOPortTag & x"01" & SPITag & x"04",
		IOPortTag & x"02" & SPITag & x"81",
		IOPortTag & x"02" & SPITag & x"82",
		IOPortTag & x"02" & SPITag & x"83",
		IOPortTag & x"02" & SPITag & x"04",
		IOPortTag & x"03" & SPITag & x"81",
		IOPortTag & x"03" & SPITag & x"82",
		IOPortTag & x"03" & SPITag & x"83",
		IOPortTag & x"03" & SPITag & x"04",
		IOPortTag & x"04" & SPITag & x"81",
		IOPortTag & x"04" & SPITag & x"82",
		IOPortTag & x"04" & SPITag & x"83",
		IOPortTag & x"04" & SPITag & x"04",
		IOPortTag & x"05" & SPITag & x"81",
		IOPortTag & x"05" & SPITag & x"82",
		IOPortTag & x"05" & SPITag & x"83",
		IOPortTag & x"05" & SPITag & x"04",
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					



	constant ModuleID_DelftSV12 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"0C",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"0C",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	
	
	constant PinDesc_DelftSV12 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"00" & QCountTag & x"01",
		IOPortTag & x"00" & QCountTag & x"02",
		IOPortTag & x"00" & QCountTag & x"03",
		IOPortTag & x"01" & QCountTag & x"01",
		IOPortTag & x"01" & QCountTag & x"02",
		IOPortTag & x"01" & QCountTag & x"03",
		IOPortTag & x"02" & QCountTag & x"01",
		IOPortTag & x"02" & QCountTag & x"02",
		IOPortTag & x"02" & QCountTag & x"03",
		IOPortTag & x"03" & QCountTag & x"01",
		IOPortTag & x"03" & QCountTag & x"02",
		IOPortTag & x"03" & QCountTag & x"03",
		IOPortTag & x"04" & QCountTag & x"01",
		IOPortTag & x"04" & QCountTag & x"02",
		IOPortTag & x"04" & QCountTag & x"03",
		IOPortTag & x"05" & QCountTag & x"01",
		IOPortTag & x"05" & QCountTag & x"02",
		IOPortTag & x"05" & QCountTag & x"03",
		IOPortTag & x"00" & PWMTag & x"81",
		IOPortTag & x"00" & PWMTag & x"82",
		IOPortTag & x"00" & PWMTag & x"83",
		IOPortTag & x"01" & PWMTag & x"81",
		IOPortTag & x"01" & PWMTag & x"82",
		IOPortTag & x"01" & PWMTag & x"83",
					
		IOPortTag & x"06" & QCountTag & x"01",
		IOPortTag & x"06" & QCountTag & x"02",
		IOPortTag & x"06" & QCountTag & x"03",
		IOPortTag & x"07" & QCountTag & x"01",
		IOPortTag & x"07" & QCountTag & x"02",
		IOPortTag & x"07" & QCountTag & x"03",
		IOPortTag & x"08" & QCountTag & x"01",
		IOPortTag & x"08" & QCountTag & x"02",
		IOPortTag & x"08" & QCountTag & x"03",
		IOPortTag & x"09" & QCountTag & x"01",
		IOPortTag & x"09" & QCountTag & x"02",
		IOPortTag & x"09" & QCountTag & x"03",
		IOPortTag & x"0A" & QCountTag & x"01",
		IOPortTag & x"0A" & QCountTag & x"02",
		IOPortTag & x"0A" & QCountTag & x"03",
		IOPortTag & x"0B" & QCountTag & x"01",
		IOPortTag & x"0B" & QCountTag & x"02",
		IOPortTag & x"0B" & QCountTag & x"03",
		IOPortTag & x"06" & PWMTag & x"81",
		IOPortTag & x"06" & PWMTag & x"82",
		IOPortTag & x"06" & PWMTag & x"83",
		IOPortTag & x"07" & PWMTag & x"81",
		IOPortTag & x"07" & PWMTag & x"82",
		IOPortTag & x"07" & PWMTag & x"83",

		IOPortTag & x"02" & PWMTag & x"81",
		IOPortTag & x"02" & PWMTag & x"82",
		IOPortTag & x"02" & PWMTag & x"83",
		IOPortTag & x"03" & PWMTag & x"81",
		IOPortTag & x"03" & PWMTag & x"82",
		IOPortTag & x"03" & PWMTag & x"83",
		IOPortTag & x"04" & PWMTag & x"81",
		IOPortTag & x"04" & PWMTag & x"82",
		IOPortTag & x"04" & PWMTag & x"83",
		IOPortTag & x"05" & PWMTag & x"81",
		IOPortTag & x"05" & PWMTag & x"82",
		IOPortTag & x"05" & PWMTag & x"83",		
		IOPortTag & x"08" & PWMTag & x"81",
		IOPortTag & x"08" & PWMTag & x"82",
		IOPortTag & x"08" & PWMTag & x"83",
		IOPortTag & x"09" & PWMTag & x"81",
		IOPortTag & x"09" & PWMTag & x"82",
		IOPortTag & x"09" & PWMTag & x"83",		
		IOPortTag & x"0A" & PWMTag & x"81",
		IOPortTag & x"0A" & PWMTag & x"82",
		IOPortTag & x"0A" & PWMTag & x"83",
		IOPortTag & x"0B" & PWMTag & x"81",
		IOPortTag & x"0B" & PWMTag & x"82",
		IOPortTag & x"0B" & PWMTag & x"83",
					

		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_MShaver : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"03",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"01",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"04",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
		(LEDTag,			x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,				x"00",	LEDMPBitMask),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
			
	constant PinDesc_MShaver : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"00" & QCountTag & QCountQAPin,			-- I/O 00
		IOPortTag & x"00" & QCountTag & QCountQBPin,			-- I/O 01	
		IOPortTag & x"00" & QCountTag & QCountIDXPin,		-- I/O 02		
		IOPortTag & x"01" & QCountTag & QCountQAPin,			-- I/O 03		
		IOPortTag & x"00" & StepGenTag & StepGenStepPin,	-- I/O 04		
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 05		
		IOPortTag & x"00" & StepGenTag & StepGenDirPin,		-- I/O 06			
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 07		
		IOPortTag & x"01" & StepGenTag & StepGenStepPin,	-- I/O 08		
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 09
		IOPortTag & x"01" & StepGenTag & StepGenDirPin,		-- I/O 10
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 11
		IOPortTag & x"01" & QCountTag & QCountQBPin,			-- I/O 12		
		IOPortTag & x"02" & QCountTag & QCountQAPin,			-- I/O 13	
		IOPortTag & x"02" & QCountTag & QCountQBPin,			-- I/O 14	
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 15
		IOPortTag & x"02" & StepGenTag & StepGenStepPin,	-- I/O 16			
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 17
		IOPortTag & x"02" & StepGenTag & StepGenDirPin,		-- I/O 18			
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 19
		IOPortTag & x"03" & StepGenTag & StepGenStepPin,	-- I/O 20			
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 21
		IOPortTag & x"03" & StepGenTag & StepGenDirPin,		-- I/O 22			
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 23
					
					
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 24
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 25
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 26
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 27
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 28
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 29
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 30
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 31
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 32
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 33
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 34
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 35
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 36
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 37
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 38
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 39
		IOPortTag & x"00" & PWMTag  & PWMAOutPin,				-- I/O 40
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 41
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 42
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 43
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 44
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 45
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 46
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 47

		IOPortTag & x"00" & NullTag & x"00",					-- I/O 48
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 49
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 50
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 51
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 52
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 53
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 54
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 55
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 56
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 57
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 58
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 59
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 60
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 61
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 62
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 63
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 64
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 65
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 66
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 67
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 68
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 69
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 70
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 71

		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					



end package IDROMParms;
	