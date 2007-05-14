library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

package businterface_pkg is

component businterface is
    port (
	clock : in std_logic;
	-- internal FPGA data buses
	rd_bus : in std_logic_vector ( 31 downto 0 );
	wr_bus : out std_logic_vector ( 31 downto 0 );
	-- addr, read, and write describe the current cycle.  Data will be
	-- transferred (and these signals change ) on the next rising edge.
	addr : buffer std_logic_vector ( 15 downto 0 ) := (others => '0');
	read  : buffer std_logic := '0';
	write : buffer std_logic := '0';
	-- The "next" signals describe the upcoming cycle.  They allow
	-- address decode logic to be pipelined for better speed.  They
	-- are derived combinatorially from PLX9030 signals.
	next_addr : buffer std_logic_vector ( 15 downto 0 );
	next_read : buffer std_logic;
	next_write : buffer std_logic;
	-- interface to 9030 PCI bridge chip
	-- these signals connect to pads and the world outside the FPGA
	LAD : inout std_logic_vector ( 31 downto 0 );
	ADS : in std_logic;
	LW_R : in std_logic;
	BLAST : in std_logic;
	READY : out std_logic;
	INT : out std_logic
    );
end component businterface;

component match16 is
    generic (
	lobit : integer := 2;
	hibit : integer := 15
    );
    port (
	ena : in std_logic;
	value : in std_logic_vector(15 downto 0);
	addr : in std_logic_vector(15 downto 0);
	matched : out std_logic
    );
end component match16;

component decode2 is
    port (
	ena : in std_logic;
	addr : in std_logic;
	sel : out std_logic_vector ( 1 downto 0 )
    );
end component decode2;

component decode4 is
    port (
	ena : in std_logic;
	addr : in std_logic_vector ( 1 downto 0 );
	sel : out std_logic_vector ( 3 downto 0 )
    );
end component decode4;

component decode8 is
    port (
	ena : in std_logic;
	addr : in std_logic_vector ( 2 downto 0 );
	sel : out std_logic_vector ( 7 downto 0 )
    );
end component decode8;

component decode16 is
    port (
	ena : in std_logic;
	addr : in std_logic_vector ( 3 downto 0 );
	sel : out std_logic_vector ( 15 downto 0 )
    );
end component decode16;

end businterface_pkg;


library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

entity businterface is
    port (
	clock : in std_logic;
	rd_bus : in std_logic_vector ( 31 downto 0 );
	wr_bus : out std_logic_vector ( 31 downto 0 );
	addr : buffer std_logic_vector ( 15 downto 0 ) := (others => '0');
	read  : buffer std_logic := '0';
	write : buffer std_logic := '0';
	next_addr : buffer std_logic_vector ( 15 downto 0 );
	next_read : buffer std_logic;
	next_write : buffer std_logic;
	LAD : inout std_logic_vector ( 31 downto 0 );
	ADS : in std_logic;
	LW_R : in std_logic;
	BLAST : in std_logic;
	READY : out std_logic;
	INT : out std_logic
    );
end businterface;

architecture behavioral of businterface is
	-- local signals
	signal addr_clken : std_logic;
	signal data_cycle : std_logic := '0';

begin
    AddrLatch:
	process (clock, ADS, BLAST, LAD, addr, addr_clken, data_cycle)
	begin
	    -- combinatorial stuff
		next_addr(1 downto 0) <= "00"; -- data is 32-bit word aligned
		if ADS = '0' then
		    next_addr(15 downto 2) <= LAD(15 downto 2);
		else
		    next_addr(15 downto 2) <= addr(15 downto 2) + 1;
		end if;
		-- only change address if needed
		addr_clken <= ( data_cycle or not ADS ) and BLAST;
	    -- clocked stuff
	    if clock'event and clock = '1' then
		data_cycle <= addr_clken;
		if addr_clken = '1' then
		    addr <= next_addr;
		end if;
	    end if;
	end process;

    Strobes:
	process (clock, addr_clken, LW_R, next_read, next_write)
	begin
	    -- combinatorial stuff
		next_read <= addr_clken and not LW_R;
		next_write <= addr_clken and LW_R;
		READY <= '0';  -- no wait states
		INT <= '1';    -- no interrupts
	    -- clocked stuff
	    if clock'event and clock = '1' then
		read <= next_read;
		write <= next_write;
	    end if;
	end process;

    DataBus:
	process (read, rd_bus, LAD)
	begin
	    -- combinatorial stuff
		if read = '1' then
		    LAD <= rd_bus;
		else
		    LAD <= (others => 'Z');
		end if;
		wr_bus <= LAD;
	end process;

end behavioral;

library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

entity match16 is
    generic (
	lobit : integer := 2;
	hibit : integer := 15
    );
    port (
	ena : in std_logic;
	value : in std_logic_vector(15 downto 0);
	addr : in std_logic_vector(15 downto 0);
	matched : out std_logic
    );
end match16;

architecture behavioral of match16 is
begin
    process (ena, addr, value)
	begin
	-- combinatorial stuff
	    if addr(hibit downto lobit) = value(hibit downto lobit) then
		matched <= ena;
	    else
		matched <= '0';
	    end if;
	end process;
end behavioral;

library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

entity decode2 is
    port (
	ena : in std_logic;
	addr : in std_logic;
	sel : out std_logic_vector ( 1 downto 0 )
    );
end decode2;

architecture behavioral of decode2 is
begin
    process (ena, addr)
	begin
	-- combinatorial stuff
	    sel <= (others => '0');
	    if ena = '1' then
		if addr = '0' then sel(0) <= '1'; end if;
		if addr = '1' then sel(1) <= '1'; end if;
	    end if;
	end process;
end behavioral;

library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

entity decode4 is
    port (
	ena : in std_logic;
	addr : in std_logic_vector ( 1 downto 0 );
	sel : out std_logic_vector ( 3 downto 0 )
    );
end decode4;

architecture behavioral of decode4 is
begin
    process (ena, addr)
	begin
	-- combinatorial stuff
	    sel <= (others => '0');
	    if ena = '1' then
		if addr = "00" then sel(0) <= '1'; end if;
		if addr = "01" then sel(1) <= '1'; end if;
		if addr = "10" then sel(2) <= '1'; end if;
		if addr = "11" then sel(3) <= '1'; end if;
	    end if;
	end process;
end behavioral;

library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

entity decode8 is
    port (
	ena : in std_logic;
	addr : in std_logic_vector ( 2 downto 0 );
	sel : out std_logic_vector ( 7 downto 0 )
    );
end decode8;

architecture behavioral of decode8 is
begin
    process (ena, addr)
	begin
	-- combinatorial stuff
	    sel <= (others => '0');
	    if ena = '1' then
		if addr = "000" then sel(0) <= '1'; end if;
		if addr = "001" then sel(1) <= '1'; end if;
		if addr = "010" then sel(2) <= '1'; end if;
		if addr = "011" then sel(3) <= '1'; end if;
		if addr = "100" then sel(4) <= '1'; end if;
		if addr = "101" then sel(5) <= '1'; end if;
		if addr = "110" then sel(6) <= '1'; end if;
		if addr = "111" then sel(7) <= '1'; end if;
	    end if;
	end process;
end behavioral;

library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

entity decode16 is
    port (
	ena : in std_logic;
	addr : in std_logic_vector ( 3 downto 0 );
	sel : out std_logic_vector ( 15 downto 0 )
    );
end decode16;

architecture behavioral of decode16 is
begin
    process (ena, addr)
	begin
	-- combinatorial stuff
	    sel <= (others => '0');
	    if ena = '1' then
		if addr = "0000" then sel(0) <= '1'; end if;
		if addr = "0001" then sel(1) <= '1'; end if;
		if addr = "0010" then sel(2) <= '1'; end if;
		if addr = "0011" then sel(3) <= '1'; end if;
		if addr = "0100" then sel(4) <= '1'; end if;
		if addr = "0101" then sel(5) <= '1'; end if;
		if addr = "0110" then sel(6) <= '1'; end if;
		if addr = "0111" then sel(7) <= '1'; end if;
		if addr = "1000" then sel(8) <= '1'; end if;
		if addr = "1001" then sel(9) <= '1'; end if;
		if addr = "1010" then sel(10) <= '1'; end if;
		if addr = "1011" then sel(11) <= '1'; end if;
		if addr = "1100" then sel(12) <= '1'; end if;
		if addr = "1101" then sel(13) <= '1'; end if;
		if addr = "1110" then sel(14) <= '1'; end if;
		if addr = "1111" then sel(15) <= '1'; end if;
	    end if;
	end process;
end behavioral;

