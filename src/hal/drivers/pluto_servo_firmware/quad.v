//    This is a component of pluto_servo, a PWM servo driver and quadrature
//    counter for emc2
//    Copyright 2006 Jeff Epler <jepler@unpythonic.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

module quad(clk, A, B, Z, c, i);
input clk, A, B, Z;
output [13:0] c, i;
reg [13:0] c, i;

reg [2:0] Ad, Bd;
reg [5:0] Zd;
always @(posedge clk) Ad <= {Ad[1:0], A};
always @(posedge clk) Bd <= {Bd[1:0], B};
always @(posedge clk) Zd <= {Zd[4:0], Z};

// stabalizes Z, finds rising edge, and requires that the index pulse be at least 3 clocks (75ns @ 40MHz) long
wire index_pulse = Zd == 6'b000111;
wire count_enable = Ad[1] ^ Ad[2] ^ Bd[1] ^ Bd[2];
wire count_direction = Ad[1] ^ Bd[2];

always @(posedge clk)
begin
    if(count_enable)
    begin
		if(count_direction) c = c + 14'd1;
		else c = c - 14'd1;
    end 
    if(index_pulse) i = c;
end
endmodule
