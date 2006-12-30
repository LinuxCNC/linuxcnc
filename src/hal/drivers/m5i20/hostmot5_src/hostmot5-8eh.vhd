library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
library UNISIM;
use UNISIM.VComponents.all;

-- 8 axis version with 24 I/O bits - 100 MHz PWM and basic timing
entity HostMot5_8EH is
  	port 
  (
 	LRD: in STD_LOGIC; 
	LWR: in STD_LOGIC; 
	LW_R: in STD_LOGIC; 
	ALE: in STD_LOGIC; 
	ADS: in STD_LOGIC; 
	BLAST: in STD_LOGIC; 
	WAITO: in STD_LOGIC;
	LOCKO: in STD_LOGIC;
	CS0: in STD_LOGIC;
	CS1: in STD_LOGIC;
	READY: out STD_LOGIC; 
	INT: out STD_LOGIC;
	
   LAD: inout STD_LOGIC_VECTOR (31 downto 0); 		-- data/address bus
 	LA: in STD_LOGIC_VECTOR (8 downto 2); 				-- non-muxed address bus
	lBE: in STD_LOGIC_VECTOR (3 downto 0); 			-- byte enables

			
	SYNCLK: in STD_LOGIC;
	LCLK: in STD_LOGIC;
	-- I/O signals	
	A: in STD_LOGIC_VECTOR (7 downto 0);
	B: in STD_LOGIC_VECTOR (7 downto 0);
	IDX: in STD_LOGIC_VECTOR (7 downto 0);
	PWM: inout STD_LOGIC_VECTOR  (7 downto 0);
	ENA: out STD_LOGIC_VECTOR  (7 downto 0);
	DIR: inout STD_LOGIC_VECTOR  (7 downto 0);

	IOBITSA: inout STD_LOGIC_VECTOR (23 downto 0);
--	IOBITSB: inout STD_LOGIC_VECTOR (23 downto 0);
		

	-- led bits
	LEDS: out STD_LOGIC_VECTOR(7 downto 0)

	);
end HostMot5_8EH; -- for 5I20 or 4I65


architecture dataflow of Hostmot5_8EH is

	alias BLE: STD_LOGIC is LBE(0);	-- 16 bit mode
   alias BHE: STD_LOGIC is LBE(3);	-- 16 bit mode
	alias LA1: STD_LOGIC is LBE(1);	-- 8/16 bit mode
	alias LA0: STD_LOGIC is LBE(0);	-- 8 bit mode
-- misc global signals --
	signal D: STD_LOGIC_VECTOR (31 downto 0);							-- internal data bus
	signal LatchedA: STD_LOGIC_VECTOR (15 downto 0);
	signal LatchedLBE: STD_LOGIC_VECTOR (3 downto 0);
	signal PreFastRead: STD_LOGIC;
	signal FastRead: STD_LOGIC;

-- Version specific constants  --

	constant counters :integer := 8;
	constant HMID : STD_LOGIC_VECTOR (31 downto 0) := x"AA020008";	-- MSW = rev 1, LSW = 8 axis 
	constant MasterClock : STD_LOGIC_VECTOR (31 downto 0) := x"05F5E100"; -- = 100 MHz

-- misc global signals --
	signal CardSelect: STD_LOGIC; 											-- card select decode
	signal LEDView: STD_LOGIC_VECTOR (7 downto 0); 					-- index register
	signal FClk: STD_LOGIC; 												-- high speed clock = 100 MHz
	signal CLKFB: STD_LOGIC;
	signal CLK0: STD_LOGIC;
	signal CLK2X: STD_LOGIC;


--	irq related signals
	signal IRQSource: STD_LOGIC; 
	signal IRQLatch: STD_LOGIC;
	signal IRQMask: STD_LOGIC;
	signal MissedIRQ: STD_LOGIC; 
	signal StopOnMissedIRQ: STD_LOGIC;
	signal ClearMissedIRQ: STD_LOGIC;
	signal LatchOnInterrupt: STD_LOGIC;

--	timeout related signals
	signal ReloadWDCmd: STD_LOGIC;
	signal StopOnTimeout: STD_LOGIC;
	signal WDTimeOut: STD_LOGIC; 

-- LEDView and id reg signals
	signal LoadLEDViewCmd: STD_LOGIC;
	signal ReadLEDViewCmd: STD_LOGIC;
	signal Enasigs :STD_LOGIC_VECTOR (counters-1 downto 0);
	signal IDSel: STD_LOGIC;
	signal ReadIDCmd: STD_LOGIC;
	signal MCSel: STD_LOGIC;
	signal ReadMCCmd: STD_LOGIC;

-- irqdiv reg signals
	signal ReadIRQDivCmd: STD_LOGIC; 
	signal LoadIRQDivCmd: STD_LOGIC; 
	signal ClearIRQCmd: STD_LOGIC;

-- irq sel reg signals

	signal loadGCRCmd: STD_LOGIC;
	signal LoadGMRCmd: STD_LOGIC;
	signal ReadGMRCmd: STD_LOGIC;

-- timeout reg signals

	signal loadTimeoutCmd: STD_LOGIC;
	signal ReadTimeoutCmd: STD_LOGIC;
	signal ReadTimerCmd: STD_LOGIC;


-- phase accumulator signals

	signal ReadPhaseCmd: STD_LOGIC; 
	signal LoadPhaseCmd: STD_LOGIC; 

-- counter signals --
	signal CounterRead: STD_LOGIC_VECTOR  (counters-1 downto 0);		-- read counter
	signal GlobalCounterEnable: STD_LOGIC;										-- enable counting
	signal GlobalCountLatchcmd: STD_LOGIC;					-- command to latch counter value	
	signal GlobalCountLatch: STD_LOGIC;						-- command + irq generated latch count
	signal CountLatchEdge1: STD_LOGIC;
	signal CountLatchEdge2: STD_LOGIC;
	signal CCRLoadCmds: STD_LOGIC_VECTOR (counters-1 downto 0);			-- counter control reg loads
	signal CCRReadCmds:	STD_LOGIC_VECTOR (counters-1 downto 0);		-- counter control reg reads
	signal GlobalCounterClear: STD_LOGIC;										-- clear counter

-- secondary counter signals --
	signal SCounterRead: STD_LOGIC_VECTOR  (counters-1 downto 0);		-- read counter
	signal SCCRLoadCmds: STD_LOGIC_VECTOR (counters-1 downto 0);			-- counter control reg loads
	signal SCCRReadCmds:	STD_LOGIC_VECTOR (counters-1 downto 0);		-- counter control reg reads

-- pwm generator signals --
	signal RefCountBus: STD_LOGIC_VECTOR (9 downto 0);
	signal LoadPWM: STD_LOGIC_VECTOR  (counters-1 downto 0);
	signal ReadPWM: STD_LOGIC_VECTOR  (counters-1 downto 0);
	signal PCRLoadCmds: STD_LOGIC_VECTOR  (counters-1 downto 0);
	signal PCRReadCmds: STD_LOGIC_VECTOR  (counters-1 downto 0);
	signal GlobalPWMEnable: STD_LOGIC;
	signal GlobalClearPWM: STD_LOGIC;
	signal GlobalClearPWMCmd: STD_LOGIC;
	signal StopPWM: STD_LOGIC;

-- misc i/o signals
	signal PortASel: STD_LOGIC;
	signal DDRASel: STD_LOGIC;
	signal LoadPortA: STD_LOGIC;
	signal LoadDDRA: STD_LOGIC;
	signal ReadDDRA: STD_LOGIC;
	signal ReadPortA: STD_LOGIC;

	signal PortBSel: STD_LOGIC;
	signal DDRBSel: STD_LOGIC;
	signal LoadPortB: STD_LOGIC;
	signal LoadDDRB: STD_LOGIC;
	signal ReadDDRB: STD_LOGIC;
	signal ReadPortB: STD_LOGIC;


-- decodes --
	signal LEDViewSel: STD_LOGIC;
	signal IndexSel: STD_LOGIC;	
	signal GCRSel: STD_LOGIC;	
	signal GMRSel: STD_LOGIC;
	signal CCRSel: STD_LOGIC;
	signal SCCRSel: STD_LOGIC;
	signal PCRSel: STD_LOGIC;
	signal TimeOutSel: STD_LOGIC; 
	signal TimerSel: STD_LOGIC;	
	signal IRQDIVSel: STD_LOGIC;	
	signal PWMValSel: STD_LOGIC;
	signal PhaseSel: STD_LOGIC;
	signal CounterSel: STD_LOGIC;	
	signal SCounterSel: STD_LOGIC;	


  function OneOfEightDecode(ena : std_logic; dec : std_logic_vector(2 downto 0)) return std_logic_vector is
    variable result : std_logic_vector(counters-1 downto 0);
  begin
    if ena = '1' then
      case dec is
        when "000" => result := "00000001";
        when "001" => result := "00000010";
        when "010" => result := "00000100";
        when "011" => result := "00001000";
        when "100" => result := "00010000";
        when "101" => result := "00100000";
        when "110" => result := "01000000";
        when "111" => result := "10000000";

        when others => result := "00000000";
      end case;
    else
      result := "00000000";
    end if;
    return result;
  end OneOfEightDecode;

  	function OneOfEightMux(sel: std_logic_vector (2 downto 0); input: std_logic_vector(counters-1 downto 0)) return std_logic is
 	variable result : std_logic;
  	begin
		case sel is
        when "000" => result := input(0);
        when "001" => result := input(1);
		  when "010" => result := input(2);
		  when "011" => result := input(3);
        when "100" => result := input(4);
        when "101" => result := input(5);
		  when "110" => result := input(6);
		  when "111" => result := input(7);
		  when others => result := '0';
      end case;
    return result;
  end OneOfEightMux;


	component indexreg 
   	port (
		clk: in STD_LOGIC;
		ibus: in STD_LOGIC_VECTOR (15 downto 0);
		obus: out STD_LOGIC_VECTOR (15 downto 0);
		loadindex: in STD_LOGIC;
		readindex: in STD_LOGIC;
		index: out STD_LOGIC_VECTOR (7 downto 0)
		);
	end component;
		
	component counter
		port ( 
		obus: out STD_LOGIC_VECTOR (31 downto 0);
		ibus: in STD_LOGIC_VECTOR (31 downto 0);
		quada: in STD_LOGIC;
		quadb: in STD_LOGIC;
		index: in STD_LOGIC;
		ccrloadcmd: in STD_LOGIC;
		ccrreadcmd: in STD_LOGIC;
		countoutreadcmd: in STD_LOGIC;
		countlatchcmd: in STD_LOGIC;
		countclearcmd: in STD_LOGIC;
		countenable: in STD_LOGIC;
		indexmask: in STD_LOGIC;
		nads: in STD_LOGIC;
		clk: in STD_LOGIC
		);
	end component;

	component pwmgenh 
		port (
		clk: in STD_LOGIC;
		hclk: in STD_LOGIC;
		refcount: in STD_LOGIC_VECTOR (9 downto 0);
		ibus: in STD_LOGIC_VECTOR (15 downto 0);
		obus: out STD_LOGIC_VECTOR (15 downto 0);
		loadpwmval: in STD_LOGIC;
		readpwmval: in STD_LOGIC;
		clearpwmval: in STD_LOGIC;
		pcrloadcmd: STD_LOGIC;
		pcrreadcmd: STD_LOGIC;
		pwmout: out STD_LOGIC;
		dirio: inout STD_LOGIC;
		enablein: in STD_LOGIC;
		enableout: out STD_LOGIC
		);
	end component pwmgenh;
		
	component pwmrefh is
   	port (
		clk: in STD_LOGIC;
		hclk: in STD_LOGIC;
		refcount: out STD_LOGIC_VECTOR (9 downto 0);
		irqgen: out STD_LOGIC;
		ibus: in STD_LOGIC_VECTOR (15 downto 0);
		obus: out STD_LOGIC_VECTOR (15 downto 0);
		irqdivload: in STD_LOGIC;
		irqdivread: in STD_LOGIC;
		phaseload: in STD_LOGIC;
		phaseread: in STD_LOGIC
		);
	end component pwmrefh;

 	
	component globalcontrolreg is
	   port (
		clk: in STD_LOGIC;
		ibus: in STD_LOGIC_VECTOR (15 downto 0);
		reset: in STD_LOGIC;
		loadgcr: in STD_LOGIC;
		ctrclear: out STD_LOGIC;
		ctrlatch: out STD_LOGIC;
		pwmclear: out STD_LOGIC;
		irqclear: out STD_LOGIC;
		reloadwd: out STD_LOGIC 
	);
	end component globalcontrolreg;

	component globalmodereg is
   	port (
		clk: in STD_LOGIC;
		ibus: in STD_LOGIC_VECTOR (15 downto 0);
		obus: out STD_LOGIC_VECTOR (15 downto 0);
		reset: in STD_LOGIC;
		loadglobalmode: in STD_LOGIC;
		readglobalmode: in STD_LOGIC;
		ctrena: out STD_LOGIC;
		pwmena: out STD_LOGIC;
		clearpwmena: in STD_LOGIC;
		loi: out STD_LOGIC;
		som: out STD_LOGIC;
		sot: out STD_LOGIC;
		miout: out STD_LOGIC;
		miin: in STD_LOGIC;
   	irqmask: out STD_LOGIC;
   	irqstatus: in STD_LOGIC		
		);
	end component globalmodereg;

	component WordPR24 is 
		port (
		clear: in STD_LOGIC;
		clk: in STD_LOGIC;
		ibus: in STD_LOGIC_VECTOR (23 downto 0);
		obus: out STD_LOGIC_VECTOR (23 downto 0);
		loadport: in STD_LOGIC;
		loadddr: in STD_LOGIC;
		readddr: in STD_LOGIC;
		portdata: out STD_LOGIC_VECTOR (23 downto 0)
		);
	end component WordPR24;

	component Word24RB is
    Port (			
	 		obus: out STD_LOGIC_VECTOR (23 downto 0);
			readport: in STD_LOGIC;
			portdata: in STD_LOGIC_VECTOR (23 downto 0) );
	end component Word24RB;

	component Timeout is
    Port ( clk : in std_logic;
           ibus : in std_logic_vector(15 downto 0);
           obus : out std_logic_vector(15 downto 0);
           timeoutload : in std_logic;
           timeoutread : in std_logic;
           timerread : in std_logic;
           reload : in std_logic;
           timerz : out std_logic);
	end component Timeout;

	component idreadback is
    Generic ( id : std_logic_vector(31 downto 0);
	           mc : std_logic_vector(31 downto 0));

	 Port ( readid : in std_logic;
           readmc : in std_logic;
           obus : out std_logic_vector(31 downto 0));
	end component idreadback;

	
	begin

 

	makecounters: for i in 0 to (counters -1) generate
		counterx: counter port map ( 		
		obus => D,
		ibus => LAD,
		quada => A(i),
		quadb => B(i),		
		index => Idx(i),
		ccrloadcmd => CCRLoadCmds(i),
		ccrreadcmd =>	CCRReadCmds(i),
		countoutreadcmd  => CounterRead(i),
		countlatchcmd => GlobalCountLatch,	
		countclearcmd => GlobalCounterClear,
		countenable => GlobalCounterEnable,
		indexmask => IOBITSA(16+i),
		nads => ADS,
		clk => LClk
		);	
	end generate;
	
	makescounters: for i in 0 to 3 generate
		counterx: counter port map ( 		
		obus => D,
		ibus => LAD,
		quada => IOBITSA((i*4)),
		quadb => IOBITSA((i*4)+1),		
		index => IOBITSA((i*4)+2),
		ccrloadcmd => SCCRLoadCmds(i),
		ccrreadcmd =>	SCCRReadCmds(i),
		countoutreadcmd  => SCounterRead(i),
		countlatchcmd => GlobalCountLatch,	
		countclearcmd => GlobalCounterClear,
		countenable => GlobalCounterEnable,
		indexmask => IOBITSA((i*4)+3),
		nads => ADS,
		clk => LClk
		);	
	end generate;


	makepwmgen: for i in 0 to (counters -1) generate
		pwmgenx: pwmgenh port map (
		clk => LClk,
		hclk => FClk,
		refcount => RefCountBus,
		ibus => LAD(15 downto 0),
		obus => D(15 downto 0),
		loadpwmval => LoadPWM(i),
		readpwmval => ReadPWM(i),
		clearpwmval => GlobalClearPWM,
		pcrloadcmd => PCRLoadCmds(i),
		pcrreadcmd => PCRReadCmds(i),
		pwmout => PWM(i),
		dirio => Dir(i),
		enablein => GlobalPWMEnable,
		enableout =>EnaSigs(i)
	);
	end generate;

	
	oporta: WordPR24 port map (
		clear => '0',
		clk => LClk,
		ibus => LAD(23 downto 0),
		obus => D(23 downto 0),
		loadport => LoadPortA,
		loadddr => LoadDDRA,
		readddr => ReadDDRA,
		portdata => IOBITSA 
		);	


	iporta: Word24RB port map (
		obus => D(23 downto 0),
		readport => ReadPortA,
		portdata => IOBITSA 
		);	

--	oportb: WordPR24 port map (
--		clear => '0',
--		clk => LClk,
--		ibus => LAD(23 downto 0),
--		obus => D(23 downto 0),
--		loadport => LoadPortB,
--		loadddr => LoadDDRB,
--		readddr => ReadDDRB,
--		portdata => IOBITSB 
--		);	


--	iportb: Word24RB port map (
--		obus => D(23 downto 0),
--		readport => ReadPortB,
--		portdata => IOBitsB 
--		);	
				 
	pwmrefcount: pwmrefh port map (
		clk => LClk,
		hclk => FClk,
		refcount => RefCountBus,
		irqgen => IRQSource,
		ibus => LAD(15 downto 0),
		obus => D(15 downto 0),
		irqdivload => LoadIRQDivCmd,
		irqdivread => ReadIRQDivCmd,
		phaseload => LoadPhaseCmd,
		phaseread => ReadPhaseCmd
		);


	gLedreg: indexreg port map (			
		clk => LClk,
		ibus => LAD(15 downto 0),
		obus => D(15 downto 0),
		loadindex => LoadLEDViewCmd,
		readindex => ReadLEDViewCmd,
		index => LEDView
		);

	ggcontrolreg: globalcontrolreg port map (
		clk => LClk,
		ibus => LAD(15 downto 0),
		reset => '0',
		loadgcr => LoadGCRCmd,
		ctrclear => GlobalCounterClear,
		ctrlatch => GlobalCountLatchCmd,
		pwmclear => GlobalClearPWMCmd,
		irqclear => ClearIRQCmd,
		reloadwd => ReloadWDCmd
		);

	gglobalmodereg: globalmodereg port map (
		clk => LClk,
		ibus => LAD(15 downto 0),
		obus => D(15 downto 0),
		reset => '0',
		loadglobalmode => loadGMRCmd,
		readglobalmode => ReadGMRCmd,
		ctrena => GlobalCounterEnable,
		pwmena => GlobalPWMEnable,
		clearpwmena => StopPWM,
		loi => LatchOnInterrupt,
		som => StopOnMissedIRQ,
		sot => StopOnTimeout,
		miout	=> ClearMissedIRQ,
		miin => MissedIRQ,
		irqmask => IRQMask,
		irqstatus => IRQLatch
		);

    atimeout: timeout port map ( 
	 	clk => LClk,
      ibus => LAD(15 downto 0), 
      obus => D(15 downto 0),
      timeoutload	 => loadTimeOutCmd,
      timeoutread => ReadTimeOutCmd,
      timerread => ReadTimerCmd,
      reload => ReLoadWDCmd,
      timerz => WDTimeout
		 );

    aidreadback: idreadback 
	     generic map (
         id => HMID,
	      mc => MasterClock
		   )
	    port map( 
		   readid => ReadIDCmd,
			readmc => ReadMCCmd,
         obus => D
			);		

    CLKDLL_inst : CLKDLL
   generic map (
      CLKDV_DIVIDE => 2.0, --  Divide by: 1.5,2.0,2.5,3.0,4.0,5.0,8.0 or 16.0
      DUTY_CYCLE_CORRECTION => TRUE, --  Duty cycle correction, TRUE or FALSE
      FACTORY_JF => X"C080",  --  FACTORY JF Values
      STARTUP_WAIT => FALSE)  --  Delay config DONE until DLL LOCK, TRUE/FALSE
   port map (
      CLK0 => CLK0,     -- 0 degree DLL CLK output
		CLKFB =>FClk,		-- DLL feedback
		CLK2X => CLK2X,   -- 2X DLL CLK output
      CLKIN => SYNCLK,  -- Clock input (from IBUFG, BUFG or DLL)
      RST => '0'        -- DLL asynchronous reset input
   );
  
  BUFG_inst : BUFG
   port map (
      O => FClk,    -- Clock buffer output
      I => CLK2X      -- Clock buffer input
   );

	LADDrivers: process (D,FastRead)
	begin 
		if FastRead ='1' then	
			LAD <= D;
		else
			LAD <= "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ";			
		end if;
	end process LADDrivers;

	AddressLatch: process (lclk)
	begin
		if lclk'event and LClk = '1' then
	  		if ADS = '0' then
	  			LatchedA <= LAD(15 downto 0);
				LatchedLBE <= LBE;
			end if;
		end if;
	end process AddressLatch;


	-- we generate an early read from ADS and LR_W
	-- since the 10 nS LRD delay and 5 nS setup time
 	-- only give us 15 nS to provide data to the PLX chip
	
	MakeFastRead: process (lclk,PreFastread,LRD)
	begin
		if lclk'event and LClk = '1' then
			if ADS = '0' and LW_R = '0'then
				PreFastRead <= '1';
			else 
				PreFastRead <= '0'; 
			end if;
		end if;
		FastRead <= PreFastRead or (not LRD);
	end process MakeFastRead;

		
	Decode: process (LatchedA) 
	begin 
	

		if LatchedA(7 downto 5) = "000" then 	 -- 32 bit access
			CounterSel <= '1'; 
		else 
			CounterSel <= '0';
		end if;		

		if LatchedA(7 downto 5) = "001" then 	 -- 32 bit access
			SCounterSel <= '1'; 
		else 
			SCounterSel <= '0';
		end if;		
		
		if LatchedA(7 downto 4) = "0100" then    -- 16 bit access
			CCRSel <= '1'; 
		else 
			CCRSel <= '0';
		end if;			

		if LatchedA(7 downto 4) = "0101" then    -- 16 bit access
			SCCRSel <= '1'; 
		else 
			SCCRSel <= '0';
		end if;			

 		if LatchedA(7 downto 5) = "011" then 		 -- 16 bit access
			PWMValSel <= '1'; 
		else 
			PWMValSel <= '0';
		end if;				

 		if LatchedA(7 downto 5) = "100" then 		-- 16 bit access
			PCRSel <= '1'; 
		else 
			PCRSel <= '0';
		end if;		

  		if LatchedA(7 downto 2) = "101000" then 		-- 32 bit access
			PortASel <= '1'; 
		else 
			PortASel <= '0';
		end if;		
 
  		if LatchedA(7 downto 2) = "101001" then 		-- 32 bit access
			DDRASel <= '1'; 
		else 
			DDRASel <= '0';
		end if;		
 

  
--  		if LatchedA(7 downto 2) = "101010" then 		-- 32 bit access
--			PortBSel <= '1'; 
--		else 
--			PortBSel <= '0';
--		end if;		
 
--  		if LatchedA(7 downto 2) = "101011" then 		-- 32 bit access
--			DDRBSel <= '1'; 
--		else 
--			DDRBSel <= '0';
--		end if;		

  		if LatchedA(7 downto 2) = "110100" then 		-- 32 bit access D0
			IDSel <= '1'; 
		else 
			IDSel <= '0';
		end if;		
    	
		if LatchedA(7 downto 2) = "110101" then 		-- 32 bit access D4
			MCSel <= '1'; 
		else 
			MCSel <= '0';
		end if;		

 		if LatchedA(7 downto 1) = "1100000"  then 	-- 16 bit access
			GCRSel <= '1'; 
		else 
			GCRSel <= '0';
		end if;
			
		if LatchedA(7 downto 1) = "1100001" then 		-- 16 bit access
			GMRSel <= '1'; 									
		else 
			GMRSel <= '0';
		end if;			
			
		if LatchedA(7 downto 1) = "1100010"then 	  -- 16 bit access
			IRQDivSel <= '1'; 
		else 
			IRQDivSel <= '0';
		end if;		

 		if LatchedA(7 downto 1) = "1100011"then 	  -- 16 bit access
			PhaseSel <= '1'; 
		else 
			PhaseSel <= '0';
		end if;		

		if LatchedA(7 downto 1) = "1100100" then 		-- 16 bit access
			TimeOutSel <= '1'; 
		else 
			TimeOutSel <= '0';
		end if;					

		if LatchedA(7 downto 1) = "1100101" then 		-- 16 bit access
			TimerSel <= '1'; 
		else 
			TimerSel <= '0';
		end if;					

		if LatchedA(7 downto 1) = "1100110" then 		-- 16 bit access
			LEDViewSel <= '1'; 
		else 
			LEDViewSel <= '0';
		end if;					

		
	
	end process;		

								

	SigsOut: process (EnaSigs)
	begin
		Ena <= EnaSigs;
	end process;
	
	CounterDecode: process (CounterSel, Fastread, LatchedA) 
	begin 
		if FastRead = '1' then
			CounterRead <= OneOfEightDecode(CounterSel,LatchedA(4 downto 2));
 		else
			CounterRead <= (others => '0');
		end if;
	end process;	

	SCounterDecode: process (SCounterSel, Fastread, LatchedA) 
	begin 
		if FastRead = '1' then
			SCounterRead <= OneOfEightDecode(SCounterSel,LatchedA(4 downto 2));
 		else
			SCounterRead <= (others => '0');
		end if;
	end process;	

	CCRegs: process (CCRSel, FastRead, LWR, LatchedA) 
	begin 
 		if FastRead = '1' then
			CCRReadCmds <= OneOfEightDecode(CCRSel,LatchedA(3 downto 1));
 		else
			CCRReadCmds <= (others => '0');
		end if;
		if LWR = '0' then
			CCRLoadCmds <= OneOfEightDecode(CCRSel,LatchedA(3 downto 1));
 		else
			CCRLoadCmds <= (others => '0');
		end if;
	end process;	

	SCCRegs: process (SCCRSel, FastRead, LWR, LatchedA) 
	begin 
 		if FastRead = '1' then
			SCCRReadCmds <= OneOfEightDecode(SCCRSel,LatchedA(3 downto 1));
 		else
			SCCRReadCmds <= (others => '0');
		end if;
		if LWR = '0' then
			SCCRLoadCmds <= OneOfEightDecode(SCCRSel,LatchedA(3 downto 1));
 		else
			SCCRLoadCmds <= (others => '0');
		end if;
	end process;	
  
	PWMdecode: process (PWMValSel,Fastread, LWR, LatchedA) 
	begin 
		if FastRead = '1' then
			ReadPWM <= OneOfEightDecode(PWMValSel,LatchedA(3 downto 1));
 		else
			ReadPWM <= (others => '0');
		end if; 			
		if LWR = '0' then
			LoadPWM <= OneOfEightDecode(PWMValSel,LatchedA(3 downto 1));
 		else
			LoadPWM <= (others => '0');
		end if; 		
	end process;	

 	PCRegs: process (PCRSel,Fastread, LWR, LatchedA) 
	begin 
		if FastRead = '1' then
			PCRReadCmds <= OneOfEightDecode(PCRSel,LatchedA(3 downto 1));
 		else
			PCRReadCmds <= (others => '0');
		end if; 			
		if LWR = '0' then
			PCRLoadCmds <= OneOfEightDecode(PCRSel,LatchedA(3 downto 1));
 		else
			PCRLoadCmds <= (others => '0');
		end if; 		
	end process;	

	PortADecode: process (PortASel,FastRead,LWR)
	begin
		if PortASel = '1' and LWR = '0' then 
			LoadPortA <= '1';
		else 
			LoadPortA <= '0';
		end if;
		if PortASel = '1' and FastRead = '1' then 
			ReadPortA <= '1';
		else 
			ReadPortA <= '0';
		end if;
	end process PortADecode;

 	DDRADecode: process (DDRASel,FastRead,LWR)
	begin
		if DDRASel = '1' and LWR = '0' then 
			LoadDDRA <= '1';
		else 
			LoadDDRA <= '0';
		end if;
		if DDRASel = '1' and FastRead = '1' then 
			ReadDDRA <= '1';
		else 
			ReadDDRA <= '0';
		end if;
	end process DDRADecode;

--	PortBDecode: process (PortBSel,FastRead,LWR)
--	begin
--		if PortBSel = '1' and LWR = '0' then 
--			LoadPortB <= '1';
--		else 
--			LoadPortB <= '0';
--		end if;
--		if PortBSel = '1' and FastRead = '1' then 
--			ReadPortB <= '1';
--		else 
--			ReadPortB <= '0';
--		end if;
--	end process PortBDecode;

-- 	DDRBDecode: process (DDRBSel,FastRead,LWR)
--	begin
--		if DDRBSel = '1' and LWR = '0' then 
--			LoadDDRB <= '1';
--		else 
--			LoadDDRB <= '0';
--		end if;
--		if DDRBSel = '1' and FastRead = '1' then 
--			ReadDDRB <= '1';
--		else 
--			ReadDDRB <= '0';
--		end if;
--	end process DDRBDecode;


	GCRDecode: process (GCRSel,LWR)
	begin
		if GCRSel = '1' and LWR = '0' then 
			LoadGCRCmd <= '1';
		else 
			LoadGCRCmd <= '0';
		end if;
	end process GCRDecode;

	GMRDecode: process (GMRSel,FastRead,LWR)
	begin
		if GMRSel = '1' and LWR = '0' then 
			LoadGMRCmd <= '1';
		else 
			LoadGMRCmd <= '0';
		end if;
		if GMRSel = '1' and FastRead = '1' then 
			ReadGMRCmd <= '1';
		else 
			ReadGMRCmd <= '0';
		end if;
	end process GMRDecode;

	TimeOutDecode: process (TimeOutSel,FastRead,LWR)
	begin
		if TimeoutSel = '1' and LWR = '0' then 
			LoadTimeOutCmd <= '1';
		else 
			LoadTimeOutCmd <= '0';
		end if;
		if TimeOutSel = '1' and FastRead = '1' then 
			ReadTimeOutCmd <= '1';
		else 
			ReadTimeOutCmd <= '0';
		end if;
	end process TimeOutDecode;

	TimerDecode: process (TimerSel,FastRead,LWR)
	begin
		if TimerSel = '1' and FastRead = '1' then 
			ReadTimerCmd <= '1';
		else 
			ReadTimerCmd <= '0';
		end if;
	end process TimerDecode;
	
	LEDViewDecode: process (LedViewSel,FastRead,LWR)
	begin
		if LEDViewSel = '1' and LWR = '0' then 
			LoadLEDViewCmd <= '1';
		else 
			LoadLEDViewCmd <= '0';
		end if;
		if LEDViewSel = '1' and FastRead= '1' then 
			ReadLEDViewCmd <= '1';
		else 
			ReadLEDViewCmd <= '0';
		end if;
	end process LEDViewDecode;

	
	IRQDivDecode: process (IRQDivSel,FastRead,LWR)
	begin
		if IRQDivSel = '1' and LWR = '0' then 
			LoadIRQDivCmd <= '1';
		else 
			LoadIRQDivCmd <= '0';
		end if;
		if IRQDivSel = '1' and FastRead = '1' then 
			ReadIRQDivCmd <= '1';
		else 
			ReadIRQDivCmd <= '0';
		end if;
	end process IrqDivDecode;

	PhaseDecode: process (PhaseSel,FastRead,LWR)
	begin
		if PhaseSel = '1' and LWR = '0' then 
			LoadPhaseCmd <= '1';
		else 
			LoadPhaseCmd <= '0';
		end if;
		if PhaseSel = '1' and FastRead = '1' then 
			ReadPhaseCmd <= '1';
		else 
			ReadPhaseCmd <= '0';
		end if;
	end process PhaseDecode;

	IDDecode: process (IDSel,FastRead)
	begin
		if IDSel = '1' and FastRead = '1' then 
			ReadIDCmd <= '1';
		else 
			ReadIDCmd <= '0';
		end if;
	end process IDDecode;

	MCDecode: process (MCSel,FastRead)
	begin
		if MCSel = '1' and FastRead = '1' then 
			ReadMCCmd <= '1';
		else 
			ReadMCCmd <= '0';
		end if;
	end process MCDecode;


	irqlogic: process (CardSelect,
							IRQSource, 
							IrqLatch, 
							ClearMissedIRQ,
							MissedIRQ,
							StopOnMissedIRQ,
							LatchOnInterrupt,
							GlobalCountLatchCmd,
							GlobalClearPWMCmd,
							ClearIRQCmd)
	begin		
 		if IrqSource'event and IRQsource = '1' then
			IRQLatch <= '1';
			if IRQLatch = '1' then 	-- if IRQLatch is set and we get the next interrupt
				MissedIRQ <= '1';		-- set Missed IRQ latch	
			end if;			
		end if;

		if LClk'event and LClk = '1' then
			if ((IRQLatch = '1') and (LatchOnInterrupt = '1')) or (GlobalCountLatchCmd = '1') then
				CountLatchEdge1 <= '1';
			else
				CountLatchEdge1 <= '0';
			end if;
			CountLatchEdge2 <= 	CountLatchEdge1;
			if  CountLatchEdge2 = '0' and	CountLatchEdge1 = '1' then
				GlobalCountLatch <= '1';
			else
				GlobalCountLatch <= '0';
		 	end if;
		end if;

		if ClearMissedIRQ = '1' then 
			MissedIRQ <= '0';
		end if;

		if ((MissedIRQ = '1') and (StopOnMissedIRQ = '1')) or
			((WDTimeOut = '1') and (StopOnTimeout = '1')) then
			StopPWM <= '1';
		else 
			StopPWM <= '0';
		end if;
		
		if (StopPWM = '1') or (GlobalClearPWMCmd = '1') then  -- either stop on pwm or global clear reset pwm gens
			GlobalClearPWM <= '1';
		else
			GlobalClearPWM <= '0';
		end if;
			
		if ClearIRQCmd = '1' then  --  clear IRQ
 	   	IRQLatch <= '0';
		end if;
		
		Int <=  not (IRQLatch and IRQMask);	-- drive our (active low) interrupt pin
		Ready <= '0';				-- We're always ready
		
	end process;


	LEDDrive: process (A,B,Idx,Dir,PWM,IRQLatch,LedView) 
	begin 
	
		LEDS(7) <= not IRQLatch;
		LEDS(6) <= not OneOfEightMux(LEDView(2 downto 0),A);
		LEDS(5) <= not OneOfEightMux(LEDView(2 downto 0),B);
		LEDS(4) <= not OneOfEightMux(LEDView(2 downto 0),Idx);
		LEDS(3) <= not OneOfEightMux(LEDView(2 downto 0),Dir);
		LEDS(2) <= not OneOfEightMux(LEDView(2 downto 0),PWM);
		LEDS(1) <= OneOfEightMux(LEDView(2 downto 0),EnaSigs);
		LEDS(0) <= not WDTimeout;
		
	end process leddrive;


end dataflow;

  