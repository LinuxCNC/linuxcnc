library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

use std.textio.all; --  Imports the standard textio package.

use work.oneshot_pkg.all;

entity Tester is
    -- no ports on a testbed
end Tester;

architecture behavioral of Tester is
	-- misc global signals --
	signal len: std_logic_vector ( 11 downto 0 );
	signal start: std_logic;
	signal pulse: std_logic;
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
		start <= '0';
		len <= "000000000000";
		wait until cycles = 3 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 4 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 6 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 8 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 10 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 13 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 15 ; wait for 5 ns;
		len <= "000000000001";
		wait until cycles = 16 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 17 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 19 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 21 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 24 ; wait for 5 ns;
		len <= "000000000010";
		wait until cycles = 25 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 26 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 29 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 32 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 36 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 37 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 39 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 40 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 41 ; wait for 5 ns;
		len <= "000000000100";
		wait until cycles = 42 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 45 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 49 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 50 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 57 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 58 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 61 ; wait for 5 ns;
		start <= '1';
		wait until cycles = 62 ; wait for 5 ns;
		start <= '0';
		wait until cycles = 70 ; wait for 5 ns;
		wait for 60 ns;
		done <= 1;
		wait;
	end process;

    os1: oneshot
	generic map ( 12 )
	port map (
		input => start,
		output => pulse,
		clock => clock,
		duration => len
	);
end behavioral;
