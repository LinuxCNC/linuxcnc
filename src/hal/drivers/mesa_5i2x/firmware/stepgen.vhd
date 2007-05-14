-- Notes
-- With the default generic values, 'stepgen_core'
-- uses 93 FFs and 121 LUTs.  Increasing 'timer_size'
-- adds about 3.3 FFs and 3 LUTs per bit.  Decreasing
-- 'timer_size' from 12 to 11 _adds_ about 40 LUTs.
-- Increasing 'acc_size' costs one FF and one LUT per
-- bit, while increasing 'rate_sze' costs 1 LUT per bit.
-- Changing 'pos_size' has no effect.

-- These numbers apply to the core only.  Since the rates
-- and time values need to be written to registers by the
-- PC, those registers will use one FF per bit.  Likewise,
-- the position feedback will use LUTs to drive the bus.

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.STD_LOGIC_SIGNED.ALL;

package stepgen_pkg is

component stepgen_core is
    generic (
	-- size of accumulator
	acc_size : integer := 48;
	-- size of position feedback
	pos_size : integer := 32;
	-- size (resolution) of rate value
	rate_size : integer := 24;
	-- size of length, hold, and setup timers
	timer_size : integer := 12
    );
    port (
	clock : in std_logic;
	enable : in std_logic;
	position : buffer std_logic_vector ( pos_size-1 downto 0);
	rate : in std_logic_vector ( rate_size-1 downto 0 );
	steplen : in std_logic_vector ( timer_size-1 downto 0 );
	holdtime : in std_logic_vector ( timer_size-1 downto 0 );
	setuptime : in std_logic_vector ( timer_size-1 downto 0 );
	stepout : buffer std_logic;
	dirout : buffer std_logic;
	stepup : buffer std_logic;
	stepdown : buffer std_logic;
	phasea : buffer std_logic;
	phaseb : buffer std_logic
    );
end component stepgen_core;

component stepgen is
    port (
	clock : in std_logic;
	enable : in std_logic;
	ibus : in std_logic_vector (31 downto 0);
	obus : out std_logic_vector (31 downto 0);
	sel : in std_logic;
	write : in std_logic;
	read : in std_logic;
	addr : in std_logic_vector (1 downto 0 );
	out0 : out std_logic;
	out1 : out std_logic
    );
end component stepgen;

end stepgen_pkg;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.STD_LOGIC_SIGNED.ALL;

use work.oneshot_pkg.all;

-- this is the core of a step generator.
-- it is completely configurable, and contains no registers
-- all of its control signals are provided through ports
-- see 'stepgen' below for a instance of a stepgen
-- with a bus interface and control registers

entity stepgen_core is
    generic (
	acc_size : integer := 48;
	pos_size : integer := 32;
	rate_size : integer := 24;
	timer_size : integer := 12
    );
    port (
	clock : in std_logic;
	enable : in std_logic;
	position : buffer std_logic_vector ( pos_size-1 downto 0);
	rate : in std_logic_vector ( rate_size-1 downto 0 );
	steplen : in std_logic_vector ( timer_size-1 downto 0 );
	holdtime : in std_logic_vector ( timer_size-1 downto 0 );
	setuptime : in std_logic_vector ( timer_size-1 downto 0 );
	stepout : buffer std_logic := '0';
	dirout : buffer std_logic := '0';
	stepup : buffer std_logic := '0';
	stepdown : buffer std_logic := '0';
	phasea : buffer std_logic := '0';
	phaseb : buffer std_logic := '0'
    );
end stepgen_core;

architecture behavioral of stepgen_core is

    constant pickoff_bit: integer := rate_size + 1;

    signal accum: std_logic_vector ( acc_size-1 downto 0 ) := (others => '0');
    alias pickoff: std_logic is accum(pickoff_bit);
    signal nextaccum: std_logic_vector ( acc_size-1 downto 0 );
    alias nextpickoff: std_logic is nextaccum(pickoff_bit);
    alias dirreq: std_logic is rate(rate_size-1);
    signal ddshold: std_logic;
    signal stepreq : std_logic := '0';
    signal dirhold: std_logic;
    signal dirsetup: std_logic;
    signal starthold: std_logic;
    signal dirchange: std_logic;

begin
    StepDDS:
	process (clock, ddshold, rate, accum, nextaccum, enable)
	begin
	    -- combinatorial stuff
		nextaccum <= signed(accum) + signed(rate);
		position <= accum(acc_size-1 downto acc_size-pos_size);
	    -- clocked stuff
	    if clock'event and clock = '1' then
		if ddshold = '0' and enable = '1' then
		    accum <= nextaccum;
		    stepreq <= pickoff xor nextpickoff;
		else
		    stepreq <= '0';
		end if;
	    end if;

	end process;

    Timing:
	process (clock, stepreq, stepout, rate, dirreq, dirout, dirchange, dirsetup, dirhold)
	begin
	    -- combinatorial
		starthold <= stepreq or stepout;
		dirchange <= dirreq xor dirout;
		ddshold <= dirchange or dirsetup;
	    -- clocked
	    if clock'event and clock = '1' then
		if dirhold = '0' and stepreq = '0' then
		    dirout <= dirreq;
		end if;
	    end if;
	end process;

    Output:
	process (clock, stepreq, stepout, dirout, phasea, phaseb)
	begin
	    -- conbinatorial
		stepup <= stepout and not dirout;
		stepdown <= stepout and dirout;
	    -- clocked
	    if clock'event and clock = '1' then
		if stepreq = '1' then
		    phasea <= phaseb xor dirout;
		    phaseb <= phasea xor not dirout;
		end if;
	    end if;
	end process;

    StepTimer: oneshot
	generic map ( timer_size )
	port map (
		input => stepreq,
		output => stepout,
		clock => clock,
		duration => steplen
	);

    HoldTimer: oneshot
	generic map ( timer_size )
	port map (
		input => starthold,
		output => dirhold,
		clock => clock,
		duration => holdtime
	);

    SetupTimer: oneshot
	generic map ( timer_size )
	port map (
		input => dirchange,
		output => dirsetup,
		clock => clock,
		duration => setuptime
	);

end behavioral;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.STD_LOGIC_SIGNED.ALL;

use work.stepgen_pkg.all;

-- this entity provides a step generator core along with
-- a bus interface and registers to allow the PC to control it

-- write registers:
--  00: 00-23 = rate (24 bits)
--  01: 00-13 = step length (14 bits)
--      16-17 = mode (00-off, 01-step/dir, 02-up/down, 03-quadrature)
--      18    = enable (0-off, 1-on)
--  02: 00-13 = direction hold time (14 bits)
--      16-29 = direction setup time (14 bits)

-- read registers:
--  00: 00-07 = position (fractional steps)
--      08-31 = position (whole steps)

entity stepgen is
    port (
	clock : in std_logic;
	enable : in std_logic;
	ibus : in std_logic_vector (31 downto 0);
	obus : out std_logic_vector (31 downto 0);
	sel : in std_logic;
	write : in std_logic;
	read : in std_logic;
	addr : in std_logic_vector (1 downto 0 );
	out0 : out std_logic;
	out1 : out std_logic
    );
end stepgen;

architecture behavioral of stepgen is

    signal wr_reg0: std_logic_vector ( 31 downto 0 ) := (others => '0');
    signal wr_reg1: std_logic_vector ( 31 downto 0 ) := (others => '0');
    signal wr_reg2: std_logic_vector ( 31 downto 0 ) := (others => '0');
    signal rd_reg0: std_logic_vector ( 31 downto 0 );
    signal wr0: std_logic;
    signal wr1: std_logic;
    signal wr2: std_logic;
    signal rd0: std_logic;
    signal stepout : std_logic;
    signal dirout : std_logic;
    signal stepup : std_logic;
    signal stepdown : std_logic;
    signal phasea : std_logic;
    signal phaseb : std_logic;
    signal local_enable : std_logic;

begin
    BusInterface:
	process (clock, sel, read, write, addr, ibus, rd_reg0, enable, wr_reg1)
	begin
	    -- combinatorial stuff
		-- decoding
		wr0 <= sel and write and not addr(1) and not addr(0);
		wr1 <= sel and write and not addr(1) and     addr(0);
		wr2 <= sel and write and     addr(1) and not addr(0);
		rd0 <= sel and read  and not addr(1) and not addr(0);
		-- reading
		obus <= (others => 'Z');
		if rd0 = '1' then
		    obus <= rd_reg0;
		end if;
		-- enable = global enable and enable bit in mode register
		local_enable <= enable and wr_reg1(18);
	    -- clocked stuff
	    if clock'event and clock = '1' then
		-- writing
		if wr0 = '1' then
		    wr_reg0 <= ibus;
		end if;
		if wr1 = '1' then
		    wr_reg1 <= ibus;
		end if;
		if wr2 = '1' then
		    wr_reg2 <= ibus;
		end if;
	    end if;
	end process;

    ModeLogic:
	process (stepout, dirout, stepup, stepdown, phasea, phaseb, wr_reg1)
	begin
	    -- combinatorial stuff
	    case wr_reg1(17 downto 16) is
		when "00" => out0 <= '0' ; out1 <= '0';
		when "01" => out0 <= stepout ; out1 <= dirout;
		when "10" => out0 <= stepup ; out1 <= stepdown;
		when "11" => out0 <= phasea ; out1 <= phaseb;
		when others => out0 <= '0' ; out1 <= '0';
	    end case;
	end process;


   Core: stepgen_core
	generic map (
		acc_size => 48,
		pos_size => 32,
		rate_size => 24,
		timer_size => 14
	)
	port map (
		stepout => stepout,
		dirout => dirout,
		stepup => stepup,
		stepdown => stepdown,
		phasea => phasea,
		phaseb => phaseb,
		rate => wr_reg0(23 downto 0),
		steplen   => wr_reg1(13 downto 0),
		holdtime => wr_reg2(13 downto 0),
		setuptime => wr_reg2(29 downto 16),
		position => rd_reg0(31 downto 0),
		enable => local_enable,
		clock => clock
	);

end behavioral;
