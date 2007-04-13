-- This is a top level design used to determine resource usage
-- of entities that expect to connect to the PC bus.  It contains
-- a bus interface.  Use it by editing the architecture to 
-- instantiate whatever you want to measure, connecting the
-- entity bus ports to rd_bus, wr_bus, etc, and the entity I/O
-- ports to IOBITS and/or LEDS as appropriate (to prevent
-- unconnected logic from being optimized away)

-- the following lines are for the make system
-- they must start with three dashes or they will be ignored
--- device 2s200pq208-5
--- constraints 5i20-normal.ucf

library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.ALL;
use IEEE.std_logic_UNSIGNED.ALL;

use work.businterface_pkg.all;

entity size_check is
    port (
	-- local bus clock
	LCLK: in std_logic;
	-- local bus control signals --
	LW_R: in std_logic; 
	ADS: in std_logic; 
	BLAST: in std_logic; 
	READY: out std_logic; 
	INT: out std_logic;
	-- local address/data bus
	LAD: inout std_logic_vector (31 downto 0);
	-- the three 24 bit user pin ports
	IOBits: inout std_logic_vector (71 downto 0);
	-- led bits
	LEDS: inout std_logic_vector(7 downto 0)
    );
end size_check;

architecture behavioral of size_check is
	signal rd_bus : std_logic_vector ( 31 downto 0 );
	signal wr_bus : std_logic_vector ( 31 downto 0 );
	signal addr : std_logic_vector ( 15 downto 0 );
	signal read : std_logic;
	signal write : std_logic;
	signal next_addr : std_logic_vector ( 15 downto 0 );
	signal next_read : std_logic;
	signal next_write : std_logic;

	alias dataport: std_logic_vector (31 downto 0) is IOBits(31 downto 0);
	alias address: std_logic_vector (15 downto 0) is IOBits(47 downto 32);
	alias naddress: std_logic_vector (15 downto 0) is IOBits(63 downto 48);
	alias oread: std_logic is IOBits(64);
	alias onread: std_logic is IOBits(65);
	alias owrite: std_logic is IOBits(66);
	alias onwrite: std_logic is IOBits(67);
--	alias phb: std_logic is LEDS(5);
	signal datareg: std_logic_vector (31 downto 0) := (others => '0');

begin

    busif: businterface
	port map (
		LAD => LAD,
		ADS => ADS,
		LW_R => LW_R,
		BLAST => BLAST,
		READY => READY,
		INT => int,
		rd_bus => rd_bus,
		wr_bus => wr_bus,
		addr => addr,
		read => read,
		write => write,
		next_addr => next_addr,
		next_read => next_read,
		next_write => next_write,
		clock => LCLK
	);

    testregister:
	process (LCLK, read, write, wr_bus, datareg)
	begin
	    -- conbinatorial
		rd_bus <= (others => 'Z');
		if read = '1'  then
		    rd_bus <= datareg;
		end if;
		dataport <= datareg;
	    -- clocked
	    if LCLK'event and LCLK = '1' then
		if write = '1' then
		    datareg <= wr_bus;
		end if;
	    end if;
	end process;

    copytopins:
	process (read, next_read, write, next_write, addr, next_addr)
	begin
		address <= addr;
		naddress <= next_addr;
		oread <= read;
		onread <= next_read;
		owrite <= write;
		onwrite <= next_write;
	end process;

end behavioral;