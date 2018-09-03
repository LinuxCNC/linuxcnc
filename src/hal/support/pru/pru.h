#ifndef _pru_H_
#define _pru_H_

// this file is from https://github.com/millerap/AM335x_PRU_BeagleBone/blob/master/pwm_sin/bin/pru.hp
// the PASM macros have been factored out into pru_macros.hp so
// this file can be used both from C and PASM

// ***************************************
// * Global Macro definitions *
// ***************************************

// Refer to this mapping in the file - \prussdrv\include\pruss_intc_mapping.h
#define PRU0_PRU1_INTERRUPT 17
#define PRU1_PRU0_INTERRUPT 18
#define PRU0_ARM_INTERRUPT 19
#define PRU1_ARM_INTERRUPT 20
#define ARM_PRU0_INTERRUPT 21
#define ARM_PRU1_INTERRUPT 22

#define CONST_PRUSSINTC C0
#define CONST_PRUCFG C4
#define CONST_PRUDRAM C24
#define CONST_PRUDRAM_REMOTE C25
#define CONST_IEP C26
#define CONST_PRUSHAREDRAM C28
#define CONST_L3RAM C30
#define CONST_DDR C31

#define PRU_ICSS_BASE 0x4a300000
#define CTRL_CONTROL 0x22000
#define CTRL_STATUS 0x22004
#define CTRL_WAKEUP_EN 0x22008
#define CTRL_CYCLE 0x2200c
#define CTRL_STALL 0x22010

// Address for the Constant table Programmable Pointer Register 0(CTPPR_0)
#define CTBIR_0 0x22020
// Address for the Constant table Programmable Pointer Register 0(CTPPR_0)
#define CTBIR_1 0x22024
// Address for the Constant table Programmable Pointer Register 0(CTPPR_0)
#define CTPPR_0 0x22028
// Address for the Constant table Programmable Pointer Register 1(CTPPR_1)
#define CTPPR_1 0x2202C

#define GER_OFFSET 0x10
#define HIESR_OFFSET 0x34
#define SICR_OFFSET 0x24
#define EISR_OFFSET 0x28

#define INTC_CHNMAP_REGS_OFFSET 0x0400
#define INTC_HOSTMAP_REGS_OFFSET 0x0800
#define INTC_HOSTINTPRIO_REGS_OFFSET 0x0900
#define INTC_HOSTNEST_REGS_OFFSET 0x1100

// Bit 5: 1 - Input, 0 - Output
// Bit 4: 1 - Pull up, 0 - Pull down
// Bit 3: 1 - Pull disabled, 0 - Pull enabled
// Bit 2 \_
// Bit 1 |- Mode
// Bit 0 /

// MODE0 - Mux Mode 0
// MODE1 - Mux Mode 1
// MODE2 - Mux Mode 2
// MODE3 - Mux Mode 3
// MODE4 - Mux Mode 4
// MODE5 - Mux Mode 5
// MODE6 - Mux Mode 6
// MODE7 - Mux Mode 7
// IDIS - Receiver disabled
// IEN - Receiver enabled
// PD - Internal pull-down
// PU - Internal pull-up
// OFF - Internal pull disabled

#define MODE0 0
#define MODE1 1
#define MODE2 2
#define MODE3 3
#define MODE4 4
#define MODE5 5
#define MODE6 6
#define MODE7 7
#define IDIS (0 << 5)
#define IEN (1 << 5)
#define PD (0 << 3)
#define PU (2 << 3)
#define OFF (1 << 3)

// To get the physical address the offset has
// to be added to AM335X_CTRL_BASE

#define CONTROL_PADCONF_GPMC_AD0                  0x0800
#define CONTROL_PADCONF_GPMC_AD1                  0x0804
#define CONTROL_PADCONF_GPMC_AD2                  0x0808
#define CONTROL_PADCONF_GPMC_AD3                  0x080C
#define CONTROL_PADCONF_GPMC_AD4                  0x0810
#define CONTROL_PADCONF_GPMC_AD5                  0x0814
#define CONTROL_PADCONF_GPMC_AD6                  0x0818
#define CONTROL_PADCONF_GPMC_AD7                  0x081C
#define CONTROL_PADCONF_GPMC_AD8                  0x0820
#define CONTROL_PADCONF_GPMC_AD9                  0x0824
#define CONTROL_PADCONF_GPMC_AD10                 0x0828
#define CONTROL_PADCONF_GPMC_AD11                 0x082C
#define CONTROL_PADCONF_GPMC_AD12                 0x0830
#define CONTROL_PADCONF_GPMC_AD13                 0x0834
#define CONTROL_PADCONF_GPMC_AD14                 0x0838
#define CONTROL_PADCONF_GPMC_AD15                 0x083C
#define CONTROL_PADCONF_GPMC_A0                   0x0840
#define CONTROL_PADCONF_GPMC_A1                   0x0844
#define CONTROL_PADCONF_GPMC_A2                   0x0848
#define CONTROL_PADCONF_GPMC_A3                   0x084C
#define CONTROL_PADCONF_GPMC_A4                   0x0850
#define CONTROL_PADCONF_GPMC_A5                   0x0854
#define CONTROL_PADCONF_GPMC_A6                   0x0858
#define CONTROL_PADCONF_GPMC_A7                   0x085C
#define CONTROL_PADCONF_GPMC_A8                   0x0860
#define CONTROL_PADCONF_GPMC_A9                   0x0864
#define CONTROL_PADCONF_GPMC_A10                  0x0868
#define CONTROL_PADCONF_GPMC_A11                  0x086C
#define CONTROL_PADCONF_GPMC_WAIT0                0x0870
#define CONTROL_PADCONF_GPMC_WPN                  0x0874
#define CONTROL_PADCONF_GPMC_BEN1                 0x0878
#define CONTROL_PADCONF_GPMC_CSN0                 0x087C
#define CONTROL_PADCONF_GPMC_CSN1                 0x0880
#define CONTROL_PADCONF_GPMC_CSN2                 0x0884
#define CONTROL_PADCONF_GPMC_CSN3                 0x0888
#define CONTROL_PADCONF_GPMC_CLK                  0x088C
#define CONTROL_PADCONF_GPMC_ADVN_ALE             0x0890
#define CONTROL_PADCONF_GPMC_OEN_REN              0x0894
#define CONTROL_PADCONF_GPMC_WEN                  0x0898
#define CONTROL_PADCONF_GPMC_BEN0_CLE             0x089C
#define CONTROL_PADCONF_LCD_DATA0                 0x08A0
#define CONTROL_PADCONF_LCD_DATA1                 0x08A4
#define CONTROL_PADCONF_LCD_DATA2                 0x08A8
#define CONTROL_PADCONF_LCD_DATA3                 0x08AC
#define CONTROL_PADCONF_LCD_DATA4                 0x08B0
#define CONTROL_PADCONF_LCD_DATA5                 0x08B4
#define CONTROL_PADCONF_LCD_DATA6                 0x08B8
#define CONTROL_PADCONF_LCD_DATA7                 0x08BC
#define CONTROL_PADCONF_LCD_DATA8                 0x08C0
#define CONTROL_PADCONF_LCD_DATA9                 0x08C4
#define CONTROL_PADCONF_LCD_DATA10                0x08C8
#define CONTROL_PADCONF_LCD_DATA11                0x08CC
#define CONTROL_PADCONF_LCD_DATA12                0x08D0
#define CONTROL_PADCONF_LCD_DATA13                0x08D4
#define CONTROL_PADCONF_LCD_DATA14                0x08D8
#define CONTROL_PADCONF_LCD_DATA15                0x08DC
#define CONTROL_PADCONF_LCD_VSYNC                 0x08E0
#define CONTROL_PADCONF_LCD_HSYNC                 0x08E4
#define CONTROL_PADCONF_LCD_PCLK                  0x08E8
#define CONTROL_PADCONF_LCD_AC_BIAS_EN            0x08EC
#define CONTROL_PADCONF_MMC0_DAT3                 0x08F0
#define CONTROL_PADCONF_MMC0_DAT2                 0x08F4
#define CONTROL_PADCONF_MMC0_DAT1                 0x08F8
#define CONTROL_PADCONF_MMC0_DAT0                 0x08FC
#define CONTROL_PADCONF_MMC0_CLK                  0x0900
#define CONTROL_PADCONF_MMC0_CMD                  0x0904
#define CONTROL_PADCONF_MII1_COL                  0x0908
#define CONTROL_PADCONF_MII1_CRS                  0x090C
#define CONTROL_PADCONF_MII1_RX_ER                0x0910
#define CONTROL_PADCONF_MII1_TX_EN                0x0914
#define CONTROL_PADCONF_MII1_RX_DV                0x0918
#define CONTROL_PADCONF_MII1_TXD3                 0x091C
#define CONTROL_PADCONF_MII1_TXD2                 0x0920
#define CONTROL_PADCONF_MII1_TXD1                 0x0924
#define CONTROL_PADCONF_MII1_TXD0                 0x0928
#define CONTROL_PADCONF_MII1_TX_CLK               0x092C
#define CONTROL_PADCONF_MII1_RX_CLK               0x0930
#define CONTROL_PADCONF_MII1_RXD3                 0x0934
#define CONTROL_PADCONF_MII1_RXD2                 0x0938
#define CONTROL_PADCONF_MII1_RXD1                 0x093C
#define CONTROL_PADCONF_MII1_RXD0                 0x0940
#define CONTROL_PADCONF_RMII1_REF_CLK             0x0944
#define CONTROL_PADCONF_MDIO                      0x0948
#define CONTROL_PADCONF_MDC                       0x094C
#define CONTROL_PADCONF_SPI0_SCLK                 0x0950
#define CONTROL_PADCONF_SPI0_D0                   0x0954
#define CONTROL_PADCONF_SPI0_D1                   0x0958
#define CONTROL_PADCONF_SPI0_CS0                  0x095C
#define CONTROL_PADCONF_SPI0_CS1                  0x0960
#define CONTROL_PADCONF_ECAP0_IN_PWM0_OUT         0x0964
#define CONTROL_PADCONF_UART0_CTSN                0x0968
#define CONTROL_PADCONF_UART0_RTSN                0x096C
#define CONTROL_PADCONF_UART0_RXD                 0x0970
#define CONTROL_PADCONF_UART0_TXD                 0x0974
#define CONTROL_PADCONF_UART1_CTSN                0x0978
#define CONTROL_PADCONF_UART1_RTSN                0x097C
#define CONTROL_PADCONF_UART1_RXD                 0x0980
#define CONTROL_PADCONF_UART1_TXD                 0x0984
#define CONTROL_PADCONF_I2C0_SDA                  0x0988
#define CONTROL_PADCONF_I2C0_SCL                  0x098C
#define CONTROL_PADCONF_MCASP0_ACLKX              0x0990
#define CONTROL_PADCONF_MCASP0_FSX                0x0994
#define CONTROL_PADCONF_MCASP0_AXR0               0x0998
#define CONTROL_PADCONF_MCASP0_AHCLKR             0x099C
#define CONTROL_PADCONF_MCASP0_ACLKR              0x09A0
#define CONTROL_PADCONF_MCASP0_FSR                0x09A4
#define CONTROL_PADCONF_MCASP0_AXR1               0x09A8
#define CONTROL_PADCONF_MCASP0_AHCLKX             0x09AC
#define CONTROL_PADCONF_XDMA_EVENT_INTR0          0x09B0
#define CONTROL_PADCONF_XDMA_EVENT_INTR1          0x09B4
#define CONTROL_PADCONF_WARMRSTN                  0x09B8
#define CONTROL_PADCONF_PWRONRSTN                 0x09BC
#define CONTROL_PADCONF_EXTINTN                   0x09C0
#define CONTROL_PADCONF_XTALIN                    0x09C4
#define CONTROL_PADCONF_XTALOUT                   0x09C8
#define CONTROL_PADCONF_TMS                       0x09D0
#define CONTROL_PADCONF_TDI                       0x09D4
#define CONTROL_PADCONF_TDO                       0x09D8
#define CONTROL_PADCONF_TCK                       0x09DC
#define CONTROL_PADCONF_TRSTN                     0x09E0
#define CONTROL_PADCONF_EMU0                      0x09E4
#define CONTROL_PADCONF_EMU1                      0x09E8
#define CONTROL_PADCONF_RTC_XTALIN                0x09EC
#define CONTROL_PADCONF_RTC_XTALOUT               0x09F0
#define CONTROL_PADCONF_RTC_PWRONRSTN             0x09F8
#define CONTROL_PADCONF_PMIC_POWER_EN             0x09FC
#define CONTROL_PADCONF_EXT_WAKEUP                0x0A00
#define CONTROL_PADCONF_RTC_KALDO_ENN             0x0A04
#define CONTROL_PADCONF_USB0_DM                   0x0A08
#define CONTROL_PADCONF_USB0_DP                   0x0A0C
#define CONTROL_PADCONF_USB0_CE                   0x0A10
#define CONTROL_PADCONF_USB0_ID                   0x0A14
#define CONTROL_PADCONF_USB0_VBUS                 0x0A18
#define CONTROL_PADCONF_USB0_DRVVBUS              0x0A1C
#define CONTROL_PADCONF_USB1_DM                   0x0A20
#define CONTROL_PADCONF_USB1_DP                   0x0A24
#define CONTROL_PADCONF_USB1_CE                   0x0A28
#define CONTROL_PADCONF_USB1_ID                   0x0A2C
#define CONTROL_PADCONF_USB1_VBUS                 0x0A30
#define CONTROL_PADCONF_USB1_DRVVBUS              0x0A34
#define CONTROL_PADCONF_DDR_RESETN                0x0A38
#define CONTROL_PADCONF_DDR_CSN0                  0x0A3C
#define CONTROL_PADCONF_DDR_CKE                   0x0A40
#define CONTROL_PADCONF_DDR_CK                    0x0A44
#define CONTROL_PADCONF_DDR_CKN                   0x0A48
#define CONTROL_PADCONF_DDR_CASN                  0x0A4C
#define CONTROL_PADCONF_DDR_RASN                  0x0A50
#define CONTROL_PADCONF_DDR_WEN                   0x0A54
#define CONTROL_PADCONF_DDR_BA0                   0x0A58
#define CONTROL_PADCONF_DDR_BA1                   0x0A5C
#define CONTROL_PADCONF_DDR_BA2                   0x0A60
#define CONTROL_PADCONF_DDR_A0                    0x0A64
#define CONTROL_PADCONF_DDR_A1                    0x0A68
#define CONTROL_PADCONF_DDR_A2                    0x0A6C
#define CONTROL_PADCONF_DDR_A3                    0x0A70
#define CONTROL_PADCONF_DDR_A4                    0x0A74
#define CONTROL_PADCONF_DDR_A5                    0x0A78
#define CONTROL_PADCONF_DDR_A6                    0x0A7C
#define CONTROL_PADCONF_DDR_A7                    0x0A80
#define CONTROL_PADCONF_DDR_A8                    0x0A84
#define CONTROL_PADCONF_DDR_A9                    0x0A88
#define CONTROL_PADCONF_DDR_A10                   0x0A8C
#define CONTROL_PADCONF_DDR_A11                   0x0A90
#define CONTROL_PADCONF_DDR_A12                   0x0A94
#define CONTROL_PADCONF_DDR_A13                   0x0A98
#define CONTROL_PADCONF_DDR_A14                   0x0A9C
#define CONTROL_PADCONF_DDR_A15                   0x0AA0
#define CONTROL_PADCONF_DDR_ODT                   0x0AA4
#define CONTROL_PADCONF_DDR_D0                    0x0AA8
#define CONTROL_PADCONF_DDR_D1                    0x0AAC
#define CONTROL_PADCONF_DDR_D2                    0x0AB0
#define CONTROL_PADCONF_DDR_D3                    0x0AB4
#define CONTROL_PADCONF_DDR_D4                    0x0AB8
#define CONTROL_PADCONF_DDR_D5                    0x0ABC
#define CONTROL_PADCONF_DDR_D6                    0x0AC0
#define CONTROL_PADCONF_DDR_D7                    0x0AC4
#define CONTROL_PADCONF_DDR_D8                    0x0AC8
#define CONTROL_PADCONF_DDR_D9                    0x0ACC
#define CONTROL_PADCONF_DDR_D10                   0x0AD0
#define CONTROL_PADCONF_DDR_D11                   0x0AD4
#define CONTROL_PADCONF_DDR_D12                   0x0AD8
#define CONTROL_PADCONF_DDR_D13                   0x0ADC
#define CONTROL_PADCONF_DDR_D14                   0x0AE0
#define CONTROL_PADCONF_DDR_D15                   0x0AE4
#define CONTROL_PADCONF_DDR_DQM0                  0x0AE8
#define CONTROL_PADCONF_DDR_DQM1                  0x0AEC
#define CONTROL_PADCONF_DDR_DQS0                  0x0AF0
#define CONTROL_PADCONF_DDR_DQSN0                 0x0AF4
#define CONTROL_PADCONF_DDR_DQS1                  0x0AF8
#define CONTROL_PADCONF_DDR_DQSN1                 0x0AFC
#define CONTROL_PADCONF_DDR_VREF                  0x0B00
#define CONTROL_PADCONF_DDR_VTP                   0x0B04
#define CONTROL_PADCONF_AIN7                      0x0B10
#define CONTROL_PADCONF_AIN6                      0x0B14
#define CONTROL_PADCONF_AIN5                      0x0B18
#define CONTROL_PADCONF_AIN4                      0x0B1C
#define CONTROL_PADCONF_AIN3                      0x0B20
#define CONTROL_PADCONF_AIN2                      0x0B24
#define CONTROL_PADCONF_AIN1                      0x0B28
#define CONTROL_PADCONF_AIN0                      0x0B2C
#define CONTROL_PADCONF_VREFP                     0x0B30
#define CONTROL_PADCONF_VREFN                     0x0B34

#define AM335X_CTRL_BASE 0x44E10000


#define GPIO0 0x44e07000
#define GPIO1 0x4804c000
#define GPIO2 0x481ac000
#define GPIO3 0x481ae000

#define GPIO_REVISION 0x0
#define GPIO_SYSCONFIG 0x10
#define GPIO_IRQSTATUS_RAW_0 0x24
#define GPIO_IRQSTATUS_RAW_1 0x28
#define GPIO_IRQSTATUS_0 0x2C
#define GPIO_IRQSTATUS_1 0x30
#define GPIO_IRQSTATUS_SET_0 0x34
#define GPIO_IRQSTATUS_SET_1 0x38
#define GPIO_IRQSTATUS_CLR_0 0x3C
#define GPIO_IRQSTATUS_CLR_1 0x40
#define GPIO_IRQWAKEN_0 0x44
#define GPIO_IRQWAKEN_1 0x48
#define GPIO_SYSSTATUS 0x114
#define GPIO_CTRL 0x130
#define GPIO_OE 0x134
#define GPIO_DATAIN 0x138
#define GPIO_DATAOUT 0x13C
#define GPIO_LEVELDETECT0 0x140
#define GPIO_LEVELDETECT1 0x144
#define GPIO_RISINGDETECT 0x148
#define GPIO_FALLINGDETECT 0x14C
#define GPIO_DEBOUNCENABLE 0x150
#define GPIO_DEBOUNCINGTIME 0x154
#define GPIO_CLEARDATAOUT 0x190
#define GPIO_SETDATAOUT 0x194





#endif // _pru_H_

