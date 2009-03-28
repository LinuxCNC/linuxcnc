
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;


entity sserial is
    Port ( address : in  STD_LOGIC_VECTOR (7 downto 0);
           data : inout  STD_LOGIC_VECTOR (31 downto 0);
           clock : in  STD_LOGIC;
           read : in  STD_LOGIC;
           write : in  STD_LOGIC);
end sserial;

architecture Behavioral of sserial is



begin



	processor: entity DumbAss8
	
	port map (
		clk		 => fclk,
		reset	  => '0',
		iabus	  =>  iabus,		  -- program address bus
		idbus	  =>  idbus,		  -- program data bus		 
		mradd	  =>  mradd,		  -- memory read address
		mwadd	  =>  mwadd,		  -- memory write address
		mibus	  =>  mibus,		  -- memory data in bus	  
		mobus	  =>  mobus,		  -- memory data out bus
		mwrite  =>  mwrite,		  -- memory write signal	
      mread   =>  mread		     -- memory read signal	
--		carryflg  =>				  -- carry flag
		);

	programROM : entity sserialrom 
	port map(
		addr => iabus,
		clk  => fclk,
		din  => x"0000",
		dout => idbus,
		we	=> '0'
	 );

	DataRam : entity sserialram 
	port map(
		addra => mwadd,
		addrb => mradd,
		clk  => fclk,
		dina  => mobus,
--		douta => 
		doutb => mibus_ram,
		wea	=> mwrite
	 );
	 
	 
		makeUARTRs: for i in 0 to UARTs -1 generate
		auarrx: entity uartr8	
		port map (
			clk => clklow,
			ibus => ibus,
			obus => obus,
			popfifo => LoadUARTRData(i),
			loadbitratel => LoadUARTRBitRatel(i),
			loadbitrateh => LoadUARTRBitRateh(i),
			readbitratel => ReadUARTRBitratel(i),
			readbitrateh => ReadUARTRBitrateh(i),
			clrfifo => ClearUARTRFIFO(i),
			readfifocount => ReadUARTRFIFOCount(i),
			loadmode => LoadUARTRMode(i),
			readmode => ReadUARTRMode(i),
			fifohasdata => UARTRFIFOHasData(i),
			rxmask => UTDrvEn(i),			-- for half duplex rx mask
			rxdata => UTRData(i)
         );
	end generate;
	
	makeUARTTXs: for i in 0 to UARTs -1 generate
		auartx:  entity uartx8	
		port map (
			clk => clklow,
			ibus => ibus,
			obus => obus,
			pushfifo => LoadUARTXData(i),
			loadbitratel => LoadUARTXBitRatel(i),
			loadbitrateh => LoadUARTXBitRateh(i),
			readbitratel => ReadUARTXBitratel(i),
			readbitrateh => ReadUARTXBitrateh(i),
			clrfifo => ClearUARTXFIFO(i),
			readfifocount => ReadUARTXFIFOCount(i),
			loadmode => LoadUARTXModeReg(i),
			readmode => ReadUARTXModeReg(i),
			fifoempty => UARTXFIFOEmpty(i),
			txen => '1',
			drven => UTDrvEn(i),
			txdata => UTXData(i)
         );
	end generate;
	
	ram_iomux : process (ioradd(11),mibus_ram,mibus_io)
	begin
		if ioradd(11) = '1' then
			mibus <= mibus_ram;
		else
			mibus <= mibus_io;
		end if;
	end process;


end Behavioral;

