.origin 0
.entrypoint GPIOTOGGLE

// infinite pin wiggling at maximum rate
GPIOTOGGLE:
    SET      R30.t15 // set GPIO1_13 high
    CLR      R30.t15 // set GPIO1_13 low
    JMP GPIOTOGGLE
