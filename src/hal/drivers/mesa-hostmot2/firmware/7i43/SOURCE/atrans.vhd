
library IEEE;
use IEEE.STD_LOGIC_1164.all;  			
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity atrans is 
	generic (
				width : integer;
				depth: integer );
	port (
 		clk  : in std_logic; 
		wea  : in std_logic; 
		rea  : in std_logic; 
		reb  : in std_logic; 		
 		adda : in std_logic_vector(depth-1 downto 0);
		addb : in std_logic_vector(depth-1 downto 0);
 		din  : in std_logic_vector(width-1 downto 0);  
 		douta : out std_logic_vector(width-1 downto 0);
		doutb: out std_logic_vector(width-1 downto 0)
		);
end atrans; 
 
 architecture syn of atrans is 			
 type ram_type is array (0 to 2**depth-1) of std_logic_vector(width-1 downto 0); 
 signal RAM : ram_type; 


 signal dadda : std_logic_vector(depth-1 downto 0);
 signal daddb : std_logic_vector(depth-1 downto 0);
 signal outa : std_logic_vector(width-1 downto 0); 
 signal outb : std_logic_vector(width-1 downto 0); 
 
 begin 
 	process (clk) 
 	begin 
 		if (clk'event and clk = '1') then  
 			if (wea = '1') then 
 				RAM(conv_integer(adda)) <= din; 
 			end if;  
 			dadda <= adda;
			daddb <= addb;
 		end if; 
 		outa <= RAM(conv_integer(dadda)); 
 		outb <= RAM(conv_integer(daddb)); 
		douta <= (others => 'Z');
		if rea = '1' then
			douta <= outa;
		end if;
		doutb <= (others => 'Z');
		if reb = '1' then
			doutb <= outb;
		end if;
		
	end process; 
 
end;
 

