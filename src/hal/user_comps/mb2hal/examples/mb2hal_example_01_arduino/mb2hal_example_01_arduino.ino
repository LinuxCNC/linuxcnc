/*
 * mb2hal_example_01_arduino.ino
 * Arduino sketch to connect to HAL's LinuxCNC using mb2hal component.
 * Tested 2012-10-12 with Arduino Mega 2560 R3.
 *
 * Victor Rocco, adapted from Stéphane Raimbault's source code wich is
 * Copyright © 2011-2012 Stéphane Raimbault <stephane.raimbault@gmail.com>
 * License ISC, see LICENSE for more details.
 */

#include <HardwareSerial.h>
#include <Modbusino.h>

/* Initialize the slave with the ID 1 */
ModbusinoSlave modbusino_slave(1);
/* Allocate a mapping of 10 values */
uint16_t tab_reg[10];

void setup() {
    /* The transfer speed is set to 115200 bauds */
    modbusino_slave.setup(115200);
}

void loop() {
    /* Launch Modbus slave loop with:
       - pointer to the mapping
       - max values of mapping */
    modbusino_slave.loop(tab_reg, 10);
    /* Copy the INPUT registers (5-9) to the OUTPUT registers (0-4) */
    tab_reg[0] = tab_reg[5];
    tab_reg[1] = tab_reg[6];
    tab_reg[2] = tab_reg[7];
    tab_reg[3] = tab_reg[8];
    tab_reg[4] = tab_reg[9];
}
