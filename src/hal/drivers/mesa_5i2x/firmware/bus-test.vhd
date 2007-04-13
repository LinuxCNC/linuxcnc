library IEEE;
use IEEE.std_logic_1164.all;  -- defines std_logic types
use IEEE.std_logic_ARITH.all;
use IEEE.std_logic_UNSIGNED.all;

use std.textio.all; --  Imports the standard textio package.

use work.businterface_pkg.all;

entity Tester is
    -- no ports on a testbed
end Tester;

architecture behavioral of Tester is
	-- misc global signals --
	signal lad : std_logic_vector ( 31 downto 0 );
	signal ads : std_logic;
	signal wr_rd : std_logic;
	signal blast : std_logic;
	signal ready : std_logic;
	signal interrupt : std_logic;

	signal rd_bus : std_logic_vector ( 31 downto 0 );
	signal wr_bus : std_logic_vector ( 31 downto 0 );
	signal addr : std_logic_vector ( 15 downto 0 );
	signal read_strb  : std_logic;
	signal write_strb : std_logic;
	signal next_addr : std_logic_vector ( 15 downto 0 );
	signal next_read : std_logic;
	signal next_write : std_logic;

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
		blast <= '1';
		ads <= '1';
		wr_rd <= '1';
		lad <= (others => '0' );
		wait until cycles = 3 ; wait for 5 ns;
		ads <= '0';
		wait for 3 ns;
		lad <= x"12345678";
		wait until cycles = 4 ; wait for 5 ns;
		ads <= '1';
		blast <= '0';
		wait for 3 ns;
		lad <= x"87654321";
		wait until cycles = 5 ; wait for 5 ns;
		blast <= '1';
		wait for 3 ns;
		lad <= x"00000000";
		wait until cycles = 8 ; wait for 5 ns;
		ads <= '0';
		wait for 3 ns;
		lad <= x"12345677";
		wait until cycles = 9 ; wait for 5 ns;
		ads <= '1';
		wait for 3 ns;
		lad <= x"87654321";
		wait until cycles = 10 ; wait for 5 ns;
		blast <= '0';
		wait for 3 ns;
		lad <= x"87654322";
		wait until cycles = 11 ; wait for 5 ns;
		blast <= '1';
		wait for 3 ns;
		lad <= x"00000000";

		wait until cycles = 15 ; wait for 5 ns;
		wait for 60 ns;
		done <= 1;
		wait;
	end process;

    test: businterface
	port map (
		clock => clock,
		rd_bus => rd_bus,
		wr_bus => wr_bus,
		addr => addr,
		read => read_strb,
		write => write_strb,
		next_addr => next_addr,
		next_read => next_read,
		next_write => next_write,
		LAD => lad,
		ADS => ads,
		LW_R => wr_rd,
		BLAST => blast,
		READY => ready,
		INT => interrupt
	);
end behavioral;
