-- Only XST supports RAM inference
-- Infers Single Port Block Ram 
library IEEE;
use IEEE.STD_LOGIC_1164.all;  -- defines std_logic types
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

package configram_pkg is
component configram is
    port (
	clock : in std_logic;
	dout  : out std_logic_vector ( 31 downto 0 );
	din   : in std_logic_vector ( 31 downto 0 );
	-- note: address is latched on the clock before the actual transfer
	addr  : in std_logic_vector(7 downto 0);
	ce    : in std_logic;
	wr    : in std_logic;
	rd    : in std_logic
    );
end component configram;
end configram_pkg;


library IEEE;
use IEEE.STD_LOGIC_1164.all;  -- defines std_logic types
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity configram is
    port (
	clock : in std_logic;
	dout  : out std_logic_vector ( 31 downto 0 );
	din   : in std_logic_vector ( 31 downto 0 );
	addr  : in std_logic_vector(7 downto 0);
	ce    : in std_logic;
	wr    : in std_logic;
	rd    : in std_logic
    );
end configram;

architecture syn of configram is -- 256 x 32 synchronous ram
	-- local signals
	type ramtype is array (0 to 255) of std_logic_vector (31 downto 0);
	signal RAM : ramtype := (others => (others => '1'));
	signal rd_addr : std_logic_vector(7 downto 0);
	signal wr_addr : std_logic_vector(7 downto 0);
 
begin 
	process (clock, wr, ce, rd, addr, din, RAM) 
	begin 
	    -- combinatorial stuff
		dout <= (others => 'Z');
		if rd = '1' and ce = '1' then
			dout <= RAM(conv_integer(rd_addr));
		end if;
	    -- clocked stuff
	    if (clock'event and clock = '1') then
		if wr = '1' and ce = '1' then
		    RAM(conv_integer(wr_addr)) <= din;
		end if;
		rd_addr <= addr;
		wr_addr <= addr;
	    end if; 
	end process; 
end;


