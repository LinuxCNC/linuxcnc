from pruaccess import *
import time


def main():
    p = Pruss(PRU_EVTOUT_0)
    p.pru = Pru(0,p)
    ecap = p.pru.ecap()

    # 5 is DEFAULT_INC each tick so this becomes a nsec timer
    # with 5ns resolution

    value = 400000000 *5  # 2 seconds

    b = AM33XX_PRUSS_ECAP_BASE

    # check revision
    if p.drw[ecap + ECAP0_REVID] != ECAP_REVID:
        print "ECAP0_REVID mismatch, got %8.8x expect%8.8x" % (
            p.drw[ecap + ECAP0_REVID], ECAP_REVID)

    # free/soft, no divider, no load of CAPx at capture event time
    p.drs[ecap + ECAP0_ECCTL1] = 0x0000C000

    # reset TSC
    p.drw[ecap + ECAP0_TSCTR] = 0

    # reset phase
    p.drw[ecap + ECAP0_CTRPHS] = 0

    # no comprende: this draws heavily on Bas' code without
    # actually fully understanding it yet ;)
    # see https://github.com/modmaker/BeBoPr/blob/master/pruss_stepper.c#L114
    slow = 20000000
    fast = 200
    p.drw[ecap + ECAP0_CAP1] = slow
    p.drw[ecap + ECAP0_CAP2] = slow/2

    p.drw[ecap + ECAP0_CAP1] = fast
    p.drw[ecap + ECAP0_CAP2] = fast/2

    # reset interrupt
    p.drs[ecap + ECAP0_ECCLR] &= (1 << 6)
    if p.drs[ecap + ECAP0_ECFLG] & (1 << 6):
        print "could not reset ecap0 interrupt" # ??

    # Start counter in APWM mode
    p.drs[ecap + ECAP0_ECCTL2] = (1 << 9) | (1 << 7) | (1 << 4)
    while True:
        if p.drs[ecap + ECAP0_ECFLG] & (1 << 6):
            break
    print "eCap0 initialized and found operational"

    # sample and print the TSC
    while True:
        now = time.time()
        print "TSCTR at %f = %d" % (now, p.drw[ecap + ECAP0_TSCTR])
        time.sleep(1)

if __name__ == '__main__':
    main()
