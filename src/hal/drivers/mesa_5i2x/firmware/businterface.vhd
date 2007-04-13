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
