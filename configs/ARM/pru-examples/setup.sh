#!/bin/bash

if [ "$UID" -ne "0" ] ; then
        echo "Script must be run as root!"
        exit 1
fi

# Check to see if omap_mux debug directory is available
if [ ! -e /sys/kernel/debug/omap_mux/ ] ; then
        echo "Trying to mount debugfs"
        mount -t debugfs debugfs /sys/kernel/debug || {
                echo "Mounting debugfs failed!"
                exit 1
        }
fi

while read REG VALUE PIN JUNK ; do
        case "$REG" in
        ""|\#*)	
		continue ;;
        *)
                echo "$VALUE" > "/sys/kernel/debug/omap_mux/$REG"
                ;;
        esac

done <<- "EOF"
#	mcasp0_aclkx	5	pru0.r30.0
#	mcasp0_fsx	5	pru0.r30.1
#	mcasp0_axr0	5	pru0.r30.2
#	mcasp0_ahclkr	5	pru0.r30.3
#	mcasp0_aclkr	5	pru0.r30.4
#	mcasp0_fsr	5	pru0.r30.5
#	mcasp0_axr1	5	pru0.r30.6
#	mcasp0_ahclkx	5	pru0.r30.7
#	mmc0_dat3	5	pru0.r30.8
#	mmc0_dat2	5	pru0.r30.9
#	mmc0_dat1	5	pru0.r30.10
#	mmc0_dat0	5	pru0.r30.11
#	mmc0_clk	5	pru0.r30.12
#	mmc0_cmd	5	pru0.r30.13
	gpmc_ad12	6	pru0.r30.14
	gpmc_ad13	6	pru0.r30.15

	lcd_data0	5	pru1.r30.0
	lcd_data1	5	pru1.r30.1
	lcd_data2	5	pru1.r30.2
	lcd_data3	5	pru1.r30.3
	lcd_data4	5	pru1.r30.4
	lcd_data5	5	pru1.r30.5
	lcd_data6	5	pru1.r30.6
	lcd_data7	5	pru1.r30.7
	lcd_vsync	5	pru1.r30.8
	lcd_hsync	5	pru1.r30.9
	lcd_pclk	5	pru1.r30.10
	lcd_ac_bias_en	5	pru1.r30.11
	gpmc_csn1	5	pru1.r30.12
	gpmc_csn2	5	pru1.r30.13
#	uart0_rxd	5	pru1.r30.14
#	uart0_txd	5	pru1.r30.15

	gpmc_ad6	7	gpio1_6
#	gpmc_ad2	7	gpio1_2

EOF
