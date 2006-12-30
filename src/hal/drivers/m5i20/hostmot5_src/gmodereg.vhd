library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity globalmodereg is
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
end globalmodereg;

architecture behavioral of globalmodereg is


signal ctrenareg: STD_LOGIC;
signal pwmenareg: STD_LOGIC;
signal loireg: STD_LOGIC;
signal somreg: STD_LOGIC;
signal sotreg: STD_LOGIC;
signal mioutreg: STD_LOGIC;
signal irqmaskreg: STD_LOGIC;


begin
	aglobalmodereg: process(
							clk, 
							ibus,
							loadglobalmode,
							reset,
							readglobalmode,
 							ctrenareg,
							pwmenareg,
							clearpwmena,
							loireg,
							somreg,
							sotreg,
							mioutreg,
							miin
							)
							
	begin
		if clk'event and clk = '1' then

			if reset = '1' then 
 				ctrenareg <= '0';
				pwmenareg <= '0';
				loireg	 <= '0';
				somreg	 <= '0';
				sotreg	 <= '0';
				mioutreg	 <= '1';	
				irqmaskreg   <= '0';		
			elsif loadglobalmode = '1' then
 				ctrenareg <= ibus(0);
				pwmenareg <= ibus(1);
				loireg	 <= ibus(2);
				somreg	 <= ibus(3);
				mioutreg	 <= not ibus(4);
				irqmaskreg <= ibus(5);
				sotreg	 <= ibus(7);
			end if;	

			if mioutreg = '1' then 
				mioutreg <= '0';
			end if;
		
		end if;
		
		if clearpwmena = '1' then 
			pwmenareg <= '0';
		end if;

		if readglobalmode = '1' then
			obus(0) <= ctrenareg;
			obus(1) <= pwmenareg;
			obus(2) <= loireg;
			obus(3) <= somreg;
			obus(4) <= miin;	
			obus(5) <= irqmaskreg;
			obus(6) <= irqstatus;
			obus(7) <= sotreg;		
			obus(15 downto 8) <= "ZZZZZZZZ";
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;
		ctrena <= ctrenareg;
		pwmena <= pwmenareg;
		loi   <= loireg;
		miout <= mioutreg or reset;
		som	<= somreg;	
		irqmask <= irqmaskreg;
		sot <=	sotreg; 


	end process;
end behavioral;

