module stepgen(clk, enable, position, velocity, dirtime, steptime, step, dir);
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

output step, dir;
reg step, dir;

reg [T-1:0] timer;
reg [1:0] state;
reg ones;
wire dbit = velocity[F];
wire pbit = position[F];

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
