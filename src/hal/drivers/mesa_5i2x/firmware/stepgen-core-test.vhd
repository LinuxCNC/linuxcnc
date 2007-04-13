library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

use std.textio.all; --  Imports the standard textio package.

use work.stepgen_pkg.all;
entity Tester is
    -- no ports on a testbed
end Tester;

architecture behavioral of Tester is
	-- misc global signals --
	signal pos: std_logic_vector ( 7 downto 0 );
	signal rate: std_logic_vector ( 11 downto 0 );
	signal step: std_logic;
	signal dir: std_logic;
	signal up: std_logic;
	signal down: std_logic;
	signal pha: std_logic;
	signal phb: std_logic;
	signal clock: std_logic;
	signal cycles : integer;
	signal done: integer := 0;

begin
    ToggleClock: process
	variable l : line;
	begin
		write (l, String'("Start"));
		writeline (output, l);
		cycles <= 0;
		clock <= '0';
		wait for 10 ns;
		while done = 0 loop
			wait for 10 ns;
			clock <= '1';
			cycles <= cycles + 1;
			wait for 10 ns;
			clock <= '0';
		end loop;
		write (l, String'("End"));
		writeline (output, l);
		assert false report "end of loop" severity note;
		wait;
	end process;

    RunTest: process
	begin
		rate <= "000000000000";
		wait until cycles = 3 ; wait for 5 ns;
		rate <= "010000000000";
		wait until cycles = 27 ; wait for 5 ns;
		rate <= "110000000000";
		wait until cycles = 90 ; wait for 5 ns;
		rate <= "011111100000";
		wait until cycles = 130 ; wait for 5 ns;
		wait for 60 ns;
		done <= 1;
		wait;
	end process;

    sg1: stepgen_core
	generic map (
		acc_size => 16,
		pos_size => 8,
		rate_size => 12,
		timer_size => 8
	)
	port map (
		stepout => step,
		dirout => dir,
		stepup => up,
		stepdown => down,
		phasea => pha,
		phaseb => phb,
		rate => rate,
		steplen   => "00000001",
		holdtime  => "00000000",
		setuptime => "00000000",
		position => pos,
		enable => '1',
		clock => clock
	);

end behavioral;
