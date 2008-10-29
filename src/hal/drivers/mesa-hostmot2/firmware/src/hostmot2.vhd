library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.ALL;
use IEEE.std_logic_UNSIGNED.ALL;
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

use work.IDROMParms.all;	
library UNISIM;
use UNISIM.VComponents.all;
	
entity HostMot2 is
  	generic
	(  
		ThePinDesc: PinDescType;
		TheModuleID: ModuleIDType;
		StepGens: integer;
		QCounters: integer;
		MuxedQCounters: integer;
		PWMGens: integer;
		SPIs: integer;
		BSPIs: integer;
		SSIs: integer;
		UARTs: integer;
		PWMRefWidth: integer;
		StepGenTableWidth: integer;
      BSPICSWidth : integer;
		IDROMType: integer;		
	   SepClocks: boolean;
		OneWS: boolean;
		UseStepGenPrescaler : boolean;
		UseIRQLogic: boolean;
		UseWatchDog: boolean;
		OffsetToModules: integer;
		OffsetToPinDesc: integer;
		ClockHigh: integer;
		ClockLow: integer;
		BoardNameLow : std_Logic_Vector(31 downto 0);
		BoardNameHigh : std_Logic_Vector(31 downto 0);
		FPGASize: integer;
		FPGAPins: integer;
		IOPorts: integer;
		IOWidth: integer;
		PortWidth: integer;
		BusWidth: integer;
		AddrWidth: integer;
		InstStride0: integer;
		InstStride1: integer;
		RegStride0: integer;
		RegStride1: integer;
		LEDCount: integer
		);
	port 
   (
     -- Generic 32  bit bus interface signals --

	ibus: in std_logic_vector(buswidth -1 downto 0);
	obus: out std_logic_vector(buswidth -1 downto 0);
	addr: in std_logic_vector(addrwidth -1 downto 2);
	read: in std_logic;
	write: in std_logic;
	clklow: in std_logic;
	clkhigh: in std_logic;
	int: out std_logic; 
	iobits: inout std_logic_vector (iowidth -1 downto 0);			
	leds: out std_logic_vector(ledcount-1 downto 0)
	);
end HostMot2;



architecture dataflow of HostMot2 is


-- decodes --
--	IDROM related signals
	signal A : std_logic_vector(addrwidth -1 downto 2);
	signal LoadIDROM: std_logic;
	signal ReadIDROM: std_logic;

	signal LoadIDROMWEn: std_logic;
	signal ReadIDROMWEn: std_logic;

	signal IDROMWEn: std_logic_vector(0 downto 0);
	signal ROMAdd: std_logic_vector(7 downto 0);

-- I/O port related signals

	signal AltData :  std_logic_vector(IOWidth-1 downto 0);
	signal PortSel: std_logic;	
	signal LoadPortCmd: std_logic_vector(IOPorts -1 downto 0);
	signal ReadPortCmd: std_logic_vector(IOPorts -1 downto 0);

	signal DDRSel: std_logic;	
	signal LoadDDRCmd: std_logic_vector(IOPorts -1 downto 0);
	signal ReadDDRCmd: std_logic_vector(IOPorts -1 downto 0);	
	signal AltDataSrcSel: std_logic;
	signal LoadAltDataSrcCmd: std_logic_vector(IOPorts -1 downto 0);
	signal OpenDrainModeSel: std_logic;
	signal LoadOpenDrainModeCmd: std_logic_vector(IOPorts -1 downto 0);
	signal OutputInvSel: std_logic;
	signal LoadOutputInvCmd: std_logic_vector(IOPorts -1 downto 0);


-- Step generator related signals
	
	signal StepGenRateSel: std_logic;
	signal LoadStepGenRate: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenRate: std_logic_vector(StepGens -1 downto 0);

	signal StepGenAccumSel: std_logic;
	signal LoadStepGenAccum: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenAccum: std_logic_vector(StepGens -1 downto 0);
	
	signal StepGenModeSel: std_logic;
	signal LoadStepGenMode: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenMode: std_logic_vector(StepGens -1 downto 0);

	signal StepGenDSUTimeSel: std_logic;
	signal LoadStepGenDSUTime: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenDSUTime: std_logic_vector(StepGens -1 downto 0);

	signal StepGenDHLDTimeSel: std_logic;
	signal LoadStepGenDHLDTime: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenDHLDTime: std_logic_vector(StepGens -1 downto 0);

	signal StepGenPulseATimeSel: std_logic;
	signal LoadStepGenPulseATime: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenPulseATime: std_logic_vector(StepGens -1 downto 0);

	signal StepGenPulseITimeSel: std_logic;
	signal LoadStepGenPulseITime: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenPulseITime: std_logic_vector(StepGens -1 downto 0);

	signal StepGenTableMaxSel: std_logic;
	signal LoadStepGenTableMax: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenTableMax: std_logic_vector(StepGens -1 downto 0);

	signal StepGenTableSel: std_logic;
	signal LoadStepGenTable: std_logic_vector(StepGens -1 downto 0);
	signal ReadStepGenTable: std_logic_vector(StepGens -1 downto 0);
	
	type StepGenOutType is array(StepGens-1 downto 0) of std_logic_vector(StepGenTableWidth-1 downto 0);
	signal StepGenOut : StepGenOutType;

-- Step generators master rate related signals

	signal LoadStepGenBasicRate: std_logic;
	signal ReadStepGenBasicRate: std_logic;
 
	signal StepGenBasicRate: std_logic;

-- Quadrature counter related signals

	signal QCounterSel : std_logic;
	signal LoadQCounter: std_logic_vector(QCounters-1 downto 0);
	signal ReadQCounter: std_logic_vector(QCounters-1 downto 0);
	
	signal QCounterCCRSel : std_logic;
	signal LoadQCounterCCR: std_logic_vector(QCounters-1 downto 0);
	signal ReadQCounterCCR: std_logic_vector(QCounters-1 downto 0);

-- Quadrature counter timestamp reference counter

	signal LoadTSDiv : std_logic;
	signal ReadTSDiv : std_logic;
	signal ReadTS : std_logic;
	signal TimeStampBus: std_logic_vector(15 downto 0);

-- Quadrature counter filter rate signals
	signal LoadQCountRate : std_logic;
	signal QCountFilterRate : std_logic;
	
-- Quadrature counter input signals
	signal QuadA: std_logic_vector(QCounters-1 downto 0);
	signal QuadB: std_logic_vector(QCounters-1 downto 0);
	signal Index: std_logic_vector(QCounters -1 downto 0);
	signal IndexMask: std_logic_vector(QCounters -1 downto 0);

--- Multiplexed Encoder related signals	
	signal MuxedQCounterSel : std_logic;
	signal LoadMuxedQCounter: std_logic_vector(MuxedQCounters-1 downto 0);
	signal ReadMuxedQCounter: std_logic_vector(MuxedQCounters-1 downto 0);
	
	signal MuxedQCounterCCRSel : std_logic;
	signal LoadMuxedQCounterCCR: std_logic_vector(MuxedQCounters-1 downto 0);
	signal ReadMuxedQCounterCCR: std_logic_vector(MuxedQCounters-1 downto 0);
-- Muxed Quadrature counter timestamp reference counter

	signal LoadMuxedTSDiv : std_logic;
	signal ReadMuxedTSDiv : std_logic;
	signal ReadMuxedTS : std_logic;
	signal MuxedTimeStampBus: std_logic_vector(15 downto 0);

-- Muxed Quadrature counter filter rate signals
	signal LoadMuxedQCountRate : std_logic;
	signal MuxedQCountFilterRate : std_logic;

	signal PreMuxedQctrSel : std_logic_vector(1 downto 0);
	signal MuxedQCtrSel : std_logic_vector(1 downto 0);
	signal MuxedQuadA: std_logic_vector(MuxedQCounters/2 -1 downto 0); -- 2 should be muxdepth constant?
	signal MuxedQuadB: std_logic_vector(MuxedQCounters/2 -1 downto 0);
	signal MuxedIndex: std_logic_vector(MuxedQCounters/2 -1 downto 0);
	signal muxedIndexMask: std_logic_vector(MuxedQCounters/2 -1 downto 0);	
	signal DemuxedIndexMask: std_logic_vector(MuxedQCounters -1 downto 0);	
	signal DeMuxedQuadA: std_logic_vector(MuxedQCounters -1 downto 0);
	signal DeMuxedQuadB: std_logic_vector(MuxedQCounters -1 downto 0);
	signal DeMuxedIndex: std_logic_vector(MuxedQCounters -1 downto 0);
	
-- PWM generator related signals
   signal NumberOfPWMS : integer;
	signal PWMGenOutA: std_logic_vector(PWMGens -1 downto 0);
	signal PWMGenOutB: std_logic_vector(PWMGens -1 downto 0);
	signal PWMGenOutC: std_logic_vector(PWMGens -1 downto 0);
	signal LoadPWMRate : std_logic;
	signal LoadPDMRate : std_logic;
	signal RefCountBus : std_logic_vector(PWMRefWidth-1 downto 0);
	signal PDMRate : std_logic;
	signal PWMValSel : std_logic;
	signal PWMCRSel : std_logic;
	signal LoadPWMVal: std_logic_vector(PWMGens -1 downto 0);
	signal LoadPWMCR: std_logic_vector(PWMGens -1 downto 0);
	signal LoadPWMEnas: std_logic;
	signal ReadPWMEnas: std_logic;

--- SPI interface related signals
	signal SPIBitCountSel : std_logic;
	signal SPIBitrateSel : std_logic;
	signal SPIDataSel : std_logic;	
	signal LoadSPIBitCount: std_logic_vector(SPIs -1 downto 0);
	signal LoadSPIBitRate: std_logic_vector(SPIs -1 downto 0);
	signal LoadSPIData: std_logic_vector(SPIs -1 downto 0);
	signal ReadSPIData: std_logic_vector(SPIs -1 downto 0);           
	signal ReadSPIBitCOunt: std_logic_vector(SPIs -1 downto 0);
	signal ReadSPIBitRate: std_logic_vector(SPIs -1 downto 0);
	signal SPIClk: std_logic_vector(SPIs -1 downto 0);
	signal SPIIn: std_logic_vector(SPIs -1 downto 0);
	signal SPIOut: std_logic_vector(SPIs -1 downto 0);
	signal SPIFrame: std_logic_vector(SPIs -1 downto 0);
	signal SPIDAV: std_logic_vector(SPIs -1 downto 0);	

--- BSPI interface related signals
	signal BSPIDataSel : std_logic;	
	signal BSPIFIFOCountSel : std_logic;
	signal BSPIDescriptorSel : std_logic;
	signal LoadBSPIData: std_logic_vector(BSPIs -1 downto 0);
	signal ReadBSPIData: std_logic_vector(BSPIs -1 downto 0);     
	signal LoadBSPIDescriptor: std_logic_vector(BSPIs -1 downto 0);
	signal ReadBSPIFIFOCOunt: std_logic_vector(BSPIs -1 downto 0);
	signal ClearBSPIFIFO: std_logic_vector(BSPIs -1 downto 0);
	signal BSPIClk: std_logic_vector(BSPIs -1 downto 0);
	signal BSPIIn: std_logic_vector(BSPIs -1 downto 0);
	signal BSPIOut: std_logic_vector(BSPIs -1 downto 0);
	signal BSPIFrame: std_logic_vector(BSPIs -1 downto 0);
	type BSPICSType is array(BSPIs-1 downto 0) of std_logic_vector(BSPICSWidth-1 downto 0);
	signal BSPICS : BSPICSType;

--- UARTX interface related signals		
	signal UARTTDataSel : std_logic;
	signal UARTTBitrateSel : std_logic;
	signal UARTTFIFOCountSel : std_logic;
	signal UARTTModeRegSel : std_logic; 

	signal LoadUARTTData: std_logic_vector(UARTs -1 downto 0);
	signal LoadUARTTBitRate: std_logic_vector(UARTs -1 downto 0);
	signal LoadUARTTModeReg: std_logic_vector(UARTs -1 downto 0);	
	signal CLearUARTTFIFO: std_logic_vector(UARTs -1 downto 0);
	signal ReadUARTTFIFOCount: std_logic_vector(UARTs -1 downto 0);
	signal ReadUARTTBitrate: std_logic_vector(UARTs -1 downto 0);
	signal ReadUARTTModeReg: std_logic_vector(UARTs -1 downto 0);
	signal UARTTFIFOEmpty: std_logic_vector(UARTs -1 downto 0);
	signal UTDrvEn: std_logic_vector(UARTs -1 downto 0);
	signal UTData: std_logic_vector(UARTs -1 downto 0);

--- UARTR interface related signals	
	signal UARTRDataSel : std_logic;
	signal UARTRBitrateSel : std_logic;
	signal UARTRFIFOCountSel : std_logic;
	signal UARTRModeRegSel : std_logic;
	
	signal LoadUARTRData: std_logic_vector(UARTs -1 downto 0);
	signal LoadUARTRBitRate: std_logic_vector(UARTs -1 downto 0);
	signal ReadUARTRBitrate: std_logic_vector(UARTs -1 downto 0);
	signal ClearUARTRFIFO: std_logic_vector(UARTs -1 downto 0);
	signal ReadUARTRFIFOCount: std_logic_vector(UARTs -1 downto 0);
	signal ReadUARTRModeReg: std_logic_vector(UARTs -1 downto 0);
	signal LoadUARTRModeReg: std_logic_vector(UARTs -1 downto 0);
	signal UARTRFIFOHasData: std_logic_vector(UARTs -1 downto 0);
	signal URData: std_logic_vector(UARTs -1 downto 0);			

--- Watchdog related signals 
	signal LoadWDTime : std_logic; 
	signal ReadWDTime : std_logic;
	signal LoadWDStatus : std_logic;
	signal ReadWDStatus : std_logic;
	signal WDCookie: std_logic;
	signal WDBite : std_logic;
	signal WDLatchedBite : std_logic;

--- IRQ related signals 
	signal LoadIRQDiv : std_logic;
	signal ReadIRQDiv : std_logic;
	signal LoadIRQStatus : std_logic;
	signal ReadIrqStatus : std_logic;
	signal ClearIRQ : std_logic;

--- ID related signals
	signal ReadID : std_logic;

--- LED related signals
	signal LoadLEDS : std_logic;
	


	function OneOfNdecode(width : integer;ena1 : std_logic;ena2 : std_logic; dec : std_logic_vector) return std_logic_vector is
	variable result   : std_logic_vector(width-1 downto 0);
	begin
		if ena1 = '1' and ena2 = '1' then
			for i in 0 to width -1 loop
				if CONV_INTEGER(dec) = i then
					result(i) := '1';
				else
					result(i) := '0';
				end if;	
			end loop;		
		else
			result := (others => '0');
		end if;
		return result;
	end OneOfNDecode;			
	
	function bitreverse(v: in std_logic_vector) -- Thanks: J. Bromley
	return std_logic_vector is
	variable result: std_logic_vector(v'RANGE);
	alias tv: std_logic_vector(v'REVERSE_RANGE) is v;
	begin
		for i in tv'RANGE loop
			result(i) := tv(i);
		end loop;
		return result;
	end;
	
	begin


	ahosmotid : entity hostmotid
		generic map ( 
			buswidth => BusWidth,
			cookie => Cookie,
			namelow => HostMotNameLow ,
			namehigh => HostMotNameHigh,
			idromoffset => IDROMOffset
			)			
		port map ( 
			readid => ReadID,
			addr => A(3 downto 2),
			obus => obus
			);


	makeoports: for i in 0 to IOPorts -1 generate
		oportx: entity WordPR 
		generic map (
			size => PortWidth,
			buswidth => BusWidth
			)		
		port map (
			clear => WDBite,
			clk => clklow,
			ibus => ibus,
			obus => obus,
			loadport => LoadPortCmd(i),
			loadddr => LoadDDRCmd(i),
			loadaltdatasrc => LoadAltDataSrcCmd(i),
			loadopendrainmode => LoadOpenDrainModeCmd(i),
			loadinvert => LoadOutputInvCmd(i),
			readddr => ReadDDRCmd(i),
			portdata => IOBits((((i+1)*PortWidth) -1) downto (i*PortWidth)), 
			altdata => Altdata((((i+1)*PortWidth) -1) downto (i*PortWidth))
			);	
	end generate;

	makeiports: for i in 0 to IOPorts -1 generate
		iportx: entity WordRB 		  
		generic map (size => PortWidth,
						 buswidth => BusWidth)
		port map (
		obus => obus,
		readport => ReadPortCmd(i),
		portdata => IOBits((((i+1)*PortWidth) -1) downto (i*PortWidth))
 		);	
	end generate;

	makewatchdog: if UseWatchDog generate  
		wdogabittus: entity watchdog
		generic map ( buswidth => BusWidth)
		
		port map (
			clk => clklow,
			ibus => ibus,
			obus => obus,
			loadtime => LoadWDTime, 
			readtime => ReadWDTime,
			loadstatus=> LoadWDStatus,
			readstatus=> ReadWDStatus,
			cookie => WDCookie,
			wdbite => WDBite,
			wdlatchedbite => WDLatchedBite
			);
		end generate;

	makeirqlogic: if UseIRQlogic generate
		somoldirqlogic: entity irqlogic    
		generic map( 
			buswidth =>  BusWidth,
			dividerwidth => 16
				)	
		port map ( 
			clk => clklow,
			ibus => ibus,
         obus =>  obus,
         loaddiv => LoadIRQDiv,
         readdiv => ReadIRQDiv,
         loadstatus => LoadIRQStatus,
         readstatus => ReadIrqStatus,
         clear =>  ClearIRQ,
         ratesource => RefCountBus(PWMRefWidth-1 downto PWMRefWidth-8), -- from toggle bit all the way to 8X PWM rate
         int => INT);
	end generate;
	
	makestepgens: if StepGens >0 generate
	
		makeStepGenPreScaler:  if UseStepGenPreScaler generate
			StepRategen : entity RateGen port map(
				ibus => ibus,
				obus => obus,
				loadbasicrate => LoadStepGenBasicRate,
				readbasicrate => ReadStepGenBasicRate,
				hold => '0',
				basicrate => StepGenBasicRate,
				clk => clklow);
			end generate;

		generatestepgens: for i in 0 to StepGens-1 generate
			usg: if UseStepGenPreScaler generate
			stepgenx: entity stepgen
			generic map (
				buswidth => BusWidth,
				timersize => 14,			-- = ~480 usec at 33 MHz, ~320 at 50 Mhz 
				tablewidth => StepGenTableWidth,
				asize => 48,
				rsize => 32 
				)
			port map (
				clk => clklow,
				ibus => ibus,
				obus 	=>	 obus,
				loadsteprate => LoadStepGenRate(i),
				loadaccum => LoadStepGenAccum(i),
				loadstepmode => LoadStepGenMode(i),
				loaddirsetuptime => LoadStepGenDSUTime(i),
				loaddirholdtime => LoadStepGenDHLDTime(i),
				loadpulseactivetime => LoadStepGenPulseATime(i),
				loadpulseidletime => LoadStepGenPulseITime(i),
				loadtable => LoadStepGenTable(i),
				loadtablemax => LoadStepGenTableMax(i),
				readsteprate => ReadStepGenRate(i),
				readaccum => ReadStepGenAccum(i),
				readstepmode => ReadStepGenMode(i),
				readdirsetuptime => ReadStepGenDSUTime(i),
				readdirholdtime => ReadStepGenDHLDTime(i),
				readpulseactivetime => ReadStepGenPulseATime(i),
				readpulseidletime => ReadStepGenPulseITime(i),
				readtable => ReadStepGenTable(i),
				readtablemax => ReadStepGenTableMax(i),
				basicrate => StepGenBasicRate,
				hold => '0',
				stout => StepGenOut(i)
				);
			end generate usg;
		
			nusg: if not UseStepGenPreScaler generate
			stepgenx: entity stepgen
			generic map (	
				buswidth => BusWidth,
				timersize => 14,			-- = ~480 usec at 33 MHz, ~320 at 50 Mhz 
				tablewidth => StepGenTableWidth,
				asize => 48,
				rsize => 32 			
				)
			port map (
				clk => clklow,
				ibus => ibus,
				obus 	=>	 obus,
				loadsteprate => LoadStepGenRate(i),
				loadaccum => LoadStepGenAccum(i),
				loadstepmode => LoadStepGenMode(i),
				loaddirsetuptime => LoadStepGenDSUTime(i),
				loaddirholdtime => LoadStepGenDHLDTime(i),
				loadpulseactivetime => LoadStepGenPulseATime(i),
				loadpulseidletime => LoadStepGenPulseITime(i),
				loadtable => LoadStepGenTable(i),
				loadtablemax => LoadStepGenTableMax(i),
				readsteprate => ReadStepGenRate(i),
				readaccum => ReadStepGenAccum(i),
				readstepmode => ReadStepGenMode(i),
				readdirsetuptime => ReadStepGenDSUTime(i),
				readdirholdtime => ReadStepGenDHLDTime(i),
				readpulseactivetime => ReadStepGenPulseATime(i),
				readpulseidletime => ReadStepGenPulseITime(i),
				readtable => ReadStepGenTable(i),
				readtablemax => ReadStepGenTableMax(i),
				basicrate => '1',
				hold => '0',
				stout => StepGenOut(i)  -- densely packed starting with I/O bit 0
				);
			end generate nusg;

		end generate generatestepgens;
	end generate makestepgens;

	
	makequadcounters: for i in 0 to QCounters-1 generate
		qcounterx: entity qcounter 
		generic map (
			buswidth => BusWidth
		)
		port map (
			obus => obus,
			ibus => ibus,
			quada => QuadA(i),
			quadb => QuadB(i),
			index => Index(i),
			loadccr => LoadQcounterCCR(i),
			readccr => ReadQcounterCCR(i),
			readcount => ReadQcounter(i),
			countclear => LoadQcounter(i),
			timestamp => TimeStampBus,
			indexmask => IndexMask(i),
			filterrate => QCountFilterRate,
			clk =>	clklow
		);
	end generate makequadcounters;
	
		makemuxedquadcounters: for i in 0 to MuxedQCounters-1 generate
		qcounterx: entity qcounter 
		generic map (
			buswidth => BusWidth
		)
		port map (
			obus => obus,
			ibus => ibus,
			quada => DemuxedQuadA(i),
			quadb => DemuxedQuadB(i),
			index => DemuxedIndex(i),
			loadccr => LoadMuxedQcounterCCR(i),
			readccr => ReadMuxedQcounterCCR(i),
			readcount => ReadMuxedQcounter(i),
			countclear => LoadMuxedQcounter(i),
			timestamp => MuxedTimeStampBus,
			indexmask => DeMuxedIndexMask(i),
			filterrate => MuxedQCountFilterRate,
			clk =>	clklow
		);
	end generate makemuxedquadcounters;

	makeqcounterglobals:  if (QCounters >0) generate
		timestampx: entity timestamp 
			port map( 
				ibus => ibus(15 downto 0),
				obus => obus(15 downto 0),
				loadtsdiv => LoadTSDiv ,
				readts => ReadTS,
				readtsdiv =>ReadTSDiv,
				tscount => TimeStampBus,
				clk => clklow
			);
				
			qcountratex: entity qcounterate 
			generic map (defaultrate => x"800") -- default is clklow divided by 1 for normal counters
			port map( 
				ibus => ibus(11 downto 0),
				loadRate => LoadQCountRate,
				rateout => QcountFilterRate,
				clk => clklow
			);
	end generate;
	
	makemuxedqcounterglobals:  if (MuxedQCounters >0) generate
		timestampx: entity timestamp 
			port map( 
				ibus => ibus(15 downto 0),
				obus => obus(15 downto 0),
				loadtsdiv => LoadTSDiv ,
				readts => ReadMuxedTS,
				readtsdiv =>ReadMuxedTSDiv,
				tscount => MuxedTimeStampBus,
				clk => clklow
			);
				
			qcountratex: entity qcounterate 
			generic map (defaultrate => x"002") -- default is clklow divided by 4 (N+2) for muxed counters
			port map( 									
				ibus => ibus(11 downto 0),
				loadRate => LoadMuxedQCountRate,
				rateout => MuxedQcountFilterRate,
				clk => clklow
			);
			
	end generate;
	
		
		
	makepwmref:  if ((PWMGens > 0) or UseIRQLogic) generate
		pwmref : entity pwmrefh
		generic map ( 
			buswidth => 16,
			refwidth => PWMRefWidth			-- Normally 13	for 12,11,10, and 9 bit PWM resolutions = 25KHz,50KHz,100KHz,200KHz max. Freq
			)
		port map (
			clk => clklow,
			hclk => clkhigh,
			refcount	=> RefCountBus,
			ibus => ibus(15 downto 0),
			pdmrate => PDMRate,
			pwmrateload => LoadPWMRate,
			pdmrateload => LoadPDMRate
			);	
	end generate;
	
	makepwmgens : for i in 0 to PWMGens-1 generate
		pwmgenx: entity pwmpdmgenh
		generic map ( 
			buswidth => BusWidth,
			refwidth => PWMRefWidth			-- Normally 13 for 12,11,10, and 9 bit PWM resolutions = 25KHz,50KHz,100KHz,200KHz max. Freq
			)
		port map (
			clk => clklow,
			hclk => clkhigh,
			refcount	=> RefCountBus,
			ibus => ibus,
			loadpwmval => LoadPWMVal(i),
			pcrloadcmd => LoadPWMCR(i),
			pdmrate => PDMRate,
			pwmouta => PWMGenOutA(i),
			pwmoutb => PWMGenOutB(i)
		 	);
	end generate;

	makePWMEna:  if (PWMGens >0) generate
		PWMEnaReg : entity boutreg 
			generic map (
				size => PWMGens,
				buswidth => BusWidth,
				invert => false
				)
			port map (
				clk  => clklow,
				ibus => ibus,
				obus => obus,
				load => LoadPWMEnas,
				read => ReadPWMEnas,
				clear => '0',
				dout => PWMGenOutC
		); 
		 		
	end generate;
	
	makeSPIs: for i in 0 to SPIs -1 generate
		aspi: entity SimpleSPI
		generic map (
			buswidth => BusWidth)		
		port map (
			clk  => clklow,
			ibus => ibus,
			obus => obus,
			loadbitcount => LoadSPIBitCount(i),
			loadbitrate => LoadSPIBitRate(i),
			loaddata => LoadSPIData(i),
			readdata => ReadSPIData(i),           
			readbitcount => ReadSPIBitCOunt(i),
			readbitrate => ReadSPIBitRate(i),
			spiclk => SPIClk(i),
			spiin => SPIIn(i),
			spiout => SPIOut(i),
			spiframe => SPIFrame(i),
			davout => SPIDAV(i)
			);
	end generate;	
		
	makeBSPIs: for i in 0 to BSPIs -1 generate
		bspi: entity BufferedSPI
		generic map (
			cswidth => BSPICSWidth)		
		port map (
			clk  => clklow,
			ibus => ibus,
			obus => obus,
			addr => A(5 downto 2),
			hostpush => LoadBSPIData(i),
			hostpop => ReadBSPIData(i),
			loaddesc => LoadBSPIDescriptor(i),
			loadasend => '0',
			clear => ClearBSPIFIFO(i),
			readcount => ReadBSPIFIFOCount(i),
			spiclk => BSPIClk(i),
			spiin => BSPIIn(i),
			spiout => BSPIOut(i),
			spiframe => BSPIFrame(i),
			spicsout => BSPICS(i)
			);
	end generate;	
	

	makeUARTRs: for i in 0 to UARTs -1 generate
		auarrx: entity uartr	
		port map (
			clk => clklow,
			ibus => ibus,
			obus => obus,
			addr => A(3 downto 2),
			popfifo => LoadUARTRData(i),
			loadbitrate => LoadUARTRBitRate(i),
			readbitrate => ReadUARTRBitrate(i),
			clrfifo => ClearUARTRFIFO(i),
			readfifocount => ReadUARTRFIFOCount(i),
			loadmode => LoadUARTRModeReg(i),
			readmode => ReadUARTRModeReg(i),
			fifohasdata => UARTRFIFOHasData(i),
			rxmask => UTDrvEn(i),			-- for half duplex rx mask
			rxdata => URData(i)
         );
	end generate;
	
	makeUARTTXs: for i in 0 to UARTs -1 generate
		auartx:  entity uartx	
		port map (
			clk => clklow,
			ibus => ibus,
			obus => obus,
			addr => A(3 downto 2),
			pushfifo => LoadUARTTData(i),
			loadbitrate => LoadUARTTBitRate(i),
			readbitrate => ReadUARTTBitrate(i),
			clrfifo => ClearUARTTFIFO(i),
			readfifocount => ReadUARTTFIFOCount(i),
			loadmode => LoadUARTTModeReg(i),
			readmode => ReadUARTTModeReg(i),
			fifoempty => UARTTFIFOEmpty(i),
			txen => '1',
			drven => UTDrvEn(i),
			txdata => UTData(i)
         );
	end generate;

	LEDReg : entity boutreg 
	generic map (
		size => LEDCount,
		buswidth => LEDCount,
		invert => true)
	port map (
		clk  => clklow,
		ibus => ibus(BusWidth-1 downto BusWidth-LEDCount),
		obus => obus(BusWidth-1 downto BusWidth-LEDCount),
		load => LoadLEDs,
		read => '0',
		clear => '0',
		dout => LEDS
		); 

		
	IDROMWP : entity boutreg 
 		generic map (
			size => 1,
			buswidth => BusWidth,
			invert => false
			)
		port map (
			clk  => clklow,
         ibus => ibus,
         obus => obus,
         load => LoadIDROMWEn,
         read => ReadIDROMWEn,
			clear => '0',
         dout => IDROMWen
		); 
		 		

	IDROM : entity IDROM
		generic map (
			idromtype => IDROMType,
			offsettomodules => OffsetToModules,
			offsettopindesc => OffsetToPinDesc,
			boardnamelow => BoardNameLow,
			boardnameHigh => BoardNameHigh,
			fpgasize => FPGASize,
			fpgapins => FPGAPins,
			ioports => IOPorts,
			iowidth => IOWidth,
			portwidth => PortWidth,		
			clocklow => ClockLow,
			clockhigh => ClockHigh,
			inststride0 => InstStride0,
			inststride1 => InstStride1,
			regstride0 => RegStride0,
			regstride1 => RegStride1,
			pindesc => ThePinDesc,
			moduleid => TheModuleID)
		port map (
			clk  => clklow, 
			we   => LoadIDROM,
			re   => ReadIDROM,
			radd => addr(9 downto 2),
			wadd => A(9 downto 2),
			din  => ibus, 
			dout => obus
		); 

		DoPinout: process(PWMGenOutA,PWMGenOutB,PWMGenOutC,StepGenOut,SPIFrame,SPIOut,SPIClk,
		                  UTData,UTDrvEn,BSPIFrame,BSPIOut,BSPIClk,BSPICS,IOBits)
		begin
			Altdata <= (others => '0');
			for i in 0 to IOWidth -1 loop 
				case ThePinDesc(i)(15 downto 8) is -- GTag
					-- all these nasty subranges will go away when pindescs are changed to records
					when QCountTag => 
						case (ThePinDesc(i)(7 downto 0)) is	--secondary pin function
							when QCountQAPin =>
								QuadA(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i); 
							when QCountQBPin =>
								QuadB(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when QCountIdxPin =>
								Index(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when QCountIdxMaskPin =>
								IndexMask(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when others => null;
						end case;

					when MuxedQCountTag => 
						case (ThePinDesc(i)(7 downto 0)) is	--secondary pin function
							when MuxedQCountQAPin =>
								MuxedQuadA(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i); 
							when MuxedQCountQBPin =>
								MuxedQuadB(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when MuxedQCountIdxPin =>
								MuxedIndex(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when MuxedQCountIdxMaskPin =>
								MuxedIndexMask(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when others => null;
						end case;

					when MuxedQCountSelTag =>
						case(ThePinDesc(i)(7 downto 0)) is	--secondary pin function
							when MuxedQCountSel0Pin =>
								AltData(i) <= MuxedQCtrSel(0);
							when MuxedQCountSel1Pin =>
								AltData(i) <= MuxedQCtrSel(1);
							when others => null;
						end case;
					
					when PWMTag =>
						case (ThePinDesc(i)(7 downto 0)) is	--secondary pin function
							when PWMAOutPin =>
								AltData(i) <= PWMGENOutA(conv_integer(ThePinDesc(i)(23 downto 16))); 
							when PWMBDirPin =>
								AltData(i) <= PWMGENOutB(conv_integer(ThePinDesc(i)(23 downto 16))); 
							when PWMCEnaPin =>
								AltData(i) <= PWMGENOutC(conv_integer(ThePinDesc(i)(23 downto 16))); 
							when others => null;
						end case;
						
					when StepGenTag =>						
						AltData(i) <= StepGenOut(conv_integer(ThePinDesc(i)(23 downto 16)))(conv_integer(ThePinDesc(i)(6 downto 0))-1);						

					when UARTTTag =>
						case (ThePinDesc(i)(7 downto 0)) is	--secondary pin function
							when UTDataPin =>
								AltData(i) <= UTData(conv_integer(ThePinDesc(i)(23 downto 16)));				
							when UTDrvEnPin =>
								AltData(i) <= UTDrvEn(conv_integer(ThePinDesc(i)(23 downto 16)));	
							when others => null;								
						end case;
						
					when UARTRTag =>
						if (ThePinDesc(i)(7 downto 0)) = URDataPin then
							URData(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
						end if;
						
					when SPITag =>
						case (ThePinDesc(i)(7 downto 0)) is	--secondary pin function, drop MSB
							when SPIFramePin =>
								AltData(i) <= SPIFrame(conv_integer(ThePinDesc(i)(23 downto 16)));				
							when SPIOutPin =>
								AltData(i) <= SPIOut(conv_integer(ThePinDesc(i)(23 downto 16)));				
							when SPIClkPin =>
								AltData(i) <= SPIClk(conv_integer(ThePinDesc(i)(23 downto 16)));				
							when SPIInPin =>		
								SPIIn(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when others => null;
						end case;
						
					when BSPITag =>
						case (ThePinDesc(i)(7 downto 0)) is	--secondary pin function, drop MSB
							when BSPIFramePin =>
								AltData(i) <= BSPIFrame(conv_integer(ThePinDesc(i)(23 downto 16)));				
							when BSPIOutPin =>
								AltData(i) <= BSPIOut(conv_integer(ThePinDesc(i)(23 downto 16)));				
							when BSPIClkPin =>
								AltData(i) <= BSPIClk(conv_integer(ThePinDesc(i)(23 downto 16)));				
							when BSPIInPin =>		
								BSPIIn(conv_integer(ThePinDesc(i)(23 downto 16))) <= IOBits(i);
							when others => AltData(i) <= BSPICS(conv_integer(ThePinDesc(i)(23 downto 16)))(conv_integer(ThePinDesc(i)(6 downto 0))-5);
							-- magic foo, magic foo, what on earth does it do?						
							-- (this needs to written more clearly!)							
						end case;

--					when SSITag =>  not done
					
					when others => null;		
				end case;	
			end loop;		
		end process;	
		
		

   LooseEnds: process(A,clklow)
	begin
		if rising_edge(clklow) then
			A <= addr;
		end if;
	end process;

   MuxedEnc: if  MuxedQCounters > 0 generate
	
		EncoderDeMux: process(clklow)
		begin
			if rising_edge(clklow) then
				if MuxedQCountFilterRate = '1' then
					PreMuxedQCtrSel <= PreMuxedQCtrSel + 1;
				end if;
				MuxedQCtrSel <= PreMuxedQCtrSel;
				for i in 0 to ((MuxedQCounters/2) -1) loop -- just 2 deep for now
					if PreMuxedQCtrSel(0) = '1' and MuxedQCtrSel(0) = '0' then	-- latch the even inputs	
						DeMuxedQuadA(2*i) <= MuxedQuadA(i);
						DeMuxedQuadB(2*i) <= MuxedQuadB(i);
						DeMuxedIndex(2*i) <= MuxedIndex(i);
						DeMuxedIndexMask(2*i) <= MuxedIndexMask(i);
					end if;
					if PreMuxedQCtrSel(0) = '0' and MuxedQCtrSel(0) = '1' then	-- latch the odd inputs
						DeMuxedQuadA(2*i+1) <= MuxedQuadA(i);
						DeMuxedQuadB(2*i+1) <= MuxedQuadB(i);
						DeMuxedIndex(2*i+1) <= MuxedIndex(i);
						DeMuxedIndexMask(2*i+1) <= MuxedIndexMask(i);
					end if;
				end loop;
			end if; -- clk
		end process;
	end generate;		
		
	

	Decode: process(A,write, IDROMWEn, read) 
	begin	
		-- basic multi decodes are at 256 byte increments (64 longs)
		-- first decode is 256 x 32 ID ROM


		if (A(15 downto 10) = IDROMAddr(7 downto 2)) and Write = '1' and IDROMWEn = "1" then	 -- 400 Hex  
			LoadIDROM <= '1';
		else
			LoadIDROM <= '0';
		end if;
		if (A(15 downto 10) = IDROMAddr(7 downto 2)) and Read = '1' then	 --  
			ReadIDROM <= '1';
		else
			ReadIDROM <= '0';
		end if;

		if A(15 downto 8) = PortAddr then  -- basic I/O port select
			PortSel <= '1';
		else
			PortSel <= '0';
		end if;

		if A(15 downto 8) = DDRAddr then	 -- DDR register select
			DDRSel <= '1';
		else
			DDRSel <= '0';
		end if;

		if A(15 downto 8) = AltDataSrcAddr then  -- Alt data source register select
			AltDataSrcSel <= '1';
		else
			AltDataSrcSel <= '0';
		end if;

		if A(15 downto 8) = OpenDrainModeAddr then	 --  OpenDrain  register select
			OpendrainModeSel <= '1';
		else
			OpenDrainModeSel <= '0';
		end if;

		if A(15 downto 8) = OutputInvAddr then	 --  IO invert register select
			OutputInvSel <= '1';
		else
			OutputInvSel <= '0';
		end if;

		if A(15 downto 8) = StepGenRateAddr then	 --  stepgen rate register select
			StepGenRateSel <= '1';
		else
			StepGenRateSel <= '0';
		end if;

		if A(15 downto 8) = StepGenAccumAddr then	 --  stepgen Accumumlator low select
			StepGenAccumSel <= '1';
		else
			StepGenAccumSel <= '0';
		end if;

		if A(15 downto 8) = StepGenModeAddr then	 --  stepgen mode register select
			StepGenModeSel <= '1';
		else
			StepGenModeSel <= '0';
		end if;

		if A(15 downto 8) = StepGenDSUTimeAddr then	 --  stepgen Dir setup time register select
			StepGenDSUTimeSel <= '1';
		else
			StepGenDSUTimeSel <= '0';
		end if;

		if A(15 downto 8) =StepGenDHLDTimeAddr then	 --  stepgen Dir hold time register select
			StepGenDHLDTimeSel <= '1';
		else
			StepGenDHLDTimeSel <= '0';
		end if;

		if A(15 downto 8) = StepGenPulseATimeAddr then	 --  stepgen pulse width register select
			StepGenPulseATimeSel <= '1';
		else
			StepGenPulseATimeSel <= '0';
		end if;

		if A(15 downto 8) = StepGenPulseITimeAddr then	 --  stepgen pulse width register select
			StepGenPulseITimeSel <= '1';
		else
			StepGenPulseITimeSel <= '0';
		end if;

		if A(15 downto 8) = StepGenTableAddr then	 --  stepgen pulse width register select
			StepGenTableSel <= '1';
		else
			StepGenTableSel <= '0';
		end if;
	
		if A(15 downto 8) = StepGenTableMaxAddr then	 --  stepgen pulse width register select
			StepGenTableMaxSel <= '1';
		else
			StepGenTableMaxSel <= '0';
		end if;

		if A(15 downto 8) = QCounterAddr then	 --  QCounter select
			QCounterSel <= '1';
		else
			QCounterSel <= '0';
		end if;

		if A(15 downto 8) = QCounterCCRAddr then	 --  QCounter CCR register select
			QCounterCCRSel <= '1';
		else
			QCounterCCRSel <= '0';
		end if;

		if A(15 downto 8) = MuxedQCounterAddr then	 --  QCounter select
			MuxedQCounterSel <= '1';
		else
			MuxedQCounterSel <= '0';
		end if;

		if A(15 downto 8) = QCounterCCRAddr then	 --  QCounter CCR register select
			MuxedQCounterCCRSel <= '1';
		else
			MuxedQCounterCCRSel <= '0';
		end if;

		if A(15 downto 8) = PWMValAddr then	 --  PWMVal select
			PWMValSel <= '1';
		else
			PWMValSel <= '0';
		end if;

		if A(15 downto 8) = PWMCRAddr then	 --  PWM mode register select
			PWMCRSel <= '1';
		else
			PWMCRSel <= '0';
		end if;

		if A(15 downto 8) = SPIDataAddr then	 --  SPI data register select
			SPIDataSel <= '1';
		else
			SPIDataSel <= '0';
		end if;
		
		if A(15 downto 8) = SPIBitCountAddr then	 --  SPI bit count register select
			SPIBitCountSel <= '1';
		else
			SPIBitCountSel <= '0';
		end if;

		if A(15 downto 8) = SPIBitrateAddr then	 --  SPI bit rate register select
			SPIBitrateSel <= '1';
		else
			SPIBitrateSel <= '0';
		end if;
		
		if A(15 downto 8) = BSPIDataAddr then	 --  BSPI data register select
			BSPIDataSel <= '1';
		else
			BSPIDataSel <= '0';
		end if;
		
		if A(15 downto 8) = BSPIFIFOCountAddr then	 --  BSPI FIFO count register select
			BSPIFIFOCountSel <= '1';
		else
			BSPIFIFOCountSel <= '0';
		end if;

		if A(15 downto 8) = BSPIDescriptorAddr then	 --  BSPI channel descriptor register select
			BSPIDescriptorSel <= '1';
		else
			BSPIDescriptorSel <= '0';
		end if;
		
		if A(15 downto 8) = UARTTDataAddr then	 --  UART TX data register select
			UARTTDataSel <= '1';
		else
			UARTTDataSel <= '0';
		end if;

		if A(15 downto 8) = UARTTFIFOCountAddr then	 --  UART TX FIFO count register select
			UARTTFIFOCountSel <= '1';
		else
			UARTTFIFOCountSel <= '0';
		end if;

		if A(15 downto 8) = UARTTBitrateAddr then	 --  UART TX bit rate register select
			UARTTBitrateSel <= '1';
		else
			UARTTBitrateSel <= '0';
		end if;

		if A(15 downto 8) = UARTTModeRegAddr then	 --  UART TX bit mode register select
			UARTTModeRegSel <= '1';
		else
			UARTTModeRegSel <= '0';
		end if;


		if A(15 downto 8) = UARTRDataAddr then	 --  UART RX data register select
			UARTRDataSel <= '1';
		else
			UARTRDataSel <= '0';
		end if;

		if A(15 downto 8) = UARTRFIFOCountAddr then	 --  UART RX FIFO count register select
			UARTRFIFOCountSel <= '1';
		else
			UARTRFIFOCountSel <= '0';
		end if;

		if A(15 downto 8) = UARTRBitrateAddr then	 --  UART RX bit rate register select
			UARTRBitrateSel <= '1';
		else
			UARTRBitrateSel <= '0';
		end if;

		if A(15 downto 8) = UARTRModeRegAddr then	 --  UART RX status register select
			UARTRModeRegSel <= '1';
		else
			UARTRModeRegSel <= '0';
		end if;

		if A(15 downto 8) = ReadIDAddr and Read = '1' then	 --  
			ReadID <= '1';
		else
			ReadID <= '0';
		end if;

		if A(15 downto 8) = WatchdogTimeAddr and Read = '1' then	 --  
			ReadWDTime <= '1';
		else
			ReadWDTime <= '0';
		end if;
		if A(15 downto 8) = WatchdogTimeAddr and Write = '1' then	 --  
			LoadWDTime <= '1';
		else
			LoadWDTime <= '0';
		end if;

		if A(15 downto 8) = WatchdogStatusAddr and Read = '1' then	 --  
			ReadWDStatus <= '1';
		else
			ReadWDStatus <= '0';
		end if;
		if A(15 downto 8) = WatchdogStatusAddr and Write = '1' then	 --  
			LoadWDStatus <= '1';
		else
			LoadWDStatus <= '0';
		end if;

		if A(15 downto 8) = WatchdogCookieAddr and Write = '1' then	 --  
			WDCookie <= '1';
		else
			WDCookie <= '0';
		end if;


		if A(15 downto 8) = IRQDivAddr and Write = '1' then	 --  
			LoadIRQDiv <= '1';
		else
			LoadIRQDiv <= '0';
		end if;

		if A(15 downto 8) = IRQDivAddr and Read = '1' then	 --  
			ReadIRQDiv <= '1';
		else
			ReadIRQDiv <= '0';
		end if;

		if A(15 downto 8) = IRQStatusAddr and Write = '1' then	 --  
			LoadIRQStatus <= '1';
		else
			LoadIRQStatus <= '0';
		end if;

		if A(15 downto 8) = IRQStatusAddr and Read = '1' then	 --  
			ReadIrqStatus <= '1';
		else
			ReadIrqStatus <= '0';
		end if;

		if A(15 downto 8) = ClearIRQAddr and Write = '1' then	 --  
			ClearIRQ <= '1';
		else
			ClearIRQ <= '0';
		end if;
 		
		if A(15 downto 8) = StepGenBasicRateAddr and Write = '1' then	 --  
			LoadStepGenBasicRate <= '1';
		else
			LoadStepGenBasicRate <= '0';
		end if;
		if A(15 downto 8) = StepGenBasicRateAddr and Read = '1' then	 --  
			ReadStepGenBasicRate <= '1';
		else
			ReadStepGenBasicRate <= '0';
		end if;

		if A(15 downto 8) = TSDivAddr and Write = '1' then	 --  
			LoadTSDiv <= '1';
		else
			LoadTSDiv <= '0';
		end if;
		if A(15 downto 8) = TSDivAddr and Read = '1' then	 --  
			ReadTSDiv <= '1';
		else
			ReadTSDiv <= '0';
		end if;

		if A(15 downto 8) = TSAddr and Read = '1' then	 --  
			ReadTS <= '1';
		else
			ReadTS <= '0';
		end if;

		if A(15 downto 8) = QCRateAddr and Write = '1' then	 --  
			LoadQCountRate <= '1';
		else
			LoadQCountRate <= '0';
		end if;
		
		if A(15 downto 8) = MuxedTSDivAddr and Write = '1' then	 --  
			LoadMuxedTSDiv <= '1';
		else
			LoadMuxedTSDiv <= '0';
		end if;
		if A(15 downto 8) = MuxedTSDivAddr and Read = '1' then	 --  
			ReadMuxedTSDiv <= '1';
		else
			ReadMuxedTSDiv <= '0';
		end if;

		if A(15 downto 8) = MuxedTSAddr and Read = '1' then	 --  
			ReadMuxedTS <= '1';
		else
			ReadMuxedTS <= '0';
		end if;

		if A(15 downto 8) = MuxedQCRateAddr and Write = '1' then	 --  
			LoadMuxedQCountRate <= '1';
		else
			LoadMuxedQCountRate <= '0';
		end if;


		if A(15 downto 8) = PWMRateAddr and Write = '1' then	 --  
			LoadPWMRate <= '1';
		else
			LoadPWMRate <= '0';
		end if;

		if A(15 downto 8) = PDMRateAddr and Write = '1' then	 --  
			LoadPDMRate <= '1';
		else
			LoadPDMRate <= '0';
		end if;

		if A(15 downto 8) = PWMEnasAddr and Write = '1' then	 --  
			LoadPWMEnas <= '1';
		else
			LoadPWMEnas <= '0';
		end if;

		if A(15 downto 8) = PWMEnasAddr and Read = '1' then	 --  
			ReadPWMEnas <= '1';
		else
			ReadPWMEnas <= '0';
		end if;


		if A(15 downto 8) = IDROMWEnAddr and Write = '1' then	 --  
			LoadIDROMWEn <= '1';
		else
			LoadIDROMWEn <= '0';
		end if;
		if A(15 downto 8) = IDROMWEnAddr and Read = '1' then	 --  
			ReadIDROMWEn <= '1';
		else
			ReadIDROMWEn <= '0';
		end if;

		if A(15 downto 8) = LEDAddr and Write = '1' then	 --  
			LoadLEDs <= '1';
		else
			LoadLEDs <= '0';
		end if;

	end process;
	
	PortDecode: process (A,Read,Write,PortSel, DDRSel, AltDataSrcSel, OpenDrainModeSel, OutputInvSel)
	begin

		LoadPortCMD <= OneOfNDecode(IOPorts,PortSel,Write,A(3 downto 2)); -- 4 max
		ReadPortCMD <= OneOfNDecode(IOPorts,PortSel,Read,A(3 downto 2));
		LoadDDRCMD <= OneOfNDecode(IOPorts,DDRSel,Write,A(3 downto 2));
		ReadDDRCMD <= OneOfNDecode(IOPorts,DDRSel,Read,A(3 downto 2));

		LoadAltDataSrcCMD <= OneOfNDecode(IOPorts,AltDataSrcSel,Write,A(3 downto 2));
		LoadOpenDrainModeCMD <= OneOfNDecode(IOPorts,OpenDrainModeSel,Write,A(3 downto 2));
		LoadOutputInvCMD <= OneOfNDecode(IOPorts,OutputInvSel,Write,A(3 downto 2));

	end process PortDecode;

		StepGenDecode: if (STEPGENs > 0) generate
			StepGenDecodeProcess : process (A,Read,write,StepGenRateSel, StepGenAccumSel, StepGenModeSel,
                                 			StepGenDSUTimeSel, StepGenDHLDTimeSel, StepGenPulseATimeSel, 
			                                 StepGenPulseITimeSel, StepGenTableSel, StepGenTableMaxSel)
			begin
				LoadStepGenRate <= OneOfNDecode(STEPGENs,StepGenRateSel,Write,A(6 downto 2)); -- 32 max
				ReadStepGenRate <= OneOfNDecode(STEPGENs,StepGenRateSel,Read,A(6 downto 2));
				LoadStepGenAccum <= OneOfNDecode(STEPGENs,StepGenAccumSel,Write,A(6 downto 2));
				ReadStepGenAccum <= OneOfNDecode(STEPGENs,StepGenAccumSel,Read,A(6 downto 2));
				LoadStepGenMode <= OneOfNDecode(STEPGENs,StepGenModeSel,Write,A(6 downto 2));			 
				ReadStepGenMode <= OneOfNDecode(STEPGENs,StepGenModeSel,Read,A(6 downto 2));	
				LoadStepGenDSUTime <= OneOfNDecode(STEPGENs,StepGenDSUTimeSel,Write,A(6 downto 2));
				ReadStepGenDSUTime <= OneOfNDecode(STEPGENs,StepGenDSUTimeSel,Read,A(6 downto 2));
				LoadStepGenDHLDTime <= OneOfNDecode(STEPGENs,StepGenDHLDTimeSel,Write,A(6 downto 2));
				ReadStepGenDHLDTime <= OneOfNDecode(STEPGENs,StepGenDHLDTimeSel,Read,A(6 downto 2));
				LoadStepGenPulseATime <= OneOfNDecode(STEPGENs,StepGenPulseATimeSel,Write,A(6 downto 2));
				ReadStepGenPulseATime <= OneOfNDecode(STEPGENs,StepGenPulseATimeSel,Read,A(6 downto 2));
				LoadStepGenPulseITime <= OneOfNDecode(STEPGENs,StepGenPulseITimeSel,Write,A(6 downto 2));
				ReadStepGenPulseITime <= OneOfNDecode(STEPGENs,StepGenPulseITimeSel,Read,A(6 downto 2));
				LoadStepGenTable <= OneOfNDecode(STEPGENs,StepGenTableSel,Write,A(6 downto 2));
				ReadStepGenTable <= OneOfNDecode(STEPGENs,StepGenTableSel,Read,A(6 downto 2));
				LoadStepGenTableMax <= OneOfNDecode(STEPGENs,StepGenTableMaxSel,Write,A(6 downto 2));
				ReadStepGenTableMax <= OneOfNDecode(STEPGENs,StepGenTableMaxSel,Read,A(6 downto 2));
			end process StepGenDecodeProcess;
		end generate;


		QCounterDecode: if (QCounters > 0) generate		
			QCounterDecodeProcess : process (A,Read,write,QCounterSel, QCounterCCRSel)
			begin
				LoadQCounter <= OneOfNDecode(QCounters,QCounterSel,Write,A(6 downto 2));  -- 32 max
				ReadQCounter <= OneOfNDecode(QCounters,QCounterSel,Read,A(6 downto 2));
				LoadQCounterCCR <= OneOfNDecode(QCounters,QCounterCCRSel,Write,A(6 downto 2));
				ReadQCounterCCR <= OneOfNDecode(QCounters,QCounterCCRSel,Read,A(6 downto 2));
			end process QCounterDecodeProcess;
		end generate;

		MuxedQCounterDecode: if (MuxedQcounters > 0) generate		
			MuxedQCounterDecodeProcess : process (A,Read,write,MuxedQCounterSel, MuxedQCounterCCRSel)
			begin
				LoadMuxedQCounter <= OneOfNDecode(MuxedQCounters,MuxedQCounterSel,Write,A(6 downto 2));  -- 32 max
				ReadMuxedQCounter <= OneOfNDecode(MuxedQCounters,MuxedQCounterSel,Read,A(6 downto 2));
				LoadMuxedQCounterCCR <= OneOfNDecode(MuxedQCounters,MuxedQCounterCCRSel,Write,A(6 downto 2));
				ReadMuxedQCounterCCR <= OneOfNDecode(MuxedQCounters,MuxedQCounterCCRSel,Read,A(6 downto 2));
			end process MuxedQCounterDecodeProcess;
		end generate;


		PWMDecode: if (PWMGENs > 0) generate		
			PWMDecodeProcess : process (A,Read,write,PWMValSel, PWMCRSel)
			begin
				LoadPWMVal <= OneOfNDecode(PWMGENs,PWMValSel,Write,A(6 downto 2)); -- 32 max
				LoadPWMCR <= OneOfNDecode(PWMGENs, PWMCRSel,Write,A(6 downto 2));
			end process PWMDecodeProcess;
		end generate;

		SPIDecode: if (SPIs > 0) generate		
			SPIDecodeProcess : process (A,Read,write,SPIDataSel,SPIBitCountSel,SPIBitRateSel)
			begin		
				LoadSPIData <= OneOfNDecode(SPIs,SPIDataSel,Write,A(5 downto 2)); -- 16 max
				ReadSPIData <= OneOfNDecode(SPIs,SPIDataSel,Read,A(5 downto 2));
				LoadSPIBitCount <= OneOfNDecode(SPIs,SPIBitCountSel,Write,A(5 downto 2));
				ReadSPIBitCount <= OneOfNDecode(SPIs,SPIBitCountSel,Read,A(5 downto 2));
				LoadSPIBitRate <= OneOfNDecode(SPIs,SPIBitRateSel,Write,A(5 downto 2));
				ReadSPIBitRate <= OneOfNDecode(SPIs,SPIBitRateSel,Read,A(5 downto 2));
			end process SPIDecodeProcess;
		end generate;

		BSPIDecode: if (BSPIs > 0) generate		
			BSPIDecodeProcess : process (A,Read,write,BSPIDataSel,BSPIFIFOCountSel,BSPIDescriptorSel)
			begin		
				LoadBSPIData <= OneOfNDecode(BSPIs,BSPIDataSel,Write,A(7 downto 6)); -- 4 max
				ReadBSPIData <= OneOfNDecode(BSPIs,BSPIDataSel,Read,A(7 downto 6));
				LoadBSPIDescriptor<= OneOfNDecode(BSPIs,BSPIDescriptorSel,Write,A(5 downto 2));
				ReadBSPIFIFOCOunt <= OneOfNDecode(BSPIs,BSPIFIFOCountSel,Read,A(5 downto 2));
				ClearBSPIFIFO <= OneOfNDecode(BSPIs,BSPIFIFOCountSel,Write,A(5 downto 2));
			end process BSPIDecodeProcess;
		end generate;

		UARTDecode: if (UARTs > 0) generate		
			UARTDecodeProcess : process (A,Read,write,UARTTDataSel,UARTTBitRateSel,UARTTModeRegSel,UARTTFIFOCountSel,
			                             UARTRDataSel,UARTRBitRateSel,UARTRFIFOCountSel,UARTRModeRegSel)
			begin		
				LoadUARTTData <= OneOfNDecode(UARTs,UARTTDataSel,Write,A(6 downto 4));
				LoadUARTTBitRate <= OneOfNDecode(UARTs,UARTTBitRateSel,Write,A(4 downto 2));
				ReadUARTTBitrate <= OneOfNDecode(UARTs,UARTTBitRateSel,Read,A(4 downto 2));
				LoadUARTTModeReg <= OneOfNDecode(UARTs,UARTTModeRegSel,Write,A(4 downto 2));
				ReadUARTTModeReg <= OneOfNDecode(UARTs,UARTTModeRegSel,Read,A(4 downto 2));
				ClearUARTTFIFO <= OneOfNDecode(UARTs,UARTTFIFOCountSel,Write,A(4 downto 2));
				ReadUARTTFIFOCount <= OneOfNDecode(UARTs,UARTTFIFOCountSel,Read,A(4 downto 2));

				LoadUARTRData <= OneOfNDecode(UARTs,UARTRDataSel,Read,A(6 downto 4));
				LoadUARTRBitRate <= OneOfNDecode(UARTs,UARTRBitRateSel,Write,A(4 downto 2));
				ReadUARTRBitrate <= OneOfNDecode(UARTs,UARTRBitRateSel,Read,A(4 downto 2));
				ClearUARTRFIFO <= OneOfNDecode(UARTs,UARTRFIFOCountSel,Write,A(4 downto 2));
				ReadUARTRFIFOCount <= OneOfNDecode(UARTs,UARTRFIFOCountSel,Read,A(4 downto 2));
				LoadUARTRModeReg <= OneOfNDecode(UARTs,UARTRModeRegSel,Write,A(4 downto 2));
				ReadUARTRModeReg <= OneOfNDecode(UARTs,UARTRModeRegSel,Read,A(4 downto 2));
			end process UARTDecodeProcess;
		end generate;

		
	dotieupint: if not UseIRQLogic generate
		tieupint : process(clklow)
		begin
			INT <= '1';
		end process;
	end generate;		

end dataflow;

  