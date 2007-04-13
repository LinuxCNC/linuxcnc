-- This is a top level design used to determine resource usage.
-- Use it by editing the architecture to instantiate whatever
-- you want to measure.  Connect the entity ports to IOBITS
-- and/or LEDS as appropriate (to prevent unconnected logic
-- from being optimized away).

-- the following lines are for the make system
-- they must start with three dashes or they will be ignored
--- device 2s200pq208-5
--- constraints 5i20-nobus.ucf

library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.ALL;
use IEEE.std_logic_UNSIGNED.ALL;

use work.stepgen_pkg.all;

entity size_check_nobus is
    port (
	-- clocks
	CLK33: in std_logic;
	CLK50: in std_logic;
	-- the three 24 bit user pin ports
	IOBits: inout std_logic_vector (71 downto 0);
	LEDS: inout std_logic_vector (7 downto 0)
    );
end size_check_nobus;

architecture behavioral of size_check_nobus is
	alias posfb: std_logic_vector (31 downto 0) is IOBits(31 downto 0);
	alias rate: std_logic_vector (23 downto 0) is IOBits(55 downto 32);
	alias len: std_logic_vector (11 downto 0) is IOBits(67 downto 56);
	alias hold: std_logic_vector (11 downto 0) is IOBits(67 downto 56);
	alias setup: std_logic_vector (11 downto 0) is IOBits(67 downto 56);
	alias step: std_logic is LEDS(0);
	alias dir: std_logic is LEDS(1);
	alias up: std_logic is LEDS(2);
	alias down: std_logic is LEDS(3);
	alias pha: std_logic is LEDS(4);
	alias phb: std_logic is LEDS(5);
	alias enable: std_logic is IOBits(68);

begin

    sg1: stepgen_core
	generic map (
		acc_size => 48,
		pos_size => 32,
		rate_size => 24,
		timer_size => 12
	)
	port map (
		stepout => step,
		dirout => dir,
		stepup => up,
		stepdown => down,
		phasea => pha,
		phaseb => phb,
		rate => rate,
		steplen   => len,
		holdtime => hold,
		setuptime => setup,
		position => posfb,
		enable => enable,
		clock => CLK33
	);

end behavioral;