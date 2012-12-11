from pruaccess import *
import time


def main():
    p = Pruss(PRU_EVTOUT_0)

    # 5 is DEFAULT_INC each tick so this becomes a nsec timer
    # with 5ns resolution

    value = 400000000 *5  # 2 seconds

    b = AM33XX_PRUSS_ECAP_BASE

    # check revision
    if p.dataram_w[b + ECAP0_REVID] != ECAP_REVID:
        print "ECAP0_REVID mismatch, got %8.8x expect%8.8x" % (
            p.dataram_w[b + ECAP0_REVID], ECAP_REVID)


    # free/soft, no divider, no load of CAPx at capture event time
    p.dataram_w[b + ECAP0_ECCTL1] = 0x0000C000

    # reset TSC
    p.dataram_w[b + ECAP0_TSCTR] = 0

    # reset phase
    p.dataram_w[b + ECAP0_CTRPHS] = 0

    # no comprende:
    slow = 20000000
    fast = 200
    p.dataram_w[b + ECAP0_CAP1] = slow
    p.dataram_w[b + ECAP0_CAP2] = slow/2

    p.dataram_w[b + ECAP0_CAP1] = fast
    p.dataram_w[b + ECAP0_CAP2] = fast/2

    # reset interrupt
    p.dataram_w[b + ECAP0_ECCLR] & (1 << 6):
    if p.dataram_w[b + ECAP0_ECFLG] & (1 << 6):
        print "could not reset ecap0 interrupt"

    # Start counter in APWM mode
    p.dataram_w[b + ECAP0_ECCTL2] = (1 << 9) | (1 << 7) | (1 << 4)
    while True:
        if p.dataram_w[b + ECAP0_ECFLG] & (1 << 6):
            break
    print "eCap0 initialized and found operational"

    # sample and print the TSC
    while True:
        now = time.time()
        print "TSCTR at %f = %d" % (now, p.dataram_w[b + ECAP0_TSCTR])
        time.sleep(1)

#   stop tsc, capture mode, SYNCO_SEL=Select CTR = PRD event to be the sync-out signal
#   pruss_wr16( PRUSS_ECAP0_OFFSET + O_ECCTL2, (1 << 9) | (1 << 7) | (0 << 4));
#   // Reset counter and phase registers
#   pruss_wr32( PRUSS_ECAP0_OFFSET + O_TSCTR, 0);
#   pruss_wr32( PRUSS_ECAP0_OFFSET + O_CTRPHS, 0);
#   // Setup for large (detection) cycle
#   pruss_wr32( PRUSS_ECAP0_OFFSET + O_CAP1, 20 000 000);	// PWM period 0.1 s
#   pruss_wr32( PRUSS_ECAP0_OFFSET + O_CAP2, 10000000);	// PWM on time, don't care
#   pruss_wr32( PRUSS_ECAP0_OFFSET + O_CAP1, 200);	// set PWM period to 1 us
#   pruss_wr32( PRUSS_ECAP0_OFFSET + O_CAP2, 100);	// PWM on time, don't care
#   // Clear pending interrupt
#   pruss_wr16( PRUSS_ECAP0_OFFSET + O_ECCLR, (1 << 6));
#   if (pruss_rd16( PRUSS_ECAP0_OFFSET + O_ECFLG) & (1 << 6)) {
#     printf( "*** WARNING: could not clear eCAP0 interrupt\n");
#     exit( EXIT_FAILURE);
#   }
#   // Start counter in APWM mode
#   pruss_wr16( PRUSS_ECAP0_OFFSET + O_ECCTL2, (1 << 9) | (1 << 7) | (1 << 4));
#   do {
#     value = pruss_rd16( PRUSS_ECAP0_OFFSET + O_ECFLG);
#   } while ((value & (1 << 6)) == 0);
#   pruss_wr16( PRUSS_ECAP0_OFFSET + O_ECCLR, (1 << 6));
#   if (debug_flags & DEBUG_PRUSS) {
#     printf( "eCap0 initialized and found operational\n");
#   }


    # # enable counter reset on compare, enable compare0
    # p.dataram_w[b + IEP_CMP_CFG] = IEP_CMP_EN0 | IEP_CMP0_RST_CNT_EN

    # # enable  IEP counter
    # p.dataram_w[b+IEP_GLOBAL_CFG]  |= IEP_CNT_ENABLE;

    # # reset the IEP counter
    # p.dataram_w[b+IEP_COUNT] = 0


if __name__ == '__main__':
    main()
