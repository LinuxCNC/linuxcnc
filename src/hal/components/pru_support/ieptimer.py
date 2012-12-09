from pruaccess import *
import time

# use of IEP COUNT and CMP registers for precise timing
# see AM335x PRU-ICSS Reference Guide for details, pages 248 ff
# works fine, but due to the reset of the COUNT register we loose a global 200Mhz tick

def main():
    p = Pruss(PRU_EVTOUT_0)

    # 5 is DEFAULT_INC each tick so this becomes a nsec timer
    # with 5ns resolution

    value = 400000000 *5  # 2 seconds

    b = AM33XX_PRUSS_IEP_BASE

    # set IEP compare register 0 value
    p.dataram_w[b + IEP_CMP0] = value

    # enable counter reset on compare, enable compare0
    p.dataram_w[b + IEP_CMP_CFG] = IEP_CMP_EN0 | IEP_CMP0_RST_CNT_EN

    # enable  IEP counter
    p.dataram_w[b+IEP_GLOBAL_CFG]  |= IEP_CNT_ENABLE;

    # reset the IEP counter
    p.dataram_w[b+IEP_COUNT] = 0

    t = time.time()
    # this should trigger a compare event and counter reset every value clock ticks
    while True:
        if p.dataram_w[b+IEP_CMP_STATUS] & IEP_CMP_HIT0:
            now = time.time()
            lap = now -t
            t = now
            # match event occurred, clear it
            p.dataram_w[b+IEP_CMP_STATUS] = IEP_CMP_HIT0
            print "CMP0 match lap time=",lap
        time.sleep(0.01)


if __name__ == '__main__':
    main()
