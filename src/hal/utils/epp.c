
#include "epp.h"


void epp_addr8(struct epp *epp, uint8_t addr) {
    outb(addr, epp->io_addr + EPP_ADDRESS_OFFSET);
}

void epp_addr16(struct epp *epp, uint16_t addr) {
    outb((addr & 0x00FF), epp->io_addr + EPP_ADDRESS_OFFSET);
    outb((addr >> 8),     epp->io_addr + EPP_ADDRESS_OFFSET);
}

void epp_write(struct epp *epp, uint8_t val) {
    outb(val, epp->io_addr + EPP_DATA_OFFSET);
}

int epp_read(struct epp *epp) {
    return inb(epp->io_addr + EPP_DATA_OFFSET);
}

uint8_t epp_read_status(struct epp *epp) {
    return inb(epp->io_addr + EPP_STATUS_OFFSET);
}

void epp_write_status(struct epp *epp, uint8_t status_byte) {
    outb(status_byte, epp->io_addr + EPP_STATUS_OFFSET);
}

void epp_write_control(struct epp *epp, uint8_t control_byte) {
    outb(control_byte, epp->io_addr + EPP_CONTROL_OFFSET);
}

int epp_check_for_timeout(struct epp *epp) {
    return (epp_read_status(epp) & 0x01);
}

int epp_clear_timeout(struct epp *epp) {
    uint8_t status;

    if (!epp_check_for_timeout(epp)) {
        return 1;
    }

    /* To clear timeout some chips require double read */
    (void)epp_read_status(epp);

    // read in the actual status register
    status = epp_read_status(epp);

    epp_write_status(epp, status | 0x01);  // Some reset by writing 1
    epp_write_status(epp, status & 0xFE);  // Others by writing 0

    return !epp_check_for_timeout(epp);
}

