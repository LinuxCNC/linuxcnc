library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;


entity fcounter is
   port ( 
	obus: out STD_LOGIC_VECTOR (31 downto 0);
	startgate: in STD_LOGIC;
	readcmd: in STD_LOGIC;
	ref: in STD_LOGIC;	-- local bus clock
	clk: in STD_LOGIC;   -- reference clock
	gateout: out STD_LOGIC
	);
end fcounter;

architecture behavioral of fcounter is

signal count: STD_LOGIC_VECTOR (23 downto 0);
signal gate : STD_LOGIC;
signal pregate : STD_LOGIC;
signal gatereq : STD_LOGIC;
signal gatecounter : STD_LOGIC_VECTOR (23 downto 0);


begin
	acounter: process (ref, clk, readcmd, count, gate) 
	 							
	begin
		if ref'event and ref = '1' then							
		
			if gatecounter /= 0 then 
			  gatecounter <= gatecounter -1;
			end if;
	
			if pregate = '1' and gate = '0' then 			 
			  gatecounter <= x"4C4640";	 -- 5 million = .1 second gate
			end if;

			
			if gate = '1' then 
				pregate <= '0';
			elsif gatereq = '1' and gate = '0' then
			   pregate <= '1';
			end if;
			
			if gatecounter /= 0 then 
           gate <= '1';
			else
			  gate <= '0';
			end if;
		
		end if; --(reference clock edge)
		
		if clk'event and clk = '1' then				
		  if startgate = '1' then
		     count <= x"000000";
			  gatereq <= '1';
		  end if;
		  if gate = '1' then
		    count <= count +1;
			 gatereq <= '0';
		  end if;
		end if; -- local bus clock edge
			  	
		obus <= "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ";
		if readcmd = '1' then
			obus(23 downto 0) <= count;
			obus(30 downto 24) <= "0000000";
			obus(31) <= gate;
		end if;	
		gateout <= gate;	
	end process;
end behavioral;
