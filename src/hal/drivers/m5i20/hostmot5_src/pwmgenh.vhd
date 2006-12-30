library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity pwmgenh is
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
end pwmgenh;

architecture behavioral of pwmgenh is

signal pwmval: STD_LOGIC_VECTOR (9 downto 0);
signal prepwmval: STD_LOGIC_VECTOR (9 downto 0);
signal fixedpwmval: STD_LOGIC_VECTOR (9 downto 0);
signal fixedrefcount: STD_LOGIC_VECTOR (9 downto 0);
signal pwm: STD_LOGIC;

signal dir: STD_LOGIC;
signal unsignedmode: STD_LOGIC;
signal localenable: STD_LOGIC;
signal lacedpwm: STD_LOGIC;

signal predir: STD_LOGIC;
signal preunsignedmode: STD_LOGIC;
signal prelocalenable: STD_LOGIC;
signal prelacedpwm: STD_LOGIC;

signal loadpwmreq: STD_LOGIC;
signal oldloadpwmreq: STD_LOGIC;
signal olderloadpwmreq: STD_LOGIC;
signal loadpcrreq: STD_LOGIC;
signal oldloadpcrreq: STD_LOGIC;
signal olderloadpcrreq: STD_LOGIC;
begin
	apwmgen: process  (clk,
							hclk, 
	 						refcount,
							ibus,
							loadpwmval,
							readpwmval,
							clearpwmval,
							enablein,
							localenable,
							lacedpwm,
							pwmval,
							dir, 
							pcrreadcmd,
						 	pwm
							)
							
	begin
		if hclk'event and hclk = '1' then	  		
			if oldloadpwmreq = '1'  and olderloadpwmreq = '1'then
				pwmval <= prepwmval;
				dir <= predir;
			end if;
	  		
			if oldloadpcrreq = '1' and olderloadpcrreq ='1' then
 		   	unsignedmode <= preunsignedmode;
				lacedpwm <= prelacedpwm;
				localenable <= prelocalenable;			
			end if;
			olderloadpwmreq <= oldloadpwmreq;
			olderloadpcrreq <= oldloadpcrreq;
			oldloadpwmreq <= loadpwmreq;
			oldloadpcrreq <= loadpcrreq;			
			if unsignedmode = '0' then
				if dir = '1' then
--					fixedpwmval <= (not pwmval) +1;		-- convert from 2s comp to magnitude
--					suffers from overflow problem 
					fixedpwmval <= (not pwmval);		   -- convert from 2s comp to magnitude
--					suffers from discontinuity at 0 but thats more palatable than overflow
				else
					fixedpwmval <= pwmval;							
				end if;
				dirio <= dir;				
			else -- unsigned mode
				fixedpwmval <= pwmval;	
				dirio <= 'Z';
			end if;

			if lacedpwm = '1' then						-- interlaced (600 kc) output for analog
				fixedrefcount(9) <= refcount(4);
				fixedrefcount(8) <= refcount(5);
				fixedrefcount(7) <= refcount(6);
				fixedrefcount(6) <= refcount(7);
				fixedrefcount(5) <= refcount(8);	
				fixedrefcount(4) <= refcount(9);
				fixedrefcount(3 downto 0) <= refcount(3 downto 0);
			else
				fixedrefcount <= refcount;
			end if;			
			
			if (UNSIGNED(fixedrefcount) < UNSIGNED(fixedpwmval)) and (enablein = '1') and (localenable = '1') then 
				pwm <= '1'; 
			else 
				pwm <= '0';
			end if;
			pwmout <= pwm;
		end if; -- hclk	


		if clk'event and clk = '1' then -- 33 mhz local bus clock			
			if loadpwmval = '1' then			 
 		   	if unsignedmode = '0' then	-- signed mode
					prepwmval <= ibus(14 downto 5);
					predir <= ibus(15);
				else								-- unsigned mode
					prepwmval <= ibus(15 downto 6);
				end if;
				loadpwmreq <= '1';
			end if;	
			if pcrloadcmd = '1' then
 		   	preunsignedmode <= ibus(2);
				prelacedpwm <= ibus(1);
				prelocalenable <= ibus(0);
				loadpcrreq <= '1';
			end if;	
		end if; -- clk

		if olderloadpwmreq = '1' then -- asyncronous request clear
			loadpwmreq <= '0';
		end if;		

		if olderloadpcrreq = '1' then -- asyncronous request clear
			loadpcrreq <= '0';
		end if;		

		if clearpwmval = '1' then  -- asynchronous reset
	 		pwmval <= "0000000000";
			localenable <= '0';
		end if;

		if readpwmval = '1' and pcrreadcmd = '0' then
			if unsignedmode = '0' then
				obus(14 downto 5) <= pwmval;
				obus(15) <= dir;
				obus(4 downto 0) <= "00000";
			else
				obus(15 downto 6) <= pwmval;
				obus(5 downto 0) <= "000000";
			end if;		
		elsif readpwmval = '0' and pcrreadcmd = '1' then
			obus(5) <= clearpwmval;
			obus(4) <= dirio;
			obus(3) <= pwm;
			obus(2) <= unsignedmode;
			obus(1) <= lacedpwm;
			obus(0) <= localenable;
			obus(15 downto 5) <="00000000000";
		else
			obus <= "ZZZZZZZZZZZZZZZZ";
		end if;					
		enableout <= not localenable;
	end process;
end behavioral;

