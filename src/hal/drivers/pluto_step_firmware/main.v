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

module main(clk, led, nConfig, epp_nReset, pport_data, nWrite, nWait, nDataStr,
	nAddrStr, dout, din, step, dir);
parameter W=10;
parameter F=11;
parameter T=4;
input clk;
output led, nConfig;
inout [7:0] pport_data;
input nWrite;
output nWait;
input nDataStr, nAddrStr, epp_nReset;

input [15:0] din;

reg Spolarity;
reg[13:0] real_dout; output [13:0] dout = do_tristate ? 14'bZ : real_dout; 
wire[3:0] real_step; output [3:0] step = do_tristate ? 4'bZ : real_step ^ {4{Spolarity}};
wire[3:0] real_dir; output [3:0] dir = do_tristate ? 4'bZ : real_dir;

wire [W+F-1:0] pos0, pos1, pos2, pos3;
reg  [F:0]     vel0, vel1, vel2, vel3;
reg [T-1:0] dirtime, steptime;
reg [1:0] tap;

reg [10:0] div2048;
wire stepcnt = ~|(div2048[5:0]);

always @(posedge clk) begin
    div2048 <= div2048 + 1'd1;
end

wire do_enable_wdt, do_tristate;
wdt w(clk, do_enable_wdt, &div2048, do_tristate);

stepgen #(W,F,T) s0(clk, stepcnt, pos0, vel0, dirtime, steptime, real_step[0], real_dir[0], tap);
stepgen #(W,F,T) s1(clk, stepcnt, pos1, vel1, dirtime, steptime, real_step[1], real_dir[1], tap);
stepgen #(W,F,T) s2(clk, stepcnt, pos2, vel2, dirtime, steptime, real_step[2], real_dir[2], tap);
stepgen #(W,F,T) s3(clk, stepcnt, pos3, vel3, dirtime, steptime, real_step[3], real_dir[3], tap);

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
wire[15:0] EPP_dataword = {EPP_datain, lowbyte};
reg[4:0] addr_reg;
reg[7:0] lowbyte;

always @(posedge clk)
    if(EPP_strobe_edge1 & EPP_write & EPP_addr_strobe) begin
        addr_reg <= EPP_datain[4:0];
    end
    else if(EPP_strobe_edge1 & !EPP_addr_strobe) addr_reg <= addr_reg + 4'd1;
always @(posedge clk) begin
    if(EPP_strobe_edge1 & EPP_write & EPP_data_strobe) begin
        if(addr_reg[3:0] == 4'd1)      vel0 <= EPP_dataword[F:0];
        else if(addr_reg[3:0] == 4'd3) vel1 <= EPP_dataword[F:0];
        else if(addr_reg[3:0] == 4'd5) vel2 <= EPP_dataword[F:0];
        else if(addr_reg[3:0] == 4'd7) vel3 <= EPP_dataword[F:0];
        else if(addr_reg[3:0] == 4'd9) begin
            real_dout <= { EPP_datain[5:0], lowbyte };
        end
        else if(addr_reg[3:0] == 4'd11) begin
	    tap <= lowbyte[7:6];
            steptime <= lowbyte[T-1:0];

            Spolarity <= EPP_datain[7];
	    // EPP_datain[6] is do_enable_wdt
            dirtime <= EPP_datain[T-1:0];
        end
        else lowbyte <= EPP_datain;
    end
end

reg [31:0] data_buf;

always @(posedge clk) begin
    if(EPP_strobe_edge1 & EPP_read && addr_reg[1:0] == 2'd0) begin
        if(addr_reg[4:2] == 3'd0) data_buf <= pos0;
        else if(addr_reg[4:2] == 3'd1) data_buf <= pos1;
        else if(addr_reg[4:2] == 3'd2) data_buf <= pos2;
        else if(addr_reg[4:2] == 3'd3) data_buf <= pos3;
        else if(addr_reg[4:2] == 3'd4)
            data_buf <= din;
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
// assign do_enable_wdt = EPP_strobe_edge1 & EPP_write & EPP_data_strobe & (addr_reg[3:0] == 4'd9) & EPP_datain[6];
// assign led = do_tristate ? 1'BZ : (real_step[0] ^ real_dir[0]);
assign led = do_tristate ? 1'bZ : (real_step[0] ^ real_dir[0]);
assign nConfig = epp_nReset; // 1'b1;
assign do_enable_wdt = EPP_strobe_edge1 & EPP_write & EPP_data_strobe & (addr_reg[3:0] == 4'd9) & EPP_datain[6];
endmodule
