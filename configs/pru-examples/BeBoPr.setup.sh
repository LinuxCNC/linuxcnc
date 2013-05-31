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

	lcd_data0	5	pru1.r30.0	P8.45	J3 - PWM
	lcd_data1	5	pru1.r30.1	P8.46	J2 - PWM
	lcd_data2	5	pru1.r30.2	P8.43	X_Step
	lcd_data3	5	pru1.r30.3	P8.44	X_Dir
	lcd_data4	7	pru1.r30.4	P8.41	X_Ena
	lcd_data5	5	pru1.r30.5	P8.42	Y_Step
	lcd_data6	5	pru1.r30.6	P8.39	Y_Dir
	lcd_data7	7	pru1.r30.7	P8.40	Y_Ena
	lcd_data10	7	gpio2_16	P8.36	J4 - PWM
	lcd_vsync	5	pru1.r30.8	P8.27	Z_Step
	lcd_hsync	5	pru1.r30.9	P8.29	Z_Dir
	lcd_pclk	7	pru1.r30.10	P8.28	Z_Ena
	lcd_ac_bias_en	5	gpio2_25	P8.30	E_Step
	gpmc_csn1	5	pru1.r30.12	P8.21	E_Dir
	gpmc_csn2	5	pru1.r30.13	P8.20	E_Ena
#	uart0_rxd	5	pru1.r30.14
#	uart0_txd	5	pru1.r30.15

	gpmc_ad6	7	gpio1_6		P8.3	Enable
	gpmc_ad2	7	gpio1_2		P8.5	Enable_n

EOF
