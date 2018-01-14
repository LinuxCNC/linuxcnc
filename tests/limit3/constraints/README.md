This test uses a mux to switch the limit3 input signal among a bunch
of options, and verifies that it never violates any of its constraints
(position, velocity, and acceleration).

The input signals are all the ones available from siggen (sine, cosine,
square, triangle, and sawtooth), plus a loopback of limit3's output.
