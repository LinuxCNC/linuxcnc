library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity pwmref is
    Port (
	 		clk: in STD_LOGIC;
			refcount: out STD_LOGIC_VECTOR (9 downto 0);
			irqgen: out STD_LOGIC;
			ibus: in STD_LOGIC_VECTOR (15 downto 0);
			obus: out STD_LOGIC_VECTOR (15 downto 0);
			irqdivload: in STD_LOGIC;
			irqdivread: in STD_LOGIC;
			phaseload: in STD_LOGIC;
			phaseread: in STD_LOGIC
			);
end pwmref;

architecture behavioral of pwmref is

signal count: STD_LOGIC_VECTOR (9 downto 0);
signal irqdivisor: STD_LOGIC_VECTOR (7 downto 0);
signal irqcounter: STD_LOGIC_VECTOR (7 downto 0);
signal phaseacc: STD_LOGIC_VECTOR (16 downto 0);
alias  phasemsb: std_logic is phaseacc(16);
signal oldphasemsb: std_logic;
signal phaselatch: STD_LOGIC_VECTOR (15 downto 0);

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
		if clk'event and clk = '1' then
			phaseacc <= phaseacc + phaselatch;
			oldphasemsb <= phasemsb;
			if oldphasemsb /= phasemsb then
				count <= count + 1;							
				if count = 0 then
					irqcounter <= irqcounter -1;
					if irqcounter = 0 then	
						irqgen <= '1'; 
						irqcounter <= irqdivisor;
					else 
						irqgen <= '0';
					end if; -- irqcounter = 0
				end if;	-- count = 0		
			end if;		-- old /= new
			
			if irqdivload = '1' then
 		   	irqdivisor <= ibus(7 downto 0);
				irqcounter <= irqdivisor;
			end if;
			if phaseload = '1' then
 		   	phaselatch <= ibus;
			end if;				

		end if;  -- clk
		if irqdivread = '1' and phaseread = '0' then
			obus(7 downto 0) <= irqdivisor;
			obus(15 downto 8) <= x"00";
		elsif phaseread = '1' and irqdivread = '0' then
			obus <= phaselatch;
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;
		refcount <= count;
	end process;

end behavioral;

