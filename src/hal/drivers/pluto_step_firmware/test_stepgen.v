// install ubuntu package "verilog" from universe
// Compile with: iverilog -DTESTING test_stepgen.v stepgen.v
// run with "./a.out | less" and look at output for problems

module test_stepgen();

reg clk;
reg [4:0] vel;
wire [19:0] pos;
wire step, dir;

stepgen #(16,4,16) s(clk, 1, pos, vel, 1, 0, step, dir, 3);
integer q;
reg ost;

initial begin
    vel = 5'h8; // two useful test cases:
                // vel=5'h8 (max step speed)
                // vel=5'h2 (~1 step per repeat)
    q = 0;
    repeat(50) begin
        repeat(50) begin
            #20 clk<=1;
            #20 clk<=0;

            if(step && !ost) begin
                if(dir) q = q+1;
                else q = q - 1;
            end
            ost <= step;

            $display("%d %d %x %x %d %d %d %d %d",
                step, dir, vel, pos, s.state, s.ones, s.pbit, s.timer, q);
        end
        vel = 6'h20 - vel;
    end
end

endmodule
