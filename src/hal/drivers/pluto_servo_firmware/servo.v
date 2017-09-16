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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

module pluto_servo(clk, led, nConfig, epp_nReset, pport_data, nWrite, nWait, nDataStr,
	nAddrStr, dout, din, quadA, quadB, quadZ, up, down);
parameter QW=14;
input clk;
output led, nConfig;
inout [7:0] pport_data;
input nWrite;
output nWait;
input nDataStr, nAddrStr, epp_nReset;
wire do_tristate;
reg[9:0] real_dout; output [9:0] dout = do_tristate ? 10'bZZZZZZZZZZ : real_dout; 
input [7:0] din;
input [3:0] quadA;
input [3:0] quadB;
input [3:0] quadZ;
wire[3:0] real_up; output [3:0] up = do_tristate ? 4'bZZZZ : real_up;
wire[3:0] real_down; output [3:0] down = do_tristate ? 4'bZZZZ : real_down;
reg Zpolarity;

wire [2*QW:0] quad0, quad1, quad2, quad3;

wire do_enable_wdt;
wire pwm_at_top;
wdt w(clk, do_enable_wdt, pwm_at_top, do_tristate);

// PWM stuff
// PWM clock is about 20kHz for clk @ 40MHz, 11-bit cnt
reg [10:0] pwmcnt;
wire [10:0] top = 11'd2046;
assign pwm_at_top = (pwmcnt == top);
reg [15:0] pwm0, pwm1, pwm2, pwm3;
always @(posedge clk) begin
    if(pwm_at_top) pwmcnt <= 0;
    else pwmcnt <= pwmcnt + 11'd1;
end

wire [10:0] pwmrev = { 
    pwmcnt[4], pwmcnt[5], pwmcnt[6], pwmcnt[7], pwmcnt[8], pwmcnt[9],
    pwmcnt[10], pwmcnt[3:0]};
wire [10:0] pwmcmp0 = pwm0[14] ? pwmrev : pwmcnt;
// wire [10:0] pwmcmp1 = pwm1[14] ? pwmrev : pwmcnt;
// wire [10:0] pwmcmp2 = pwm2[14] ? pwmrev : pwmcnt;
// wire [10:0] pwmcmp3 = pwm3[14] ? pwmrev : pwmcnt;
wire pwmact0 = pwm0[10:0] > pwmcmp0;
wire pwmact1 = pwm1[10:0] > pwmcmp0;
wire pwmact2 = pwm2[10:0] > pwmcmp0;
wire pwmact3 = pwm3[10:0] > pwmcmp0;
assign real_up[0] = pwm0[12] ^ (pwm0[15] ? 1'd0 : pwmact0);
assign real_up[1] = pwm1[12] ^ (pwm1[15] ? 1'd0 : pwmact1);
assign real_up[2] = pwm2[12] ^ (pwm2[15] ? 1'd0 : pwmact2);
assign real_up[3] = pwm3[12] ^ (pwm3[15] ? 1'd0 : pwmact3);
assign real_down[0] = pwm0[13] ^ (~pwm0[15] ? 1'd0 : pwmact0);
assign real_down[1] = pwm1[13] ^ (~pwm1[15] ? 1'd0 : pwmact1);
assign real_down[2] = pwm2[13] ^ (~pwm2[15] ? 1'd0 : pwmact2);
assign real_down[3] = pwm3[13] ^ (~pwm3[15] ? 1'd0 : pwmact3);

// Quadrature stuff
// Quadrature is digitized at 40MHz into 14-bit counters
// Read up to 2^13 pulses / polling period = 8MHz for 1kHz servo period
reg qtest;
wire qr0, qr1, qr2, qr3;
quad q0(clk, qtest ? real_dout[0] : quadA[0], qtest ? real_dout[1] : quadB[0], qtest ? real_dout[2] : quadZ[0]^Zpolarity, qr0, quad0);
quad q1(clk, quadA[1], quadB[1], quadZ[1]^Zpolarity, qr1, quad1);
quad q2(clk, quadA[2], quadB[2], quadZ[2]^Zpolarity, qr2, quad2);
quad q3(clk, quadA[3], quadB[3], quadZ[3]^Zpolarity, qr3, quad3);

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
        addr_reg <= EPP_datain[4:0];
    end
    else if(EPP_strobe_edge1 & !EPP_addr_strobe) addr_reg <= addr_reg + 4'd1;
always @(posedge clk) begin
    if(EPP_strobe_edge1 & EPP_write & EPP_data_strobe) begin
        if(addr_reg[3:0] == 4'd1)      pwm0 <= { EPP_datain, lowbyte };
        else if(addr_reg[3:0] == 4'd3) pwm1 <= { EPP_datain, lowbyte };
        else if(addr_reg[3:0] == 4'd5) pwm2 <= { EPP_datain, lowbyte };
        else if(addr_reg[3:0] == 4'd7) pwm3 <= { EPP_datain, lowbyte };
        else if(addr_reg[3:0] == 4'd9) begin
            real_dout <= { EPP_datain[1:0], lowbyte };
            Zpolarity <= EPP_datain[7];
            qtest <= EPP_datain[5];
        end
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
        else if(addr_reg[4:2] == 3'd4)
            data_buf <= {quadA, quadB, quadZ, din};
    end
end

// the addr_reg test looks funny because it is auto-incremented in an always
// block so "1" reads the low byte, "2 and "3" read middle bytes, and "0"
// reads the high byte I have a feeling that I'm doing this in the wrong way.
wire [7:0] data_reg = addr_reg[1:0] == 2'd1 ? data_buf[7:0] :
                         (addr_reg[1:0] == 2'd2 ? data_buf[15:8] :
                         (addr_reg[1:0] == 2'd3 ? data_buf[23:16] :
                         data_buf[31:24]));

wire [7:0] EPP_data_mux = data_reg;
assign EPP_dataout = (EPP_read & EPP_wait) ? EPP_data_mux : 8'hZZ;
assign do_enable_wdt = EPP_strobe_edge1 & EPP_write & EPP_data_strobe & (addr_reg[3:0] == 4'd9) & EPP_datain[6];
assign qr0 = EPP_strobe_edge1 & EPP_read & EPP_data_strobe & (addr_reg[4:2] == 3'd0);
assign qr1 = EPP_strobe_edge1 & EPP_read & EPP_data_strobe & (addr_reg[4:2] == 3'd1);
assign qr2 = EPP_strobe_edge1 & EPP_read & EPP_data_strobe & (addr_reg[4:2] == 3'd2);
assign qr3 = EPP_strobe_edge1 & EPP_read & EPP_data_strobe & (addr_reg[4:2] == 3'd3);
assign led = do_tristate ? 1'BZ : (real_up[0] ^ real_down[0]);
assign nConfig = epp_nReset; // 1'b1;
endmodule
