library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity threeph is
    Port (
	 		clk: in STD_LOGIC;
			ibus: in STD_LOGIC_VECTOR (15 downto 0);
			obus: out STD_LOGIC_VECTOR (15 downto 0);
			crload: in STD_LOGIC;
			crread: in STD_LOGIC;
			rateload: in STD_LOGIC;
			rateread: in STD_LOGIC;
			phaseouta: out STD_LOGIC;
			phaseoutb: out STD_LOGIC;
			phaseoutc: out STD_LOGIC
			);
end threeph;

architecture behavioral of threeph is

signal prescale: STD_LOGIC_VECTOR (12 downto 0);
signal creg: STD_LOGIC_VECTOR (2 downto 0);
signal phaseacc: STD_LOGIC_VECTOR (16 downto 0);
alias  phasemsb: std_logic is phaseacc(16);
signal phstate: STD_LOGIC_VECTOR   (2 downto 0);
signal oldphasemsb: std_logic;
signal phaselatch: STD_LOGIC_VECTOR (15 downto 0);
signal phasea : std_logic;
signal phaseb : std_logic;
signal phasec : std_logic;
begin
	athreephase: process  (clk,
			                 crread,
                          rateread) 
	begin
		if clk'event and clk = '1' then
			if creg(0) = '0' then 
				phstate <= "000";
			end if;
			phaseacc <= phaseacc + phaselatch;
			oldphasemsb <= phasemsb;
			if oldphasemsb /= phasemsb then
				prescale <= prescale + 1;							
				if prescale = 0 then
					phstate <= phstate +1;
					if phstate = 5 then 
						phstate <= "000";
					end if;
				end if; -- prescale = 0	
			end if;		-- old /= new
			if rateload = '1' then
 		   	phaselatch <= ibus;
			end if;				
			if crload = '1' then
 		   	creg <= ibus(2 downto 0);
			end if;				

		end if;  -- clk
		if crread = '1' and rateread = '0' then
			obus(2 downto 0) <= creg;
			obus(15 downto 3) <= "0000000000000";
		elsif rateread = '1' and crread = '0' then
			obus <= phaselatch;
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;
		
		case phstate is			
			when "000" =>
				phasea <= '1';
				phaseb <= '0';
				phasec <= '1';
			when "001" =>
				phasea <= '1';
				phaseb <= '0';
				phasec <= '0';
			when "010" =>
				phasea <= '1';
				phaseb <= '1';
				phasec <= '0';
			when "011" =>
				phasea <= '0';
				phaseb <= '1';
				phasec <= '0';
			when "100" =>
				phasea <= '0';
				phaseb <= '1';
				phasec <= '1';
			when "101" =>
				phasea <= '0';
				phaseb <= '0';
				phasec <= '1';
			when others =>	 -- start
				phasea <= '1';
				phaseb <= '0';
				phasec <= '1';
		end case;
		
		if creg(1) = '1' then
			if creg(2) = '0' then
				phaseouta <= phasea;
				phaseoutb <= phaseb;
				phaseoutc <= phasec;
			else
				phaseouta <= phaseb;
				phaseoutb <= phasea;
				phaseoutc <= phasec;
			end if;	
		else
			phaseouta <= 'Z';
			phaseoutb <= 'Z';
			phaseoutc <= 'Z';
		end if;
									

	end process;

end behavioral;

