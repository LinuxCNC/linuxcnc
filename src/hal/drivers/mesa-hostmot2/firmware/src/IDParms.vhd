
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

	constant MuxedQCounterAddr : std_logic_vector(7 downto 0) := x"35";
	constant MuxedQCounterCCRAddr : std_logic_vector(7 downto 0) := x"36";
	constant MuxedTSDivAddr : std_logic_vector(7 downto 0) := x"37";
	constant MuxedTSAddr : std_logic_vector(7 downto 0) := x"38";
	constant MuxedQCRateAddr : std_logic_vector(7 downto 0) := x"39";
	constant MuxedQCounterNumRegs : std_logic_vector(7 downto 0) := x"05";
	constant MuxedQCounterMPBitMask : std_logic_vector(31 downto 0) := x"00000003";

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

	constant BSPIDataAddr : std_logic_vector(7 downto 0) := x"54";
	constant BSPIDescriptorAddr : std_logic_vector(7 downto 0) := x"55";
	constant BSPIFIFOCountAddr : std_logic_vector(7 downto 0) := x"56";
	constant BSPINumRegs : std_logic_vector(7 downto 0) := x"03";
	constant BSPIMPBitMask : std_logic_vector(31 downto 0) := x"00000007";
	
	constant UARTTDataAddr : std_logic_vector(7 downto 0) := x"60";	
	constant UARTTFIFOCountAddr : std_logic_vector(7 downto 0) := x"61";
	constant UARTTBitrateAddr: std_logic_vector(7 downto 0) := x"62";
	constant UARTTModeRegAddr : std_logic_vector(7 downto 0) := x"63";	
	constant UARTTNumRegs : std_logic_vector(7 downto 0) := x"04";
	constant UARTTMPBitMask : std_logic_vector(31 downto 0) := x"0000000F";

	constant UARTRDataAddr : std_logic_vector(7 downto 0) := x"70";
	constant UARTRFIFOCountAddr : std_logic_vector(7 downto 0) := x"71";
	constant UARTRBitrateAddr : std_logic_vector(7 downto 0) := x"72";
	constant UARTRModeRegAddr : std_logic_vector(7 downto 0) := x"73";
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
		constant NullPin : std_logic_vector(7 downto 0) := x"00";
		
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
		constant	StepGenTable6Pin : std_logic_vector(7 downto 0) := x"87";
		constant	StepGenTable7Pin : std_logic_vector(7 downto 0) := x"88";

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

	constant UARTTTag : std_logic_vector(7 downto 0) := x"09";
		constant UTDataPin : std_logic_vector(7 downto 0) := x"81";
		constant UTDrvEnPin : std_logic_vector(7 downto 0) := x"82";		

	constant UARTRTag : std_logic_vector(7 downto 0) := x"0A";
		constant URDataPin : std_logic_vector(7 downto 0) := x"01";	

	constant AddrXTag : std_logic_vector(7 downto 0) := x"0B";

	constant MuxedQCountTag: std_logic_vector(7 downto 0) := x"0C";
		constant MuxedQCountQAPin : std_logic_vector(7 downto 0) := x"01";
		constant MuxedQCountQBPin : std_logic_vector(7 downto 0) := x"02";
		constant MuxedQCountIdxPin : std_logic_vector(7 downto 0) := x"03";
		constant MuxedQCountIdxMaskPin : std_logic_vector(7 downto 0) := x"04";

	constant MuxedQCountSelTag: std_logic_vector(7 downto 0) := x"0D";
		constant MuxedQCountSel0Pin : std_logic_vector(7 downto 0) := x"81";
		constant MuxedQCountSel1Pin : std_logic_vector(7 downto 0) := x"82";

	constant BSPITag : std_logic_vector(7 downto 0) := x"0E";
		constant BSPIFramePin : std_logic_vector(7 downto 0) := x"81";
		constant BSPIOutPin : std_logic_vector(7 downto 0) := x"82";
		constant BSPIClkPin : std_logic_vector(7 downto 0) := x"83";
		constant BSPIInPin : std_logic_vector(7 downto 0) := x"04";
		constant BSPICS0Pin : std_logic_vector(7 downto 0) := x"85";
		constant BSPICS1Pin : std_logic_vector(7 downto 0) := x"86";
		constant BSPICS2Pin : std_logic_vector(7 downto 0) := x"87";
		constant BSPICS3Pin : std_logic_vector(7 downto 0) := x"88";
		constant BSPICS4Pin : std_logic_vector(7 downto 0) := x"89";
		constant BSPICS5Pin : std_logic_vector(7 downto 0) := x"8A";
		constant BSPICS6Pin : std_logic_vector(7 downto 0) := x"8B";
		constant BSPICS7Pin : std_logic_vector(7 downto 0) := x"8C";
	
	constant LEDTag : std_logic_vector(7 downto 0) := x"80";
	
	
	constant emptypin : std_logic_vector(31 downto 0) := x"00000000";
	constant empty : std_logic_vector(31 downto 0) := x"00000000";
	constant PadT : std_logic_vector(7 downto 0) := x"00";
	constant MaxModules : integer := 32;			-- maximum number of module types 
	constant MaxPins : integer := 128;				-- maximum number of I/O pins (may change to 144 with 3X20)

-- would be better to change all the pindescs to records
-- but that requires reversing the byte order of the constant
-- pindesc arrays, some other day...

	type PinDescRecord is  -- not used yet!
	record
		SecPin : std_logic_vector(7 downto 0);	
		SecFunc : std_logic_vector(7 downto 0);	
		SecInst : std_logic_vector(7 downto 0);	
		PriFunc : std_logic_vector(7 downto 0);	
	end record;
	
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
		(IOPortTag,		x"00",	ClockLowTag,	x"04",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
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
		(StepGenTag,	x"00",	ClockLowTag,	x"03",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
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
		IOPortTag & x"00" & PWMTag  & PWMAOutPin,				-- I/O 20			
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 21
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 22			
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
		IOPortTag & x"00" & NullTag & x"00",					-- I/O 40
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

	constant ModuleID_72Pin_2x7I65 : ModuleIDType :=( 
		(WatchDogTag,	 		x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,			x"00",	WatchDogMPBitMask),
		(IOPortTag,				x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,				x"00",	IOPortMPBitMask),
		(MuxedQcountTag,		x"02",	ClockLowTag,	x"10",	QcounterAddr&PadT,			MuxedQCounterNumRegs,	x"00",	MuxedQCounterMPBitMask),
		(MuxedQCountSelTag,	x"00",	ClockLowTag,	x"01",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(BSPITag,				x"00",	ClockLowTag,	x"02",	BSPIDataAddr&PadT,			BSPINumRegs,				x"00",	BSPIMPBitMask),
		(LEDTag,					x"00",	ClockLowTag,	x"01",	LEDAddr&PadT,					LEDNumRegs,					x"00",	LEDMPBitMask),	
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000"),
		(NullTag,				x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",						x"00",	x"00000000")
		);
			
	constant PinDesc_72Pin_2x7I65 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"00" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 00
		IOPortTag & x"00" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 01	
		IOPortTag & x"00" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 02		
		IOPortTag & x"01" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 03		
		IOPortTag & x"01" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 04		
		IOPortTag & x"01" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 05		
		IOPortTag & x"02" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 06			
		IOPortTag & x"02" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 07		
		IOPortTag & x"02" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 08		
		IOPortTag & x"03" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 09
		IOPortTag & x"03" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 10
		IOPortTag & x"03" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 11
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 12		
		IOPortTag & x"00" & MuxedQCountSelTag & MuxedQCountSel0Pin,	-- I/O 37
		IOPortTag & x"00" & BSPITag & BSPIFramePin,						-- I/O 14	
		IOPortTag & x"00" & BSPITag & BSPIOutPin,							-- I/O 15
		IOPortTag & x"00" & BSPITag & BSPIClkPin,							-- I/O 16			
		IOPortTag & x"00" & BSPITag & BSPIInPin,							-- I/O 17
		IOPortTag & x"00" & BSPITag & BSPICS2Pin,							-- I/O 18			
		IOPortTag & x"00" & BSPITag & BSPICS1Pin,							-- I/O 19
		IOPortTag & x"00" & BSPITag & BSPICS0Pin,							-- I/O 20			
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 21
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 22			
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 23
					
					
		IOPortTag & x"04" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 24
		IOPortTag & x"04" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 25
		IOPortTag & x"04" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 26
		IOPortTag & x"05" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 27
		IOPortTag & x"05" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 28
		IOPortTag & x"05" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 29
		IOPortTag & x"06" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 30
		IOPortTag & x"06" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 31
		IOPortTag & x"06" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 32
		IOPortTag & x"07" & MuxedQCountTag & MuxedQCountQAPin,		-- I/O 33
		IOPortTag & x"07" & MuxedQCountTag & MuxedQCountQBPin,		-- I/O 34
		IOPortTag & x"07" & MuxedQCountTag & MuxedQCountIDXPin,		-- I/O 35
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 36
		IOPortTag & x"00" & MuxedQCountSelTag & MuxedQCountSel0Pin,	-- I/O 37
		IOPortTag & x"01" & BSPITag & BSPIFramePin,						-- I/O 38
		IOPortTag & x"01" & BSPITag & BSPIOutPin,							-- I/O 39
		IOPortTag & x"01" & BSPITag & BSPIClkPin,							-- I/O 40
		IOPortTag & x"01" & BSPITag & BSPIInPin,							-- I/O 41
		IOPortTag & x"01" & BSPITag & BSPICS2Pin,							-- I/O 42
		IOPortTag & x"01" & BSPITag & BSPICS1Pin,							-- I/O 43
		IOPortTag & x"01" & BSPITag & BSPICS0Pin,							-- I/O 44
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 45
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 46
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 47

		IOPortTag & x"00" & NullTag & x"00",								-- I/O 48
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 49
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 50
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 51
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 52
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 53
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 54
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 55
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 56
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 57
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 58
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 59
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 60
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 61
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 62
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 63
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 64
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 65
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 66
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 67
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 68
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 69
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 70
		IOPortTag & x"00" & NullTag & x"00",								-- I/O 71

		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);					

	constant ModuleID_SVST8_4IM2 : ModuleIDType :=( 
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
		
	
	
	constant PinDesc_SVST8_4IM2 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & QCountQBPin, 		 	-- I/O 00
		IOPortTag & x"01" & QCountTag & QCountQAPin,        	-- I/O 01
		IOPortTag & x"00" & QCountTag & QCountQBPin,        	-- I/O 02
		IOPortTag & x"00" & QCountTag & QCountQAPin,        	-- I/O 03
		IOPortTag & x"01" & QCountTag & QCountIDXPin,       	-- I/O 04
		IOPortTag & x"00" & QCountTag & QCountIDXPin,       	-- I/O 05
		IOPortTag & x"01" & PWMTag & PWMAOutPin,            	-- I/O 06
		IOPortTag & x"00" & PWMTag & PWMAOutPin,            	-- I/O 07
		IOPortTag & x"01" & PWMTag & PWMBDirPin,            	-- I/O 08
		IOPortTag & x"00" & PWMTag & PWMBDirPin,            	-- I/O 09
		IOPortTag & x"01" & PWMTag & PWMCEnaPin,            	-- I/O 10
		IOPortTag & x"00" & PWMTag & PWMCEnaPin,            	-- I/O 11
		IOPortTag & x"03" & QCountTag & QCountQBPin,        	-- I/O 12
		IOPortTag & x"03" & QCountTag & QCountQAPin,        	-- I/O 37
		IOPortTag & x"02" & QCountTag & QCountQBPin,        	-- I/O 14
		IOPortTag & x"02" & QCountTag & QCountQAPin,       	-- I/O 15
		IOPortTag & x"03" & QCountTag & QCountIDXPin,      	-- I/O 16
		IOPortTag & x"02" & QCountTag & QCountIDXPin,       	-- I/O 17
		IOPortTag & x"03" & PWMTag & PWMAOutPin,            	-- I/O 18
		IOPortTag & x"02" & PWMTag & PWMAOutPin,            	-- I/O 19
		IOPortTag & x"03" & PWMTag & PWMBDirPin,            	-- I/O 20
		IOPortTag & x"02" & PWMTag & PWMBDirPin,            	-- I/O 21
		IOPortTag & x"03" & PWMTag & PWMCEnaPin,            	-- I/O 22
		IOPortTag & x"02" & PWMTag & PWMCenaPin,            	-- I/O 23
					                                   
		IOPortTag & x"05" & QCountTag & QCountQBPin,        	-- I/O 24
		IOPortTag & x"05" & QCountTag & QCountQAPin,        	-- I/O 25	
		IOPortTag & x"04" & QCountTag & QCountQBPin,        	-- I/O 26
		IOPortTag & x"04" & QCountTag & QCountQAPin,        	-- I/O 27	
		IOPortTag & x"05" & QCountTag & QCountIDXPin,       	-- I/O 28 
		IOPortTag & x"04" & QCountTag & QCountIDXPin,       	-- I/O 29 
		IOPortTag & x"05" & PWMTag & PWMAOutPin,            	-- I/O 30
		IOPortTag & x"04" & PWMTag & PWMAOutPin,            	-- I/O 31
		IOPortTag & x"05" & PWMTag & PWMBDirPin,            	-- I/O 32
		IOPortTag & x"04" & PWMTag & PWMBDirPin,            	-- I/O 33
		IOPortTag & x"05" & PWMTag & PWMCEnaPin,            	-- I/O 34
		IOPortTag & x"04" & PWMTag & PWMCEnaPin,            	-- I/O 35
		IOPortTag & x"07" & QCountTag & QCountQBPin,        	-- I/O 36
		IOPortTag & x"07" & QCountTag & QCountQAPin,        	-- I/O 37	
		IOPortTag & x"06" & QCountTag & QCountQBPin,        	-- I/O 38	
		IOPortTag & x"06" & QCountTag & QCountQAPin,        	-- I/O 39	
		IOPortTag & x"07" & QCountTag & QCountIDXPin,       	-- I/O 40 
		IOPortTag & x"06" & QCountTag & QCountIDXPin,       	-- I/O 41 
		IOPortTag & x"07" & PWMTag & PWMAOutPin,            	-- I/O 42
		IOPortTag & x"06" & PWMTag & PWMAOutPin,            	-- I/O 43
		IOPortTag & x"07" & PWMTag & PWMBDirPin,            	-- I/O 44
		IOPortTag & x"06" & PWMTag & PWMBDirPin,            	-- I/O 45
		IOPortTag & x"07" & PWMTag & PWMCEnaPin,            	-- I/O 46
		IOPortTag & x"06" & PWMTag & PWMCEnaPin,           	-- I/O 47	
																					
		IOPortTag & x"00" & QCountTag & QCountIdxMaskPin,     -- I/O 48   
		IOPortTag & x"01" & QCountTag & QCountIdxMaskPin,   	-- I/O 49
		IOPortTag & x"02" & QCountTag & QCountIdxMaskPin,   	-- I/O 50
		IOPortTag & x"03" & QCountTag & QCountIdxMaskPin,   	-- I/O 51
		IOPortTag & x"04" & QCountTag & QCountIdxMaskPin,   	-- I/O 52
		IOPortTag & x"05" & QCountTag & QCountIdxMaskPin,		-- I/O 53
		IOPortTag & x"06" & QCountTag & QCountIdxMaskPin,		-- I/O 54
		IOPortTag & x"07" & QCountTag & QCountIdxMaskPin,		-- I/O 55
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 56
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 57
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 58
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 59
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 60
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 61
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 62
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 63
		IOPortTag & x"00" & StepGenTag & StepGenStepPin,      -- I/O 64
		IOPortTag & x"00" & StepGenTag & StepGenDirPin,			-- I/O 65
		IOPortTag & x"01" & StepGenTag & StepGenStepPin,		-- I/O 66
		IOPortTag & x"01" & StepGenTag & StepGenDirPin,			-- I/O 67
		IOPortTag & x"02" & StepGenTag & StepGenStepPin,		-- I/O 68
		IOPortTag & x"02" & StepGenTag & StepGenDirPin,			-- I/O 69
		IOPortTag & x"03" & StepGenTag & StepGenStepPin,		-- I/O 70
		IOPortTag & x"03" & StepGenTag & StepGenDirPin,			-- I/O 71
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_SVUA8_4 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"08",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(UARTTTag,		x"00",	ClockLowTag,	x"04",	UARTTDataAddr&PadT,			UARTTNumRegs,			x"00",	UARTTMPBitMask),
		(UARTRTag,		x"00",	ClockLowTag,	x"04",	UARTRDataAddr&PadT,			UARTRNumRegs,			x"00",	UARTRMPBitMask),
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
		
	
	
	constant PinDesc_SVUA8_4 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & QCountQBPin, 		 	-- I/O 00
		IOPortTag & x"01" & QCountTag & QCountQAPin,        	-- I/O 01
		IOPortTag & x"00" & QCountTag & QCountQBPin,        	-- I/O 02
		IOPortTag & x"00" & QCountTag & QCountQAPin,        	-- I/O 03
		IOPortTag & x"01" & QCountTag & QCountIDXPin,       	-- I/O 04
		IOPortTag & x"00" & QCountTag & QCountIDXPin,       	-- I/O 05
		IOPortTag & x"01" & PWMTag & PWMAOutPin,            	-- I/O 06
		IOPortTag & x"00" & PWMTag & PWMAOutPin,            	-- I/O 07
		IOPortTag & x"01" & PWMTag & PWMBDirPin,            	-- I/O 08
		IOPortTag & x"00" & PWMTag & PWMBDirPin,            	-- I/O 09
		IOPortTag & x"01" & PWMTag & PWMCEnaPin,            	-- I/O 10
		IOPortTag & x"00" & PWMTag & PWMCEnaPin,            	-- I/O 11
		IOPortTag & x"03" & QCountTag & QCountQBPin,        	-- I/O 12
		IOPortTag & x"03" & QCountTag & QCountQAPin,        	-- I/O 37
		IOPortTag & x"02" & QCountTag & QCountQBPin,        	-- I/O 14
		IOPortTag & x"02" & QCountTag & QCountQAPin,       	-- I/O 15
		IOPortTag & x"03" & QCountTag & QCountIDXPin,      	-- I/O 16
		IOPortTag & x"02" & QCountTag & QCountIDXPin,       	-- I/O 17
		IOPortTag & x"03" & PWMTag & PWMAOutPin,            	-- I/O 18
		IOPortTag & x"02" & PWMTag & PWMAOutPin,            	-- I/O 19
		IOPortTag & x"03" & PWMTag & PWMBDirPin,            	-- I/O 20
		IOPortTag & x"02" & PWMTag & PWMBDirPin,            	-- I/O 21
		IOPortTag & x"03" & PWMTag & PWMCEnaPin,            	-- I/O 22
		IOPortTag & x"02" & PWMTag & PWMCenaPin,            	-- I/O 23
					                                   
		IOPortTag & x"05" & QCountTag & QCountQBPin,        	-- I/O 24
		IOPortTag & x"05" & QCountTag & QCountQAPin,        	-- I/O 25	
		IOPortTag & x"04" & QCountTag & QCountQBPin,        	-- I/O 26
		IOPortTag & x"04" & QCountTag & QCountQAPin,        	-- I/O 27	
		IOPortTag & x"05" & QCountTag & QCountIDXPin,       	-- I/O 28 
		IOPortTag & x"04" & QCountTag & QCountIDXPin,       	-- I/O 29 
		IOPortTag & x"05" & PWMTag & PWMAOutPin,            	-- I/O 30
		IOPortTag & x"04" & PWMTag & PWMAOutPin,            	-- I/O 31
		IOPortTag & x"05" & PWMTag & PWMBDirPin,            	-- I/O 32
		IOPortTag & x"04" & PWMTag & PWMBDirPin,            	-- I/O 33
		IOPortTag & x"05" & PWMTag & PWMCEnaPin,            	-- I/O 34
		IOPortTag & x"04" & PWMTag & PWMCEnaPin,            	-- I/O 35
		IOPortTag & x"07" & QCountTag & QCountQBPin,        	-- I/O 36
		IOPortTag & x"07" & QCountTag & QCountQAPin,        	-- I/O 37	
		IOPortTag & x"06" & QCountTag & QCountQBPin,        	-- I/O 38	
		IOPortTag & x"06" & QCountTag & QCountQAPin,        	-- I/O 39	
		IOPortTag & x"07" & QCountTag & QCountIDXPin,       	-- I/O 40 
		IOPortTag & x"06" & QCountTag & QCountIDXPin,       	-- I/O 41 
		IOPortTag & x"07" & PWMTag & PWMAOutPin,            	-- I/O 42
		IOPortTag & x"06" & PWMTag & PWMAOutPin,            	-- I/O 43
		IOPortTag & x"07" & PWMTag & PWMBDirPin,            	-- I/O 44
		IOPortTag & x"06" & PWMTag & PWMBDirPin,            	-- I/O 45
		IOPortTag & x"07" & PWMTag & PWMCEnaPin,            	-- I/O 46
		IOPortTag & x"06" & PWMTag & PWMCEnaPin,           	-- I/O 47	
																					
		IOPortTag & x"00" & UARTRTag & URDataPin, 			   -- I/O 48   
		IOPortTag & x"01" & UARTRTag & URDataPin,   				-- I/O 49
		IOPortTag & x"02" & UARTRTag & URDataPin,   				-- I/O 50
		IOPortTag & x"03" & UARTRTag & URDataPin,   				-- I/O 51
		IOPortTag & x"00" & UARTTTag & UTDataPin,   				-- I/O 52
		IOPortTag & x"00" & UARTTTag & UTDrvEnPin,				-- I/O 53
		IOPortTag & x"01" & UARTTTag & UTDataPin,					-- I/O 54
		IOPortTag & x"01" & UARTTTag & UTDrvEnPin,				-- I/O 55
		IOPortTag & x"02" & UARTTTag & UTDataPin,       	 	-- I/O 56
		IOPortTag & x"02" & UARTTTag & UTDrvEnPin,        		-- I/O 57
		IOPortTag & x"03" & UARTTTag & UTDataPin,      			-- I/O 58
		IOPortTag & x"03" & UARTTTag & UTDrvEnPin,        		-- I/O 59
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 60
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 61
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 62
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 63
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 64
		IOPortTag & x"00" & NullTag & x"00",						-- I/O 65
		IOPortTag & x"00" & NullTag & x"00",						-- I/O 66
		IOPortTag & x"00" & NullTag & x"00",						-- I/O 67
		IOPortTag & x"00" & NullTag & x"00",						-- I/O 68
		IOPortTag & x"00" & NullTag & x"00",						-- I/O 69
		IOPortTag & x"00" & NullTag & x"00",						-- I/O 70
		IOPortTag & x"00" & NullTag & x"00",						-- I/O 71
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_SVUA8_8 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(PWMTag,			x"00",	ClockHighTag,	x"08",	PWMValAddr&PadT,				PWMNumRegs,				x"00",	PWMMPBitMask),
		(UARTTTag,		x"00",	ClockLowTag,	x"08",	UARTTDataAddr&PadT,			UARTTNumRegs,			x"00",	UARTTMPBitMask),
		(UARTRTag,		x"00",	ClockLowTag,	x"08",	UARTRDataAddr&PadT,			UARTRNumRegs,			x"00",	UARTRMPBitMask),
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
		
	
	
	constant PinDesc_SVUA8_8 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"01" & QCountTag & QCountQBPin, 		 	-- I/O 00
		IOPortTag & x"01" & QCountTag & QCountQAPin,        	-- I/O 01
		IOPortTag & x"00" & QCountTag & QCountQBPin,        	-- I/O 02
		IOPortTag & x"00" & QCountTag & QCountQAPin,        	-- I/O 03
		IOPortTag & x"01" & QCountTag & QCountIDXPin,       	-- I/O 04
		IOPortTag & x"00" & QCountTag & QCountIDXPin,       	-- I/O 05
		IOPortTag & x"01" & PWMTag & PWMAOutPin,            	-- I/O 06
		IOPortTag & x"00" & PWMTag & PWMAOutPin,            	-- I/O 07
		IOPortTag & x"01" & PWMTag & PWMBDirPin,            	-- I/O 08
		IOPortTag & x"00" & PWMTag & PWMBDirPin,            	-- I/O 09
		IOPortTag & x"01" & PWMTag & PWMCEnaPin,            	-- I/O 10
		IOPortTag & x"00" & PWMTag & PWMCEnaPin,            	-- I/O 11
		IOPortTag & x"03" & QCountTag & QCountQBPin,        	-- I/O 12
		IOPortTag & x"03" & QCountTag & QCountQAPin,        	-- I/O 37
		IOPortTag & x"02" & QCountTag & QCountQBPin,        	-- I/O 14
		IOPortTag & x"02" & QCountTag & QCountQAPin,       	-- I/O 15
		IOPortTag & x"03" & QCountTag & QCountIDXPin,      	-- I/O 16
		IOPortTag & x"02" & QCountTag & QCountIDXPin,       	-- I/O 17
		IOPortTag & x"03" & PWMTag & PWMAOutPin,            	-- I/O 18
		IOPortTag & x"02" & PWMTag & PWMAOutPin,            	-- I/O 19
		IOPortTag & x"03" & PWMTag & PWMBDirPin,            	-- I/O 20
		IOPortTag & x"02" & PWMTag & PWMBDirPin,            	-- I/O 21
		IOPortTag & x"03" & PWMTag & PWMCEnaPin,            	-- I/O 22
		IOPortTag & x"02" & PWMTag & PWMCenaPin,            	-- I/O 23
					                                   
		IOPortTag & x"05" & QCountTag & QCountQBPin,        	-- I/O 24
		IOPortTag & x"05" & QCountTag & QCountQAPin,        	-- I/O 25	
		IOPortTag & x"04" & QCountTag & QCountQBPin,        	-- I/O 26
		IOPortTag & x"04" & QCountTag & QCountQAPin,        	-- I/O 27	
		IOPortTag & x"05" & QCountTag & QCountIDXPin,       	-- I/O 28 
		IOPortTag & x"04" & QCountTag & QCountIDXPin,       	-- I/O 29 
		IOPortTag & x"05" & PWMTag & PWMAOutPin,            	-- I/O 30
		IOPortTag & x"04" & PWMTag & PWMAOutPin,            	-- I/O 31
		IOPortTag & x"05" & PWMTag & PWMBDirPin,            	-- I/O 32
		IOPortTag & x"04" & PWMTag & PWMBDirPin,            	-- I/O 33
		IOPortTag & x"05" & PWMTag & PWMCEnaPin,            	-- I/O 34
		IOPortTag & x"04" & PWMTag & PWMCEnaPin,            	-- I/O 35
		IOPortTag & x"07" & QCountTag & QCountQBPin,        	-- I/O 36
		IOPortTag & x"07" & QCountTag & QCountQAPin,        	-- I/O 37	
		IOPortTag & x"06" & QCountTag & QCountQBPin,        	-- I/O 38	
		IOPortTag & x"06" & QCountTag & QCountQAPin,        	-- I/O 39	
		IOPortTag & x"07" & QCountTag & QCountIDXPin,       	-- I/O 40 
		IOPortTag & x"06" & QCountTag & QCountIDXPin,       	-- I/O 41 
		IOPortTag & x"07" & PWMTag & PWMAOutPin,            	-- I/O 42
		IOPortTag & x"06" & PWMTag & PWMAOutPin,            	-- I/O 43
		IOPortTag & x"07" & PWMTag & PWMBDirPin,            	-- I/O 44
		IOPortTag & x"06" & PWMTag & PWMBDirPin,            	-- I/O 45
		IOPortTag & x"07" & PWMTag & PWMCEnaPin,            	-- I/O 46
		IOPortTag & x"06" & PWMTag & PWMCEnaPin,           	-- I/O 47	
																					
		IOPortTag & x"00" & UARTRTag & URDataPin, 			   -- I/O 48   
		IOPortTag & x"01" & UARTRTag & URDataPin,   				-- I/O 49
		IOPortTag & x"02" & UARTRTag & URDataPin,   				-- I/O 50
		IOPortTag & x"03" & UARTRTag & URDataPin,   				-- I/O 51
		IOPortTag & x"00" & UARTTTag & UTDataPin,   				-- I/O 52
		IOPortTag & x"00" & UARTTTag & UTDrvEnPin,				-- I/O 53
		IOPortTag & x"01" & UARTTTag & UTDataPin,					-- I/O 54
		IOPortTag & x"01" & UARTTTag & UTDrvEnPin,				-- I/O 55
		IOPortTag & x"02" & UARTTTag & UTDataPin,       	 	-- I/O 56
		IOPortTag & x"02" & UARTTTag & UTDrvEnPin,        		-- I/O 57
		IOPortTag & x"03" & UARTTTag & UTDataPin,      			-- I/O 58
		IOPortTag & x"03" & UARTTTag & UTDrvEnPin,        		-- I/O 59
		IOPortTag & x"04" & UARTRTag & URDataPin, 			   -- I/O 60   
		IOPortTag & x"05" & UARTRTag & URDataPin,   				-- I/O 61
		IOPortTag & x"06" & UARTRTag & URDataPin,   				-- I/O 62
		IOPortTag & x"07" & UARTRTag & URDataPin,   				-- I/O 63
		IOPortTag & x"04" & UARTTTag & UTDataPin,   				-- I/O 64
		IOPortTag & x"04" & UARTTTag & UTDrvEnPin,				-- I/O 65
		IOPortTag & x"05" & UARTTTag & UTDataPin,					-- I/O 66
		IOPortTag & x"05" & UARTTTag & UTDrvEnPin,				-- I/O 67
		IOPortTag & x"06" & UARTTTag & UTDataPin,       	 	-- I/O 68
		IOPortTag & x"06" & UARTTTag & UTDrvEnPin,        		-- I/O 69
		IOPortTag & x"07" & UARTTTag & UTDataPin,      			-- I/O 70
		IOPortTag & x"07" & UARTTTag & UTDrvEnPin,        		-- I/O 71
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_JDosa66 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"06",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
		(StepGenTag,	x"00",	ClockLowTag,	x"06",	StepGenRateAddr&PadT,		StepGenNumRegs,		x"00",	StepGenMPBitMask),
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
		
	
	
	constant PinDesc_JDosa66 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"00" & QCountTag & QCountQAPin, 		 	-- I/O 00
		IOPortTag & x"00" & QCountTag & QCountQBPin,        	-- I/O 01
		IOPortTag & x"00" & QCountTag & QCountIDXPin,       	-- I/O 02
		IOPortTag & x"01" & QCountTag & QCountQAPin,        	-- I/O 03
		IOPortTag & x"01" & QCountTag & QCountQBPin,        	-- I/O 04
		IOPortTag & x"01" & QCountTag & QCountIDXPin,       	-- I/O 05
		IOPortTag & x"02" & QCountTag & QCountQAPin, 		 	-- I/O 06
		IOPortTag & x"02" & QCountTag & QCountQBPin,        	-- I/O 07
		IOPortTag & x"02" & QCountTag & QCountIDXPin,       	-- I/O 08
		IOPortTag & x"03" & QCountTag & QCountQAPin,        	-- I/O 09
		IOPortTag & x"03" & QCountTag & QCountQBPin,        	-- I/O 10
		IOPortTag & x"03" & QCountTag & QCountIDXPin,       	-- I/O 11
		IOPortTag & x"04" & QCountTag & QCountQAPin,        	-- I/O 12
		IOPortTag & x"04" & QCountTag & QCountQBPin,        	-- I/O 37
		IOPortTag & x"04" & QCountTag & QCountIDXPin,        	-- I/O 14
		IOPortTag & x"05" & QCountTag & QCountQAPin,       	-- I/O 15
		IOPortTag & x"05" & QCountTag & QCountQBPin,	      	-- I/O 16
		IOPortTag & x"05" & QCountTag & QCountIDXPin,       	-- I/O 17
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 18
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 19
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 20
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 21
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 22
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 23
					                                   
		IOPortTag & x"00" & StepGenTag & StepGenStepPin,      -- I/O 24
		IOPortTag & x"00" & StepGenTag & StepGenDirPin,	      -- I/O 25	
		IOPortTag & x"01" & StepGenTag & StepGenStepPin,      -- I/O 26
		IOPortTag & x"01" & StepGenTag & StepGenDirPin,	      -- I/O 27	
		IOPortTag & x"02" & StepGenTag & StepGenStepPin,      -- I/O 28 
		IOPortTag & x"02" & StepGenTag & StepGenDirPin,	      -- I/O 29 
		IOPortTag & x"03" & StepGenTag & StepGenStepPin,      -- I/O 30
		IOPortTag & x"03" & StepGenTag & StepGenDirPin,	      -- I/O 31
		IOPortTag & x"04" & StepGenTag & StepGenStepPin,		-- I/O 32
		IOPortTag & x"04" & StepGenTag & StepGenDirPin,			-- I/O 33
		IOPortTag & x"05" & StepGenTag & StepGenStepPin,		-- I/O 34
		IOPortTag & x"05" & StepGenTag & StepGenDirPin,			-- I/O 35
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 36
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 37	
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 38	
		IOPortTag & x"00" & NullTag & x"00",     			   	-- I/O 39	
		IOPortTag & x"00" & NullTag & x"00",      			 	-- I/O 40 
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 41 
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 42
		IOPortTag & x"00" & NullTag & x"00",     				  	-- I/O 43
		IOPortTag & x"00" & NullTag & x"00",       		    	-- I/O 44
		IOPortTag & x"00" & NullTag & x"00",           			-- I/O 45
		IOPortTag & x"00" & NullTag & x"00",           			-- I/O 46
		IOPortTag & x"00" & NullTag & x"00",          			-- I/O 47	
																					
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 48   
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 49
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 50
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 51
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 52
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 53
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 54
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 55
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 56
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 57
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 58
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 59
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 60
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 61
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 62
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 63
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 64
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 65
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 66
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 67
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 68
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 69
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 70
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 71
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_JDosa88 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"08",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
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
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000"),
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	
	
	constant PinDesc_JDosa88 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"00" & QCountTag & QCountQAPin, 		 	-- I/O 00
		IOPortTag & x"00" & QCountTag & QCountQBPin,        	-- I/O 01
		IOPortTag & x"00" & QCountTag & QCountIDXPin,       	-- I/O 02
		IOPortTag & x"01" & QCountTag & QCountQAPin,        	-- I/O 03
		IOPortTag & x"01" & QCountTag & QCountQBPin,        	-- I/O 04
		IOPortTag & x"01" & QCountTag & QCountIDXPin,       	-- I/O 05
		IOPortTag & x"02" & QCountTag & QCountQAPin, 		 	-- I/O 06
		IOPortTag & x"02" & QCountTag & QCountQBPin,        	-- I/O 07
		IOPortTag & x"02" & QCountTag & QCountIDXPin,       	-- I/O 08
		IOPortTag & x"03" & QCountTag & QCountQAPin,        	-- I/O 09
		IOPortTag & x"03" & QCountTag & QCountQBPin,        	-- I/O 10
		IOPortTag & x"03" & QCountTag & QCountIDXPin,       	-- I/O 11
		IOPortTag & x"04" & QCountTag & QCountQAPin,        	-- I/O 12
		IOPortTag & x"04" & QCountTag & QCountQBPin,        	-- I/O 37
		IOPortTag & x"04" & QCountTag & QCountIDXPin,        	-- I/O 14
		IOPortTag & x"05" & QCountTag & QCountQAPin,       	-- I/O 15
		IOPortTag & x"05" & QCountTag & QCountQBPin,	      	-- I/O 16
		IOPortTag & x"05" & QCountTag & QCountIDXPin,       	-- I/O 17
		IOPortTag & x"06" & QCountTag & QCountQAPin,      		-- I/O 18
		IOPortTag & x"06" & QCountTag & QCountQBPin,      		-- I/O 19
		IOPortTag & x"06" & QCountTag & QCountIDXPin,      	-- I/O 20
		IOPortTag & x"07" & QCountTag & QCountQAPin,       	-- I/O 21
		IOPortTag & x"07" & QCountTag & QCountQBPin,	      	-- I/O 22
		IOPortTag & x"07" & QCountTag & QCountIDXPin,      	-- I/O 23
					                                   
		IOPortTag & x"00" & StepGenTag & StepGenStepPin,      -- I/O 24
		IOPortTag & x"00" & StepGenTag & StepGenDirPin,	      -- I/O 25	
		IOPortTag & x"01" & StepGenTag & StepGenStepPin,      -- I/O 26
		IOPortTag & x"01" & StepGenTag & StepGenDirPin,	      -- I/O 27	
		IOPortTag & x"02" & StepGenTag & StepGenStepPin,      -- I/O 28 
		IOPortTag & x"02" & StepGenTag & StepGenDirPin,	      -- I/O 29 
		IOPortTag & x"03" & StepGenTag & StepGenStepPin,      -- I/O 30
		IOPortTag & x"03" & StepGenTag & StepGenDirPin,	      -- I/O 31
		IOPortTag & x"04" & StepGenTag & StepGenStepPin,		-- I/O 32
		IOPortTag & x"04" & StepGenTag & StepGenDirPin,			-- I/O 33
		IOPortTag & x"05" & StepGenTag & StepGenStepPin,		-- I/O 34
		IOPortTag & x"05" & StepGenTag & StepGenDirPin,			-- I/O 35
		IOPortTag & x"06" & StepGenTag & StepGenStepPin,  		-- I/O 36
		IOPortTag & x"06" & StepGenTag & StepGenDirPin,	      -- I/O 37	
		IOPortTag & x"07" & StepGenTag & StepGenStepPin,      -- I/O 38	
		IOPortTag & x"07" & StepGenTag & StepGenDirPin,	     	-- I/O 39	
		IOPortTag & x"00" & NullTag & x"00",      			 	-- I/O 40 
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 41 
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 42
		IOPortTag & x"00" & NullTag & x"00",     				  	-- I/O 43
		IOPortTag & x"00" & NullTag & x"00",       		    	-- I/O 44
		IOPortTag & x"00" & NullTag & x"00",           			-- I/O 45
		IOPortTag & x"00" & NullTag & x"00",           			-- I/O 46
		IOPortTag & x"00" & NullTag & x"00",          			-- I/O 47	
																					
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 48   
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 49
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 50
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 51
		IOPortTag & x"00" & NullTag & x"00",    					-- I/O 52
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 53
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 54
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 55
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 56
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 57
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 58
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 59
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 60
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 61
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 62
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 63
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 64
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 65
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 66
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 67
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 68
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 69
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 70
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 71
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);

	constant ModuleID_JDosa1212 : ModuleIDType :=( 
		(WatchDogTag,	x"00",	ClockLowTag,	x"01",	WatchDogTimeAddr&PadT,		WatchDogNumRegs,		x"00",	WatchDogMPBitMask),
		(IOPortTag,		x"00",	ClockLowTag,	x"03",	PortAddr&PadT,					IOPortNumRegs,			x"00",	IOPortMPBitMask),
		(QcountTag,		x"02",	ClockLowTag,	x"0C",	QcounterAddr&PadT,			QCounterNumRegs,		x"00",	QCounterMPBitMask),
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
		(NullTag,		x"00",	NullTag,			x"00",	NullAddr&PadT,					x"00",					x"00",	x"00000000")
		);
		
	
	
	constant PinDesc_JDosa1212 : PinDescType :=(
-- 	Base func  sec unit sec func 	 sec pin		
		IOPortTag & x"00" & QCountTag & QCountQAPin, 		 	-- I/O 00
		IOPortTag & x"00" & QCountTag & QCountQBPin,        	-- I/O 01
		IOPortTag & x"00" & QCountTag & QCountIDXPin,       	-- I/O 02
		IOPortTag & x"01" & QCountTag & QCountQAPin,        	-- I/O 03
		IOPortTag & x"01" & QCountTag & QCountQBPin,        	-- I/O 04
		IOPortTag & x"01" & QCountTag & QCountIDXPin,       	-- I/O 05
		IOPortTag & x"02" & QCountTag & QCountQAPin, 		 	-- I/O 06
		IOPortTag & x"02" & QCountTag & QCountQBPin,        	-- I/O 07
		IOPortTag & x"02" & QCountTag & QCountIDXPin,       	-- I/O 08
		IOPortTag & x"03" & QCountTag & QCountQAPin,        	-- I/O 09
		IOPortTag & x"03" & QCountTag & QCountQBPin,        	-- I/O 10
		IOPortTag & x"03" & QCountTag & QCountIDXPin,       	-- I/O 11
		IOPortTag & x"04" & QCountTag & QCountQAPin,        	-- I/O 12
		IOPortTag & x"04" & QCountTag & QCountQBPin,        	-- I/O 37
		IOPortTag & x"04" & QCountTag & QCountIDXPin,        	-- I/O 14
		IOPortTag & x"05" & QCountTag & QCountQAPin,       	-- I/O 15
		IOPortTag & x"05" & QCountTag & QCountQBPin,	      	-- I/O 16
		IOPortTag & x"05" & QCountTag & QCountIDXPin,       	-- I/O 17
		IOPortTag & x"06" & QCountTag & QCountQAPin,      		-- I/O 18
		IOPortTag & x"06" & QCountTag & QCountQBPin,      		-- I/O 19
		IOPortTag & x"06" & QCountTag & QCountIDXPin,      	-- I/O 20
		IOPortTag & x"07" & QCountTag & QCountQAPin,       	-- I/O 21
		IOPortTag & x"07" & QCountTag & QCountQBPin,	      	-- I/O 22
		IOPortTag & x"07" & QCountTag & QCountIDXPin,      	-- I/O 23
					                                   
		IOPortTag & x"00" & StepGenTag & StepGenStepPin,      -- I/O 24
		IOPortTag & x"00" & StepGenTag & StepGenDirPin,	      -- I/O 25	
		IOPortTag & x"01" & StepGenTag & StepGenStepPin,      -- I/O 26
		IOPortTag & x"01" & StepGenTag & StepGenDirPin,	      -- I/O 27	
		IOPortTag & x"02" & StepGenTag & StepGenStepPin,      -- I/O 28 
		IOPortTag & x"02" & StepGenTag & StepGenDirPin,	      -- I/O 29 
		IOPortTag & x"03" & StepGenTag & StepGenStepPin,      -- I/O 30
		IOPortTag & x"03" & StepGenTag & StepGenDirPin,	      -- I/O 31
		IOPortTag & x"04" & StepGenTag & StepGenStepPin,		-- I/O 32
		IOPortTag & x"04" & StepGenTag & StepGenDirPin,			-- I/O 33
		IOPortTag & x"05" & StepGenTag & StepGenStepPin,		-- I/O 34
		IOPortTag & x"05" & StepGenTag & StepGenDirPin,			-- I/O 35
		IOPortTag & x"06" & StepGenTag & StepGenStepPin,  		-- I/O 36
		IOPortTag & x"06" & StepGenTag & StepGenDirPin,	      -- I/O 37	
		IOPortTag & x"07" & StepGenTag & StepGenStepPin,      -- I/O 38	
		IOPortTag & x"07" & StepGenTag & StepGenDirPin,	     	-- I/O 39	
		IOPortTag & x"08" & StepGenTag & StepGenStepPin,   	-- I/O 40 
		IOPortTag & x"08" & StepGenTag & StepGenDirPin,	    	-- I/O 41 
		IOPortTag & x"09" & StepGenTag & StepGenStepPin,    	-- I/O 42
		IOPortTag & x"09" & StepGenTag & StepGenDirPin,	  	 	-- I/O 43
		IOPortTag & x"0A" & StepGenTag & StepGenStepPin,     	-- I/O 44
		IOPortTag & x"0A" & StepGenTag & StepGenDirPin,	      -- I/O 45
		IOPortTag & x"0B" & StepGenTag & StepGenStepPin,      -- I/O 46
		IOPortTag & x"0B" & StepGenTag & StepGenDirPin,	      -- I/O 47	
																					
		IOPortTag & x"08" & QCountTag & QCountQAPin,        	-- I/O 48   
		IOPortTag & x"08" & QCountTag & QCountQBPin,   			-- I/O 49
		IOPortTag & x"08" & QCountTag & QCountIDXPin,     		-- I/O 50
		IOPortTag & x"09" & QCountTag & QCountQAPin,      		-- I/O 51
		IOPortTag & x"09" & QCountTag & QCountQBPin,	     		-- I/O 52
		IOPortTag & x"09" & QCountTag & QCountIDXPin,    		-- I/O 53
		IOPortTag & x"0A" & QCountTag & QCountQAPin,     		-- I/O 54
		IOPortTag & x"0A" & QCountTag & QCountQBPin,     		-- I/O 55
		IOPortTag & x"0A" & QCountTag & QCountIDXPin,        	-- I/O 56
		IOPortTag & x"0B" & QCountTag & QCountQAPin,      		-- I/O 57
		IOPortTag & x"0B" & QCountTag & QCountQBPin,	       	-- I/O 58
		IOPortTag & x"0B" & QCountTag & QCountIDXPin,        	-- I/O 59
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 60
		IOPortTag & x"00" & NullTag & x"00",        				-- I/O 61
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 62
		IOPortTag & x"00" & NullTag & x"00",       				-- I/O 63
		IOPortTag & x"00" & NullTag & x"00",      				-- I/O 64
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 65
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 66
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 67
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 68
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 69
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 70
		IOPortTag & x"00" & NullTag & x"00",   					-- I/O 71
		
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,
		emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin,emptypin);



end package IDROMParms;
	