library IEEE;
use IEEE.std_logic_1164.ALL;
use IEEE.std_logic_ARITH.ALL;
use IEEE.std_logic_UNSIGNED.ALL;
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

entity qcounter is
   generic (
		buswidth : integer := 32
		);
   port ( 
		obus: out std_logic_vector (buswidth-1 downto 0);
		ibus: in std_logic_vector (buswidth-1 downto 0);
		quada: in std_logic;
		quadb: in std_logic;
		index: in std_logic;
		loadccr: in std_logic;
		readccr: in std_logic;
		readcount: in std_logic;
		countclear: in std_logic;
		timestamp: in std_logic_vector (15 downto 0);
		indexmask: in std_logic;
		clk: in std_logic
	);
end qcounter;

architecture behavioral of qcounter is

signal count: std_logic_vector (15 downto 0);
signal up: std_logic;
signal down: std_logic;
signal countlatch: std_logic_vector (15 downto 0);
signal timestamplatch: std_logic_vector (15 downto 0);
signal quadadel: std_logic;
signal quada1: std_logic;
signal quada2: std_logic;
signal quadacnt: std_logic_vector (3 downto 0);
signal quadafilt: std_logic;
signal quadbdel: std_logic;
signal quadb1: std_logic;
signal quadb2: std_logic;
signal quadbcnt: std_logic_vector (3 downto 0);
signal quadbfilt: std_logic;
signal indexdel: std_logic;
signal index1: std_logic;
signal index2: std_logic;
signal indexdet: std_logic;
signal indexcnt: std_logic_vector (3 downto 0);
signal indexfilt: std_logic;
signal qcountup: std_logic; 
signal qcountdown: std_logic;
signal udcountup: std_logic; 
signal udcountdown: std_logic;
signal doclear: std_logic;
signal clearonindex: std_logic;	-- ccr register bits...
signal latchonindex:std_logic;
signal justonce: std_logic;
signal abgateindex: std_logic;
signal indexsrc: std_logic;
signal quadfilter: std_logic;
signal countermode: std_logic;
signal quaderror: std_logic;
signal indexpol: std_logic;
signal fixedindexmask: std_logic;
signal indexmaskpol: std_logic;
signal useindexmask: std_logic;
signal abmaskpol: std_logic;
signal flimit: std_logic_vector(3 downto 0);


begin
	aqcounter: process 	(clk)

	begin
		-- new index logic 02/09/2006 PCW
		
		if abgateindex = '0' then	 		-- not gated by A,B
			if indexpol = '1' then
				indexsrc	<= indexdel;
			else
				indexsrc <= not indexdel;
			end if;
		else									-- gated by A,B
			if indexpol = '1' then										  		-- normal index
				if abmaskpol = '1' then 
					indexsrc <=	quadadel and quadbdel and indexdel;					-- enable by A,B high
				else
					indexsrc <= (not (quadadel or quadbdel)) and indexdel;		-- enable by A,B low
				end if;
			else																		-- inverted index
				if abmaskpol = '1' then 
					indexsrc <=	quadadel and quadbdel and (not indexdel);			-- enable by A,B high
				else
					indexsrc <= (not (quadadel or quadbdel)) and (not indexdel);-- enable by A,B low
				end if;
			end if;
		end if;

		if indexmaskpol = '1' then
			fixedindexmask <= indexmask;
		else
			fixedindexmask <= not indexmask;
		end if;

		if quadfilter = '1' then
			flimit <= "1111";
		else
			flimit <= "0011";
		end if;		
		if countermode = '0' and doclear = '0' and (  
		(quada2 = '0' and quada1 = '1' and quadb2 = '0' and quadb1 = '0')  or
		(quada2 = '0' and quada1 = '0' and quadb2 = '1' and quadb1 = '0')  or
		(quada2 = '1' and quada1 = '1' and quadb2 = '0' and quadb1 = '1')  or
		(quada2 = '1' and quada1 = '0' and quadb2 = '1' and quadb1 = '1')) then		   
			qcountup <= '1';
		else 
			qcountup <= '0';
		end if;			

		if (countermode = '1' and doclear = '0' and 
		quadb2 = '1' and quada2 = '0' and quada1 = '1') then  -- up down mode: count up on rising edge of A when B is high
			udcountup <= '1';
		else
			udcountup <= '0';
		end if;
			
		if countermode = '0' and doclear = '0' and ( 
		(quada2 = '0' and quada1 = '0' and quadb2 = '0' and quadb1 = '1')  or
		(quada2 = '0' and quada1 = '1' and quadb2 = '1' and quadb1 = '1')  or
		(quada2 = '1' and quada1 = '0' and quadb2 = '0' and quadb1 = '0')  or
		(quada2 = '1' and quada1 = '1' and quadb2 = '1' and quadb1 = '0')) then
			qcountdown <= '1';
		else
			qcountdown <= '0';
		end if;		
	
		if (countermode = '1' and doclear = '0' and  
		quadb2 = '0' and quada2 = '0' and quada1 = '1') then
			udcountdown <= '1';
		else
		   udcountdown <= '0';
		end if;		

		if rising_edge(clk) then
			

			quadadel <= quada;			
			quada1 <= quadafilt;
			quada2 <= quada1;	

			quadbdel <= quadb;
			quadb1 <= quadbfilt;			
			quadb2 <= quadb1;	

			indexdel <= index;
			index1 <= indexfilt;			
			index2 <= index1;	

			-- deadended counter for A input filter --
			if (quadadel = '1') and (quadacnt < flimit) then 
				quadacnt <= quadacnt + 1;
			end if;
			if (quadadel = '0') and (quadacnt /= 0) then 
			   quadacnt <= quadacnt -1;
			end if;
			if quadacnt >= flimit then
			  quadafilt<= '1';
			end if;
			if quadacnt = 0 then
			  quadafilt<= '0';
			end if;
				
			-- deadended counter for A input filter --
			if (quadbdel = '1') and (quadbcnt < flimit ) then 
			   quadbcnt <= quadbcnt + 1;
			end if;
			if (quadbdel = '0') and (quadbcnt /= 0) then 
			   quadbcnt <= quadbcnt -1;
			end if;
			if quadbcnt >= flimit then
			  quadbfilt<= '1';
			end if;
			if quadbcnt = 0 then
			  quadbfilt <= '0';
			end if;	

			-- deadended counter for index input filter --
			if (indexsrc = '1') and (indexcnt < flimit ) then 
			   indexcnt <= indexcnt + 1;
			end if;
			if (indexsrc = '0') and (indexcnt /= 0) then 
			   indexcnt <= indexcnt -1;
			end if;
			if indexcnt >= flimit then
			  indexfilt<= '1';
			end if;
			if indexcnt = 0 then
			  indexfilt<= '0';
			end if;					

			if (countclear = '1')  or 
			((clearonindex = '1') and (indexdet = '1')) then -- rising edge of conditioned index	
				doclear <= '1';
				if justonce = '1' then
					clearonindex <= '0';
				end if;
			else
				doclear <= '0';
			end if;		

			if ((latchonindex = '1') and (indexdet = '1') ) then	-- rising edge of conditioned index
				countlatch <= count;
				if justonce = '1' then
					latchonindex <= '0';
				end if;		
			end if;


			if	countermode = '0' and (
			(quada2 = '0' and quada1 = '1' and quadb2 = '0' and quadb1 = '1')  or	  -- any time both a,b change at same time
			(quada2 = '1' and quada1 = '0' and quadb2 = '1' and quadb1 = '0')  or	  -- indicates a quadrature count error
			(quada2 = '0' and quada1 = '1' and quadb2 = '1' and quadb1 = '0')  or
			(quada2 = '1' and quada1 = '0' and quadb2 = '0' and quadb1 = '1')) then
			 	quaderror <= '1';
			end if;

			if up /= down then
				timestamplatch <= timestamp;	 		-- time stamp whenever we count
				if up = '1' then 
					count <= count + 1; 
				else
					count <= count - 1;
				end if;
			end if;	

			if doclear = '1' then
				count <= x"0000";
			end if;

			if loadccr = '1' then	
				quaderror <= ibus(15);
				abmaskpol <= ibus(14);
				quadfilter <= ibus(11);
				countermode <= ibus(10);
				useindexmask <= ibus(9);
				indexmaskpol <= ibus(8);
				abgateindex <= ibus(7);
				justonce <= ibus(6);
				clearonindex <= ibus(5);
				latchonindex <= ibus(4);
				indexpol <= ibus(3);
			end if;
		end if; --(clock edge)

		if (index1 = '1') and (index2 = '0') and ((fixedindexmask = '1') or (useindexmask = '0')) then
			indexdet <= '1';
		else
			indexdet <= '0';
		end if;

		if (qcountup = '1' or udcountup = '1' ) and doclear = '0' then
			up <= '1';
		else
		  	up <= '0';
		end if;
   
		if (qcountdown = '1' or udcountdown = '1' ) and doclear = '0'  then
			down <= '1';
		else
		  	down <= '0';
		end if;	 
			
		obus <= (others => 'Z');
	
		if	 (readcount = '1') then
			obus(31 downto 16) <= timestamplatch;
			obus(15 downto 0) <= count;
		end if;		
		if (readccr = '1') then
				obus(31 downto 16) <= countlatch;
				obus(15) <= quaderror;
				obus(14) <= abmaskpol;		
				obus(11) <= quadfilter;
				obus(10) <= countermode;
				obus(9) <= useindexmask; 
				obus(8) <= indexmaskpol;
				obus(7) <= abgateindex;	
				obus(6) <= justonce;							
				obus(5) <= clearonindex;
				obus(4) <= latchonindex;
				obus(3) <= indexpol;
				obus(2) <= index1;
				obus(1) <= quadb1;
				obus(0) <= quada1;
		end if;
	end process;
end behavioral;
