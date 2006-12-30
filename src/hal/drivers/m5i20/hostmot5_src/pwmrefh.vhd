library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity pwmrefh is
    Port (
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
end pwmrefh;

architecture behavioral of pwmrefh is

signal count: STD_LOGIC_VECTOR (9 downto 0);
signal irqdivisor: STD_LOGIC_VECTOR (7 downto 0);
signal preirqdivisor: STD_LOGIC_VECTOR (7 downto 0);
signal irqcounter: STD_LOGIC_VECTOR (7 downto 0);
signal phaseacc: STD_LOGIC_VECTOR (16 downto 0);
alias  phasemsb: std_logic is phaseacc(16);
signal oldphasemsb: STD_LOGIC;
signal phaselatch: STD_LOGIC_VECTOR (15 downto 0);
signal prephaselatch: STD_LOGIC_VECTOR (15 downto 0);
signal irqdivloadreq: STD_LOGIC;
signal oldirqdivloadreq: STD_LOGIC;
signal olderirqdivloadreq: STD_LOGIC;
signal phaselatchloadreq: STD_LOGIC;
signal oldphaselatchloadreq: STD_LOGIC;
signal olderphaselatchloadreq: STD_LOGIC;
signal irqgenreq: STD_LOGIC;
signal oldirqgenreq: STD_LOGIC;
signal olderirqgenreq: STD_LOGIC;
signal irqgenint: STD_LOGIC;

begin
	apwmref: process  (clk,
							irqdivload,
							count,
							irqcounter,
							irqdivisor,
							ibus,
							irqdivread,
							phaseread) 
	begin
		if hclk'event and hclk = '1' then	  -- 100 Mhz high speed clock
			if oldirqdivloadreq = '1' and olderirqdivloadreq = '1' then  -- these are for crossing 33 Mhz/100 MHz
				irqdivisor <= preirqdivisor;								  -- clock domains
				irqcounter <= preirqdivisor;
			end if;
			if oldphaselatchloadreq = '1' and olderphaselatchloadreq = '1' then
				phaselatch <= prephaselatch;
			end if;
			oldirqdivloadreq <= irqdivloadreq;
			oldphaselatchloadreq	<= phaselatchloadreq;
			olderirqdivloadreq <= oldirqdivloadreq;
			olderphaselatchloadreq	<= oldphaselatchloadreq;
			phaseacc <= phaseacc + phaselatch;
			oldphasemsb <= phasemsb;
			if oldphasemsb /= phasemsb then
				count <= count + 1;							
				if count = 0 then
					irqcounter <= irqcounter -1;
					if irqcounter = 0 then	
						irqgenreq <= '1'; 
						irqcounter <= irqdivisor;
					end if; -- irqcounter = 0
				end if;	-- count = 0		
			end if;		-- old /= new
			

		end if;  -- hclk

		if clk'event and clk = '1' then	-- 33 Mhz local clock
			 if olderirqgenreq = '0' and oldirqgenreq = '1' then	 -- just one 33 mhz clk wide
			 	irqgenint <= '1';	
			 else
			 	irqgenint  <='0';
			 end if;
		
			olderirqgenreq <= oldirqgenreq;
			oldirqgenreq <= irqgenreq;
			if irqdivload = '1' then
 		   	preirqdivisor <= ibus(7 downto 0);
				irqdivloadreq <= '1';
			end if;
			
			if phaseload = '1' then
 		   	prephaselatch <= ibus;
				phaselatchloadreq	<= '1';
			end if;				
						
		end if;  -- clk
		
		if olderirqdivloadreq = '1' then  		-- asyncronous request clear
			irqdivloadreq <= '0';
		end if;
		if olderphaselatchloadreq = '1' then	-- asyncronous request clear
			phaselatchloadreq	<= '0';
		end if;
		if olderirqgenreq = '1' then 			-- asyncronous request clear
			irqgenreq <= '0';
		end if;

		if irqdivread = '1' and phaseread = '0' then
			obus(7 downto 0) <= irqdivisor;
			obus(15 downto 8) <= x"00";
		elsif phaseread = '1' and irqdivread = '0' then
			obus <= phaselatch;
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;
		refcount <= count;
		irqgen <= irqgenint;
	end process;

end behavioral;

