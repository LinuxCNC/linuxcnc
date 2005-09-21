library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity pwmgen is
	port (
	clk: in STD_LOGIC;
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
end pwmgen;

architecture behavioral of pwmgen is

signal pwmval: STD_LOGIC_VECTOR (9 downto 0);
signal fixedpwmval: STD_LOGIC_VECTOR (9 downto 0);
signal fixedrefcount: STD_LOGIC_VECTOR (9 downto 0);
signal dir: STD_LOGIC;
signal pwm: STD_LOGIC;
signal unsignedmode: STD_LOGIC;
signal localenable: STD_LOGIC;
signal lacedpwm: STD_LOGIC;


begin
	apwmgen: process  (clk, 
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
		if clk'event and clk = '1' then
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
			if loadpwmval = '1' then 
 		   	if unsignedmode = '0' then
					pwmval <= ibus(14 downto 5);
					dir <= ibus(15);
				else
					pwmval <= ibus(15 downto 6);
				end if;
			end if;	
			if pcrloadcmd = '1' then
 		   	unsignedmode <= ibus(2);
				lacedpwm <= ibus(1);
				localenable <= ibus(0);
			end if;	
		end if;
		if clearpwmval = '1' then
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

