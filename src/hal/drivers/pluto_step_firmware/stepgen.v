//    This is a component of pluto_step, a hardware step waveform generator
//    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

module stepgen(clk, enable, position, velocity, dirtime, steptime, step, dir, tap);
`define STATE_STEP 0
`define STATE_DIRCHANGE 1
`define STATE_DIRWAIT 2

parameter W=12;
parameter F=10;
parameter T=5;

input clk, enable;
output [W+F-1:0] position; reg [W+F-1:0] position;
input [F:0] velocity;
input [T-1:0] dirtime, steptime;
input [1:0] tap;

output step, dir;
reg step, dir;

reg [T-1:0] timer;
reg [1:0] state;
reg ones;
wire dbit = velocity[F];
wire pbit = (tap == 0 ? position[F] 
	    : (tap == 1 ? position[F+1]
	    : (tap == 2 ? position[F+2]
	    : position[F+3])));

wire [W+F-1:0] xvelocity = {{W{velocity[F]}}, {1{velocity[F-1:0]}}};

`ifdef TESTING
// for testing:
initial position = 1'b0;
initial state = `STATE_STEP;
initial timer = 0;
initial dir = 0;
initial ones = 0;
`endif
 
always @(posedge clk) begin
    if(enable) begin
	// $display("state=%d timer=%d position=%h velocity=%h dir=%d dbit=%d pbit=%d ones=%d", state, timer, position, xvelocity, dir, dbit, pbit, ones);
	if((dir != dbit) && (pbit == ones)) begin
	    if(state == `STATE_DIRCHANGE) begin
		if(timer == 0) begin
		    dir <= dbit;
		    timer <= dirtime;
		    state <= `STATE_DIRWAIT;
		end else begin
		    timer <= timer - 1'd1;
		end
	    end else begin
		if(timer == 0) begin
		    step <= 0;
		    timer <= dirtime;
		    state <= `STATE_DIRCHANGE;
		end else begin
		    timer <= timer - 1'd1;
		end
	    end
	end else if(state == `STATE_DIRWAIT) begin
	    if(timer == 0) begin
		state <= `STATE_STEP;
	    end else begin
		timer <= timer - 1'd1;
	    end
	end else begin
	    if(timer == 0) begin
		if(pbit != ones) begin
		    ones <= pbit;
		    step <= 1'd1;
		    timer <= steptime;
		end else begin
		    step <= 0;
		end
	    end else begin
		timer <= timer - 1'd1;
	    end
            if(dir == dbit) 
                position <= position + xvelocity;
        end
    end
end

endmodule
