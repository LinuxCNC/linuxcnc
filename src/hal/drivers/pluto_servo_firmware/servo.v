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

module pluto_servo(clk, led, nConfig, epp_nReset, pport_data, nWrite, nWait, nDataStr,
	nAddrStr, dout, din, quadA, quadB, quadZ, up, down);
input clk;
output led, nConfig;
inout [7:0] pport_data;
input nWrite;
output nWait;
input nDataStr, nAddrStr, epp_nReset;
output [9:0] dout; reg[9:0] dout;
input [7:0] din;
input [3:0] quadA;
input [3:0] quadB;
input [3:0] quadZ;
output [3:0] up;
output [3:0] down;
reg Zpolarity;

wire [27:0] quad0, quad1, quad2, quad3;

// PWM stuff
// PWM clock is about 20kHz for clk @ 40MHz, 11-bit cnt
reg [10:0] pwmcnt;
wire [10:0] top = 11'd2046;
reg [15:0] pwm0, pwm1, pwm2, pwm3;
always @(posedge clk) begin
    if(pwmcnt == top) pwmcnt <= 0;
	else pwmcnt <= pwmcnt + 11'd1;
end

wire [10:0] pwmrev = { pwmcnt[2], pwmcnt[3],
    pwmcnt[4], pwmcnt[5], pwmcnt[6], pwmcnt[7], pwmcnt[8], pwmcnt[9],
    pwmcnt[10], pwmcnt[1:0]};

assign up[0] = pwm0[12] ^ (pwm0[15] ? 1'd0 : pwm0[10:0] > (pwm0[14] ? pwmrev : pwmcnt));
assign up[1] = pwm1[12] ^ (pwm1[15] ? 1'd0 : pwm1[10:0] > (pwm0[14] ? pwmrev : pwmcnt));
assign up[2] = pwm2[12] ^ (pwm2[15] ? 1'd0 : pwm2[10:0] > (pwm0[14] ? pwmrev : pwmcnt));
assign up[3] = pwm3[12] ^ (pwm3[15] ? 1'd0 : pwm3[10:0] > (pwm0[14] ? pwmrev : pwmcnt));
assign down[0] = pwm0[13] ^ (~pwm0[15] ? 1'd0 : pwm0[10:0] > (pwm0[14] ? pwmrev : pwmcnt));
assign down[1] = pwm1[13] ^ (~pwm1[15] ? 1'd0 : pwm1[10:0] > (pwm0[14] ? pwmrev : pwmcnt));
assign down[2] = pwm2[13] ^ (~pwm2[15] ? 1'd0 : pwm2[10:0] > (pwm0[14] ? pwmrev : pwmcnt));
assign down[3] = pwm3[13] ^ (~pwm3[15] ? 1'd0 : pwm3[10:0] > (pwm0[14] ? pwmrev : pwmcnt));

// Quadrature stuff
// Quadrature is digitized at 40MHz into 12-bit counters
// Read up to 2^11 pulses / polling period = 2048 kHz for 1kHz servo period

quad q0(clk, quadA[0], quadB[0], quadZ[0] ^ Zpolarity, quad0[13:0], quad0[27:14]);
quad q1(clk, quadA[1], quadB[1], quadZ[1] ^ Zpolarity, quad1[13:0], quad1[27:14]);
quad q2(clk, quadA[2], quadB[2], quadZ[2] ^ Zpolarity, quad2[13:0], quad2[27:14]);
quad q3(clk, quadA[3], quadB[3], quadZ[3] ^ Zpolarity, quad3[13:0], quad3[27:14]);

// EPP stuff
wire EPP_write = ~nWrite;
wire EPP_read = nWrite;
wire EPP_addr_strobe = ~nAddrStr;
wire EPP_data_strobe = ~nDataStr;
wire EPP_strobe = EPP_data_strobe | EPP_addr_strobe;

wire EPP_wait; assign nWait = ~EPP_wait;
wire [7:0] EPP_datain = pport_data;
wire [7:0] EPP_dataout; assign pport_data = EPP_dataout;

reg [4:0] EPP_strobe_reg;
always @(posedge clk) EPP_strobe_reg <= {EPP_strobe_reg[3:0], EPP_strobe};
wire EPP_strobe_edge1 = (EPP_strobe_reg[2:1]==2'b01);

// reg led;

assign EPP_wait = EPP_strobe_reg[4];
reg[4:0] addr_reg;
reg[7:0] lowbyte;

always @(posedge clk)
    if(EPP_strobe_edge1 & EPP_write & EPP_addr_strobe) begin
		addr_reg <= EPP_datain[3:0];
		// led <= ~led;
	end
    else if(EPP_strobe_edge1 & !EPP_addr_strobe) addr_reg <= addr_reg + 4'd1;
always @(posedge clk) begin
    if(EPP_strobe_edge1 & EPP_write & EPP_data_strobe) begin
	if(addr_reg[3:0] == 4'd1)      pwm0 <= { EPP_datain, lowbyte };
	else if(addr_reg[3:0] == 4'd3) pwm1 <= { EPP_datain, lowbyte };
	else if(addr_reg[3:0] == 4'd5) pwm2 <= { EPP_datain, lowbyte };
	else if(addr_reg[3:0] == 4'd7) pwm3 <= { EPP_datain, lowbyte };
	else if(addr_reg[3:0] == 4'd9) begin
		dout[9:0] <= { EPP_datain[1:0], lowbyte };
		Zpolarity <= EPP_datain[7];
	end
//	else if(addr_reg[3:0] == 4'd11) reset <= EPP_datain == 8'hee;
	else lowbyte <= EPP_datain;
    end
end

reg [31:0] data_buf;

always @(posedge clk) begin
    if(EPP_strobe_edge1 & EPP_read && addr_reg[1:0] == 2'd0) begin
		if(addr_reg[4:2] == 3'd0) data_buf <= quad0;
		else if(addr_reg[4:2] == 3'd1) data_buf <= quad1;
		else if(addr_reg[4:2] == 3'd2) data_buf <= quad2;
		else if(addr_reg[4:2] == 3'd3) data_buf <= quad3;
		else if(addr_reg[4:2] == 3'd4) begin data_buf <= {4'd0, quadA, quadB, quadZ, din}; end
    end
end

// the addr_reg test looks funny because it is auto-incremented in an always block
// so "1" reads the low byte, "2 and "3" read middle bytes, and "0" reads the high byte
// I have a feeling that I'm doing this in the wrong way.
wire [7:0] data_reg = addr_reg[1:0] == 2'd1 ? data_buf[7:0] :
			 (addr_reg[1:0] == 2'd2 ? data_buf[15:8] :
			 (addr_reg[1:0] == 2'd3 ? data_buf[23:16] :
			 data_buf[27:24]));

wire [7:0] EPP_data_mux = EPP_addr_strobe ? addr_reg : data_reg;
assign EPP_dataout = (EPP_read & EPP_wait) ? EPP_data_mux : 8'hZZ;

assign led = up[0] ^ down[0];
assign nConfig = epp_nReset; // 1'b1;
endmodule
