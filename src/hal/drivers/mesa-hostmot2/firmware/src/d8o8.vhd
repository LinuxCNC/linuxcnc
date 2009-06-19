library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.STD_LOGIC_ARITH.all;
use IEEE.STD_LOGIC_UNSIGNED.all;
--
-- Copyright (C) 2007, Peter C. Wallace, Mesa Electronics
-- http://www.mesanet.com
--
-- This program is is licensed under a disjunctive dual license giving you
-- the choice of one of the two following sets of free software/open source
-- licensing terms:
--
--    * GNU General Public License (GPL), version 2.0 or later
--    * 3-clause BSD License
-- 
--
-- The GNU GPL License:
-- 
--     This program is free software; you can redistribute it and/or modify
--     it under the terms of the GNU General Public License as published by
--     the Free Software Foundation; either version 2 of the License, or
--     (at your option) any later version.
-- 
--     This program is distributed in the hope that it will be useful,
--     but WITHOUT ANY WARRANTY; without even the implied warranty of
--     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--     GNU General Public License for more details.
-- 
--     You should have received a copy of the GNU General Public License
--     along with this program; if not, write to the Free Software
--     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
-- 
-- 
-- The 3-clause BSD License:
-- 
--     Redistribution and use in source and binary forms, with or without
--     modification, are permitted provided that the following conditions
--     are met:
-- 
--         * Redistributions of source code must retain the above copyright
--           notice, this list of conditions and the following disclaimer.
-- 
--         * Redistributions in binary form must reproduce the above
--           copyright notice, this list of conditions and the following
--           disclaimer in the documentation and/or other materials
--           provided with the distribution.
-- 
--         * Neither the name of Mesa Electronics nor the names of its
--           contributors may be used to endorse or promote products
--           derived from this software without specific prior written
--           permission.
-- 
-- 
-- Disclaimer:
-- 
--     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
--     "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
--     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
--     FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
--     COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
--     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
--     BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
--     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
--     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
--     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
--     ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
--     POSSIBILITY OF SUCH DAMAGE.
-- 



-------------------------------------------------------------------------------
-- Small 8 bit Harvard Arch accumulator oriented processor ~150 slices: 
-- 1 clk/inst, >120 MHz operation in Spartan3 >60 MHz in Spartan2
-- 8 bit data, 16 bit instruction width
-- 6 JMP instructions: 
-- JMP, JMPNZ, JMPZ, JMPNC, JMPC, JSR
-- 9 basic memory reference instructions:
-- OR, XOR, AND, ADD, ADDC, SUB, SUBB, LDA, STA
-- 13 operate instructions, load immediate, rotate, index load/store: 
-- LDI, RCL, RCR, LDYL, LDXL, STYL, STXL, LDYH, LDXH, STYL, STXH, RTT, TTR  
-- 1K or 2K words instruction space
-- 4K words data space
-- 2 index registers for indirect memory access
-- 8 bit offset for indirect addressing (ADD sinetable(6) etc)
-- 11 bit direct memory addressing range
-- 12 bit indirect addressing range with 8 bit offset range
-- 2 levels of subroutine call/return
-- Starts at address 0 from reset
-- THE BAD NEWS: pipelined processor with no locks so --->
-- Instruction hazards: 
--	2 instructions following conditional jumps always executed
--	TTR must precede JSR by at least 2 instructions
--	RTT must precede JSR by at least 2 instructions
--	TTR must precede JMP@R by at least 2 instructions
-- Data hazards:
--	Stored data requires 3 instructions before fetch
-- Address hazards:
--	Fetches via index register require 2 instructions from ST(X,Y) or ADDI(X,Y)
--	to actual fetch (STA via index takes no extra delay) 
-------------------------------------------------------------------------------

entity DumbAss8 is
	generic(
		width : integer := 8;			-- data width
		iwidth	: integer := 16;		-- instruction width
		maddwidth : integer := 12;		-- memory address width
		paddwidth : integer := 10		-- program counter width
		
		 );
	port (
		clk	: in  std_logic;
		reset : in  std_logic;
		iabus : out std_logic_vector(paddwidth-1 downto 0);	-- program address bus
		idbus : in  std_logic_vector(iwidth-1 downto 0);		-- program data bus		 
		mradd	: out std_logic_vector(maddwidth-1  downto 0);	-- memory read address
		mwadd	: out std_logic_vector(maddwidth-1  downto 0);	-- memory write address
		mibus	: in  std_logic_vector(width-1 downto 0);			-- memory data in bus	  
		mobus	: out std_logic_vector(width-1 downto 0);			-- memory data out bus
		mwrite: out std_logic;											-- memory write signal
		mread:  out std_logic;											-- memory read signal
		carryflg : out std_logic										-- carry flag
		);
end DumbAss8;


architecture Behavioral of DumbAss8 is


  constant carrymask : std_logic_vector := b"0_1111_1111";  -- mask to drop carry bit
  constant ProcVersion : std_logic_vector (width downto 0) := conv_std_logic_vector(12,width);
-- basic op codes
-- Beware, certain bits are used to simpilfy decoding - dont change without knowing what you are doing...
  constant opr	: std_logic_vector (3 downto 0) := x"0";
  constant jmp	: std_logic_vector (3 downto 0) := x"1";
  constant jmpnz : std_logic_vector (3 downto 0) := x"2";
  constant jmpz : std_logic_vector (3 downto 0) := x"3";
  constant jmpnc : std_logic_vector (3 downto 0) := x"4";
  constant jmpc : std_logic_vector (3 downto 0) := x"5";
  constant jsr	: std_logic_vector (3 downto 0) := x"6";
  constant lda	: std_logic_vector (3 downto 0) := x"7";
  constant lor	: std_logic_vector (3 downto 0) := x"8";
  constant lxor : std_logic_vector (3 downto 0) := x"9";
  constant land : std_logic_vector (3 downto 0) := x"A";
  constant sta	: std_logic_vector (3 downto 0) := x"B";
  constant add	: std_logic_vector (3 downto 0) := x"C";
  constant addc : std_logic_vector (3 downto 0) := x"D";
  constant sub	: std_logic_vector (3 downto 0) := x"E";
  constant subc  : std_logic_vector (3 downto 0) := x"F";

-- operate instructions
  constant nop : std_logic_vector (3 downto 0) := x"0";

-- immediate load type
  constant ldi : std_logic_vector (3 downto 0) := x"1";

-- accumulator operate type

  constant rotcl : std_logic_vector (3 downto 0) := x"2";
  constant rotcr : std_logic_vector (3 downto 0) := x"3";
  
-- index register load/store in address order
  constant ldxl	: std_logic_vector (3 downto 0) := x"4";
  constant ldyl	: std_logic_vector (3 downto 0) := x"5";
  constant ldxh	: std_logic_vector (3 downto 0) := x"6";
  constant ldyh	: std_logic_vector (3 downto 0) := x"7";  
  constant stxl	: std_logic_vector (3 downto 0) := x"8";
  constant styl	: std_logic_vector (3 downto 0) := x"9";
  constant stxh	: std_logic_vector (3 downto 0) := x"A";
  constant styh	: std_logic_vector (3 downto 0) := x"B";
  constant addix	: std_logic_vector (3 downto 0) := x"C";  
  constant addiy	: std_logic_vector (3 downto 0) := x"D";
-- return register save/restore
  constant rtt	: std_logic_vector (3 downto 0) := x"E";
  constant ttr	: std_logic_vector (3 downto 0) := x"F";  
-- basic signals

  signal accumcar  : std_logic_vector (width downto 0) := ProcVersion;  -- accumulator+carry
  alias accum		: std_logic_vector (width-1 downto 0) is accumcar(width-1 downto 0);
  alias carrybit	: std_logic is accumcar(width);
  signal maskedcarry : std_logic;

  signal pc		  : std_logic_vector (paddwidth -1 downto 0); -- program counter - 10 bits = 1k
  signal mra		 : std_logic_vector (maddwidth -1 downto 0);  -- memory read address - 11 bits = 2k
  signal mwa		 : std_logic_vector (maddwidth -1 downto 0);  -- memory write address - 11 bits = 2k
  signal id1		 : std_logic_vector (iwidth -1 downto 0);  -- instruction pipeline 1		 
  signal id2		 : std_logic_vector (iwidth -1 downto 0);  -- instruction pipeline 2			  
  alias opcode0	 : std_logic_vector (3 downto 0) is idbus (iwidth-1 downto iwidth-4);	-- main opcode at pipe0
  alias opcode2	 : std_logic_vector (3 downto 0) is id2 (iwidth-1 downto iwidth-4);	  -- main opcode at pipe2
  alias Arith		 : std_logic_vector (1 downto 0) is id2 (iwidth-1 downto iwidth-2);
  alias WithCarry  : std_logic is id2(iwidth-4);		-- indicates add with carry or subtract with borrow
  alias Minus		  is id2(iwidth-3);		-- indicates subtract
  alias opradd0	 : std_logic_vector (maddwidth -1 downto 0) is idbus (maddwidth -1 downto 0);					-- operand address at pipe0
  alias opradd2	 : std_logic_vector (maddwidth -1 downto 0) is id2 (maddwidth -1 downto 0);					  -- operand address at pipe2
  alias ind0		 : std_logic is idbus(iwidth -5); 
  alias ind2		 : std_logic is id2(iwidth -5);
  alias ireg0		: std_logic is idbus(iwidth -8);
  alias ireg2		: std_logic is id2(iwidth -8); 
  alias offset0	 : std_logic_vector (iwidth-9  downto 0) is idbus(iwidth-9 downto 0);
  alias offset2	 : std_logic_vector (iwidth-9  downto 0) is	id2(iwidth-9 downto 0);
  alias opropcode0 : std_logic_vector (3 downto 0) is idbus(iwidth-5 downto iwidth-8);	 -- operate opcode at pipe2  alias iopr2		: std_logic_vector (7 downto 0) is id2 (7 downto 0);					  -- immediate operand at pipe2
  alias opropcode2 : std_logic_vector (3 downto 0) is id2(iwidth-5 downto iwidth-8);	 -- operate opcode at pipe2  alias iopr2		: std_logic_vector (7 downto 0) is id2 (7 downto 0);					  -- immediate operand at pipe2
  alias iopr2		: std_logic_vector (7 downto 0) is id2 (7 downto 0);					  	 -- immediate operand at pipe2
  signal oprr		: std_logic_vector (width -1 downto 0);										 -- operand register
  signal idx		 : std_logic_vector (maddwidth -1 downto 0);
  signal idy		 : std_logic_vector (maddwidth -1 downto 0);
  signal idn0		 : std_logic_vector (maddwidth -1 downto 0);
  signal idn2		 : std_logic_vector (maddwidth -1 downto 0);
  signal maddpipe1  : std_logic_vector (maddwidth -1 downto 0);
  signal maddpipe2  : std_logic_vector (maddwidth -1 downto 0);
  signal maddpipe3  : std_logic_vector (maddwidth -1 downto 0);
  signal idr		 : std_logic_vector (paddwidth -1 downto 0);
  signal idt		 : std_logic_vector (paddwidth -1 downto 0);
  signal nextpc	 : std_logic_vector (paddwidth -1 downto 0);
  signal pcplus1	: std_logic_vector (paddwidth -1 downto 0);
  signal zero		: std_logic;

  function rotcleft(v : std_logic_vector ) return std_logic_vector is
	 variable result	: std_logic_vector(width downto 0);
  begin
	 result(width downto 1) := v(width-1 downto 0);
	 result(0)				  := v(width);
	 return result;
  end rotcleft;

  function rotcright(v : std_logic_vector ) return std_logic_vector is
	 variable result	 : std_logic_vector(width downto 0);
  begin
	 result(width -1 downto 0) := v(width downto 1);
	 result(width)				 := v(0);
	 return result;
  end rotcright;

begin  -- the CPU

  idproc : process (clk)					 -- instruction data pipeline
  begin
	 
	 if clk'event and clk = '1' then
		id1  <= idbus;	
		id2  <= id1;	  
		if reset = '1' then
		  id1  <= x"0000";					 -- fill pipeline with 0 (nop)
		end if;
	 end if;  -- if clk
  end process idproc;

  nextpcproc : process (clk, reset, pc, zero, nextpc, id2,
								ind0, ind2,idr, idbus, opcode0,
								opcode2, carrybit)  -- next pc calculation - jump decode
  begin
	 pcplus1 <= pc + '1';	 
	 iabus <= nextpc;							 -- program memory address from combinatorial	 
	 if reset = '1' then						 -- nextpc since blockram has built in addr register
		nextpc <= (others => '0');
	 else
		if (opcode0 = jmp) or (opcode0 = jsr) then
		  if ind0 = '1' then					  -- indirect
			 nextpc <= idr;
		  else										 -- direct
			 nextpc <= idbus(paddwidth -1 downto 0);
		  end if;
		elsif
		  ((opcode2 = jmpnz) and (zero = '0')) or
		  ((opcode2 = jmpz) and (zero = '1')) or
		  ((opcode2 = jmpnc) and (carrybit = '0')) or
		  ((opcode2 = jmpc) and (carrybit = '1')) then
		  if ind2 = '1' then				  -- indirect
			 nextpc <= idr;
		  else								  -- direct
			 nextpc <= id2(paddwidth -1 downto 0);
		  end if;
		else
		  nextpc <= pcplus1;
		end if;  -- opcode = jmp
	 end if;  -- no reset

	 if clk'event and clk = '1' then
		pc <= nextpc;

	 end if;

  end process nextpcproc;

  mraproc : process (idbus, idx, idy, mra, ind0,
							ireg0,
							offset0, opcode0, opradd0, clk)  -- memory read address generation
  begin
	 mradd <= mra;
	 case ireg0 is
		when '0' => idn0 <= idx;
		when '1' => idn0 <= idy;
		when others => null;
	  end case;
	 -- mux 
	 if ((opcode0 /= opr) and (ind0 = '0')) then
		mra <= opradd0;
	 else
		mra <= idn0 + (x"0"&offset0);
	 end if;
	 
	 
	 if clk'event and clk = '1' then
		maddpipe3 <= maddpipe2;
		maddpipe2 <= maddpipe1;
		maddpipe1 <= mra;
		if (opcode0 = lda) or (opcode0 = lor) or (opcode0 = lxor) or (opcode0 = land) 
		or (opcode0 = add) or (opcode0 = addc) or (opcode0 = sub) or (opcode0 = subc) then
			mread <= '1';
		else
			mread <= '0';
		end if;	
	 end if;		
  end process mraproc;

	mwaproc : process (maddpipe3)			 	-- memory write address generation
	begin
		mwadd	  <= maddpipe3;
	end process mwaproc;

  oprrproc : process (clk)			  			-- memory operand register  -- could remove to
  begin									  			-- reduce pipelining depth but would impact I/O read
	 if clk'event and clk = '1' then			-- access time  --> not good for larger systems
		oprr <= mibus;
	 end if;
  end process oprrproc;

  accumproc : process (clk, accum, carrybit)  -- accumulator instruction decode - operate
  begin
	 carryflg <= carrybit;
	 if accum = x"00" then
		zero <= '1';
	 else
		zero <= '0';
	end if;
	maskedcarry <= carrybit and WithCarry;
	
	if clk'event and clk = '1' then
		case opcode2 is -- memory reference first
			when land		=> accum	 <= accum and oprr;
			when lor			=> accum	 <= accum or oprr;
			when lxor		=> accum	 <= accum xor oprr;
			when lda			=> accum	 <= oprr;
					
			when opr				=>												  			-- operate
				case opropcode2 is
					when ldi		  => accum	 <= iopr2;							-- load immediate byte	  
					when rotcl		=> accumcar <= rotcleft(accumcar);		-- rotate left through carry
					when rotcr		=> accumcar <= rotcright(accumcar);	 	-- rotate right through carry				  

					when ldxl		  => accum	 <= idx(width-1 downto 0);
					when ldyl		  => accum	 <= idy(width-1 downto 0);
					when stxl		  => idx(width-1 downto 0) <= accum;
					when styl		  => idy(width-1 downto 0) <= accum;

					when ldxh		=> accum((maddwidth-width)-1 downto 0) <= idx(maddwidth-1 downto width);
										  accum(width-1 downto maddwidth-width) <= (others => '0'); 
					when ldyh		=> accum((maddwidth-width)-1 downto 0) <= idy(maddwidth-1 downto width);
											accum(width-1 downto maddwidth-width) <= (others => '0');  
					when stxh		=> idx(maddwidth-1 downto width) <= accum((maddwidth-width)-1 downto 0);
					when styh		=> idy(maddwidth-1 downto width) <= accum((maddwidth-width)-1 downto 0);
					when rtt			=> idt <= idr;
					when ttr			=> idr <= idt;
					when addix		=> idx <= maddpipe2;
					when addiy     => idy <= maddpipe2;
					when others	  => null;
			  end case;
			when others		 => null;
		end case;
		
		if Arith = "11" then
			if Minus = '0' then
				accumcar <= '0'&accum + oprr + maskedcarry; -- add/addc
			else
				accumcar <= '0'&accum - oprr - maskedcarry; -- sub/subc
			end if;
		end  if;					
		if (opcode0 = jsr) then			 	-- save return address -- note priority over ttr!	  
			idr  <= pcplus1;  
		end if;	
--		if opcode0 = opr then				-- this can be done at pipe 0 at the cost of 6 or so slices
--			if opropcode0 = addix then
--				idx <= mra;
--			end if;
--			if opropcode0 = addiy then
--				idy <= mra;
--			end if;
--		end if;			
	end if;  -- clk
end process accumproc;

  staproc : process (clk, accum)  -- sta decode  -- not much to do but enable mwrite
  begin
	 mobus <= accum;
	 if clk'event and clk = '1' then
		if opcode2 = sta then
		  mwrite <= '1';
		else
		  mwrite <= '0';
		end if;
	 end if;
  end process staproc;

end Behavioral;
