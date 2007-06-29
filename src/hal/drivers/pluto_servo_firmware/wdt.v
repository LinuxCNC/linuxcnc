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

module wdt(clk, ena, cnt, out);
input clk, ena, cnt;
output out;
reg [6:0] timer;
wire timer_top = (timer == 7'd127);
reg internal_enable;
wire out = internal_enable && timer_top;

always @(posedge clk) begin
    if(ena) begin
	internal_enable <= 1;
	timer <= 0;
    end else if(cnt && !timer_top) timer <= timer + 7'd1;
end
endmodule
