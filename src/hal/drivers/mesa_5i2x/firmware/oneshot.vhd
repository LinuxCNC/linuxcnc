-- retriggerable one-shot
--   'output' goes hi on a rising clock edge if 'input' is
--   hi, and goes low 'duration' rising clock edges after
--   the last edge during which 'input' was high.
--   If 'duration' is zero, 'output' never goes high.
--   'size' is the length of 'duration' in bits, minimum
--   value for 'size' is 2.

-- Notes (specific to the 5i20 FPGA)
--   For size >= 12, it uses (size + 1) FFs and (size + 1) LUTs
--   For size < 12, it uses (size + 1) FFs and significantly
--    more LUTs (up to 42 for 11 bits), apparently in an attempt
--   to improve speed.  However, even a 32 bit one has an unrouted
--   delay of under 15nS.  Since all the large ones use the fast
--   carry chain, routing will have little effect on speed.


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

package oneshot_pkg is
component oneshot is
    generic (
	-- width of timer
	size: integer := 8
    );
    port (
	duration: in std_logic_vector (size-1 downto 0);
	input: in std_logic;
	output: buffer std_logic;
	clock: in std_logic
    );
end component oneshot;
end oneshot_pkg;


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity oneshot is
    generic ( size: integer := 2 );
    port (
	duration: in std_logic_vector (size-1 downto 0);
	input: in std_logic;
	output: buffer std_logic := '0';
	clock: in std_logic
    );
end oneshot;

architecture behavioral of oneshot is
    -- internal signals
    signal count: std_logic_vector ( size-1 downto 0) := (others => '0');
    signal muxout: std_logic_vector ( size downto 0);
    alias  muxout_lo: std_logic_vector (size-1 downto 0) is muxout (size-1 downto 0);
    alias  muxout_hi: std_logic is muxout(size);
    signal nextcount: std_logic_vector ( size downto 0);
    alias  nextcount_lo: std_logic_vector (size-1 downto 0) is nextcount (size-1 downto 0);
    alias  nextcount_hi: std_logic is nextcount(size);

begin
    OneShot:
	process (clock, input, duration, count, muxout, nextcount)
	begin
	    -- combinatorial
		if input = '1'  then
		    muxout_lo <= duration;
		else
		    muxout_lo <= count;
		end if;
		muxout_hi <= '0';
		nextcount <= muxout - 1;
	    -- clocked
	    if clock'event and clock = '1' then
		if nextcount_hi /= '1' then
		    count <= nextcount_lo;
		end if;
		output <= not nextcount_hi;
	    end if;
	end process;

end behavioral;
