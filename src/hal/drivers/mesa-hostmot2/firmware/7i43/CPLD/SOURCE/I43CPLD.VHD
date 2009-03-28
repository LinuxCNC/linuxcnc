library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity i43cpld is
		generic(
			FPGASize : std_logic := '0'			-- FPGA size 0 = 200K, 1 = 400K
			);
		Port ( 
			clk : in std_logic;
			opt : in std_logic_vector(1 downto 0);
			nreset : in std_logic;
			usbnrd : out std_logic;
			usbwr : out std_logic;
--		 usbntxe : in std_logic;
			usbnrxf : in std_logic;
			fpganrdwr : out std_logic;
			fpgancs_paramode : inout std_logic;
			fpganreconfig : in std_logic;
			fpganprogram : out std_logic;
			fpgadone : in std_logic;
			fpgacclk : inout std_logic;
			fpgaserial : out std_logic;
			fpgaconfigd0 : out std_logic;
			fpganinit: in std_logic;
			fpga_epp_astrobe: out std_logic;
			fpga_epp_dstrobe: out std_logic;
			fpga_epp_read: out std_logic;
			fpga_epp_wait: in std_logic;
			cmdo : in std_logic;
			cmdi : out std_logic;
			cmcs : out std_logic;
			nstrobe_nwrite : in std_logic;
			ninit : in std_logic;
			autofd_dstrobe : in std_logic;
			selectin_astrobe : in std_logic;
			busy_wait : out std_logic;
			fault : out std_logic;
			perror : out std_logic;
			busd0 : inout std_logic);
end i43cpld;

architecture Behavioral of i43cpld is

type configtype is (para,usb,spi,done);
signal configmode : configtype;

signal fcnt : std_logic_vector(1 downto 0);
signal filter : std_logic_vector(1 downto 0);
signal waitpipe : std_logic_vector(4 downto 0);
signal para_cclk : std_logic;
signal para_program : std_logic;
signal para_wait: std_logic;
signal dautofd_dstrobe: std_logic;
signal dselectin_astrobe: std_logic;	  


signal alatch : std_logic; 
signal dwrite : std_logic; 
signal awrite : std_logic; 

signal usb_cclk : std_logic;
signal dusb_cclk : std_logic;
signal usb_echo : std_logic := '1';
signal rdtimer: std_logic_vector(3 downto 0);
signal usbtxoffsr: std_logic_vector(3 downto 0) := x"F";
alias  usblastd0 : std_logic is usbtxoffsr(0);
signal dusbnrxf : std_logic;
signal usbconfigread : std_logic;
signal usbconfigwrite : std_logic;
signal usbwritetsen : std_logic;
signal spi_program : std_logic;

signal spi_cmcs : std_logic;
signal spi_cmdi : std_logic;
signal spi_cclk : std_logic;
signal pdelcounter : std_logic_vector(11 downto 0);
signal lastcounts : std_logic_vector(3 downto 0); 
signal spiprescale  : std_logic_vector(3 downto 0);
signal eestate : std_logic_vector(3 downto 0);
signal senddata : std_logic;
signal dospiconfig : std_logic;

begin

			
	paraconfig: process(clk,waitpipe, alatch, filter, nstrobe_nwrite, 
							  autofd_dstrobe, selectin_astrobe)
	begin

		if rising_edge(clk) then
			dautofd_dstrobe <= autofd_dstrobe;		-- 1st stage sync FF
			dselectin_astrobe <= selectin_astrobe;	-- 1st stage sync FF		  
			filter(1) <= filter(0);

			if dautofd_dstrobe = '0' or dselectin_astrobe = '0' then
				waitpipe  <=  waitpipe(3 downto 0) & '1';
				if fcnt /= "11" then 
					fcnt <= fcnt +1;
				end if;
			else
				waitpipe <= "00000";
				if fcnt /= "00" then 
					fcnt <= fcnt -1;
				end if;
			end if;
			if fcnt = "11" then	 
				filter(0) <= '1';
			end if;
			if fcnt = "00" then 
				filter(0) <= '0';
			end if;
			
			if awrite = '1' then 
				alatch <= busd0;
			end if;
		
			if dwrite = '1'  and alatch = '1' then
				para_program <= busd0;
			end if;	
		end if; -- clk	
		
		para_wait <= waitpipe(4);
		
		if filter = "01" and nstrobe_nwrite = '0' and dautofd_dstrobe = '0' then
			dwrite <= '1';
		else
			dwrite <= '0';
		end if;
	
		if filter = "01" and nstrobe_nwrite = '0' and dselectin_astrobe = '0' then
			awrite <= '1';
		else
			awrite <= '0';
		end if;
	
		if dwrite = '1' and alatch = '0' then
			para_cclk <= '1';
		else
			para_cclk <= '0';
		end if;	
		
			

		
	end process paraconfig;

	

	usbconfig: process(clk, rdtimer)
	begin
		if rising_edge(clk) then
			dusbnrxf <= usbnrxf;		-- async so need 1 FF sync
         dusb_cclk <= usb_cclk; 
			usbwritetsen <= usbconfigwrite; -- delayed one clock for data hold time
			if rdtimer /= "0000" then
				rdtimer <= rdtimer -1;		
			end if;		
			if rdtimer = "0000" and dusbnrxf = '0' then
				rdtimer <= "1111";
			end if;		
			if usb_cclk= '1' and dusb_cclk = '0' then
				usbtxoffsr <= usbtxoffsr(2 downto 0)& busd0; -- left shift 20 ns after rising edge of cclk
			end if;
			if usbtxoffsr = x"0" then								-- if host has sent 4 characters in a row with '1' lsb, turn off echo
				usb_echo <= '0';
			end if;
			if (nreset = '0') or (fpganreconfig = '0') then	-- enable usbecho at startup or reconfig
				usb_echo <= '1';
				usbtxoffsr <= x"F";
			end if;	
         dusb_cclk <= usb_cclk; 
			
		end if; -- clk
		usbconfigwrite <= not rdtimer(3) and (rdtimer(2) xor rdtimer(1)) and usb_echo;
		usbconfigread <= not rdtimer(3);
		usb_cclk <= rdtimer(3) and not rdtimer(2);
		
	end process usbconfig;



	spiconfig : process (clk)
	begin
		if rising_edge(clk) then
			if  (nreset = '0') or (fpganreconfig = '0') then
				spi_program <= '0';
				spi_cmcs <= '1';
				spi_cmdi <= '0';
				spi_cclk <= '0';
				senddata <= '0';
				eestate <= x"0";
				pdelcounter <= x"000";
				spiprescale <= x"0";
				lastcounts <= x"0";
				dospiconfig <= '1';
			 
			else
				if dospiconfig = '1' then
					spi_program <= '1';				
					spiprescale <= spiprescale +1;
					spi_cclk <= spiprescale(3);
					if spiprescale = x"0" then
						pdelcounter <= pdelcounter + 1;
						if pdelcounter(11) = '1' then 
							senddata <= '1';
						end if;
						if senddata = '1' then
							if eestate /= 15 then 
								eestate <= eestate +1;
							end if;
							if eestate = 1 then 
								spi_cmcs <= '0';
							end if;
							if eestate = 7 then -- serial EEPROM read command needs ones from now on
								spi_cmdi <= '1';
							end if;
							if fpgadone = '1' then 
								lastcounts <= lastcounts +1;
							end if;
							if lastcounts(3) = '1' then
								dospiconfig <= '0';
								spi_cmcs <= '1';
							end if;			
						end if;
					end if;			
				end if;
			end if;
		end if;				
	end process	spiconfig;		



	doconfig : process (configmode,opt, spi_cclk, para_cclk, usb_cclk, 
							  fpgadone, busd0, autofd_dstrobe, nstrobe_nwrite, 
							  alatch, nreset, ninit, para_program, fpganinit, 
							  para_wait, usbconfigread, cmdo, spi_program, 
							  spi_cmcs, spi_cmdi, fpganreconfig, selectin_astrobe, 
							  fpga_epp_wait)
	begin
		
		if opt(1) = '1' then
			fpgaserial <= 'Z';
			configmode <= spi;
			fpgacclk <= spi_cclk;
		else
			fpgaserial <= '0';
			if opt(0) = '0' then
				configmode <= para;
				fpgacclk <= para_cclk;
			else
				configmode <= usb;
				fpgacclk <= usb_cclk;
			end if;	
		end if;
		

		if fpgadone = '1' then
			configmode <= done;
		end if;	
		
		
		case configmode is
		when para =>	
			fpgaconfigd0 <= busd0;
			if autofd_dstrobe = '0' and nstrobe_nwrite = '1' then
				if alatch = '1' then  
					busd0 <= fpgadone;
				else
					busd0 <= FPGASize;
				end if;	
			else
					busd0 <= 'Z'; 
			end if;			
			fpgancs_paramode <= '0';
			fpganrdwr <= '0';
			fpganprogram <= nreset and ninit and para_program;
			cmcs <= 'Z';
			cmdi <= 'Z';
			perror <= fpgadone;
			fault	 <= fpganinit;
			fpga_epp_astrobe <= 'Z';
			fpga_epp_dstrobe <= 'Z';
			fpga_epp_read <= 'Z';
			busy_wait <= para_wait;
			usbwr <= '0';
			usbnrd<= '1';
		when usb =>
			fpgaconfigd0 <= busd0;
			if usbwritetsen = '1' and usb_echo =  '1' then
				if usblastd0 = '1' then  
					busd0 <= fpgadone;
				else
					busd0 <= FPGASize;
				end if;	
			else
					busd0 <= 'Z'; 
			end if;			
			fpgancs_paramode <= '0';
			fpganrdwr <= '0';
			fpganprogram <= nreset;
			cmcs <= 'Z';
			cmdi <= 'Z';
			perror <= 'Z';
			fault	 <= 'Z';
			fpga_epp_astrobe <= 'Z';
			fpga_epp_dstrobe <= 'Z';
			fpga_epp_read <= 'Z';
			busy_wait <= 'Z';
			usbwr <= usbconfigwrite;			
			usbnrd<= usbconfigread;
		when spi =>
			fpgaconfigd0 <= cmdo;
			busd0 <= 'Z';
			fpgancs_paramode <= '0';
			fpganrdwr <= '0';
			fpganprogram <= nreset and spi_program;
			cmcs <= spi_cmcs;
			cmdi <= spi_cmdi;
			perror <= 'Z';
			fault	 <= 'Z';
			fpga_epp_astrobe <= 'Z';
			fpga_epp_dstrobe <= 'Z';
			fpga_epp_read <= 'Z';
			busy_wait <= 'Z';
			usbwr <= '0';
			usbnrd<= '1';
		when done =>
			fpgaconfigd0 <= 'Z';
			busd0 <= 'Z';
			fpga_epp_astrobe <= selectin_astrobe;
			fpga_epp_dstrobe <= autofd_dstrobe;
			fpga_epp_read <= nstrobe_nwrite;
			if fpgancs_paramode = '1' then		-- FPGA says parallel mode so feed through
				busy_wait <= fpga_epp_wait;
			else
				busy_wait <= 'Z';
			end if;	
			fpgacclk <= 'Z';
			fpgancs_paramode <= 'Z';
			fpganrdwr <= 'Z';
			fpganprogram <= nreset and fpganreconfig;
			cmcs <= 'Z';
			cmdi <= 'Z';
			perror <= 'Z';
			fault	 <= 'Z';
			usbwr <= 'Z';
			usbnrd<= 'Z';
		end case;
	
	end process;
					

end Behavioral;
