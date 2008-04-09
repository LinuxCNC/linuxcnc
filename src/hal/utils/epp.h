
#include <stdint.h>
#include <sys/io.h>


#define EPP_STATUS_OFFSET   (1)
#define EPP_CONTROL_OFFSET  (2)
#define EPP_ADDRESS_OFFSET  (3)
#define EPP_DATA_OFFSET     (4)

#define ECP_CONFIG_A_HIGH_OFFSET  (0)
#define ECP_CONFIG_B_HIGH_OFFSET  (1)
#define ECP_CONTROL_HIGH_OFFSET   (2)


struct epp {
    uint16_t io_addr;
    uint16_t io_addr_hi;
};


void epp_init(struct epp *epp);
void epp_addr8(struct epp *epp, uint8_t addr);
void epp_addr16(struct epp *epp, uint16_t addr);
void epp_write(struct epp *epp, uint8_t val);
int epp_read(struct epp *epp);
uint8_t epp_read_status(struct epp *epp);
void epp_write_status(struct epp *epp, uint8_t status_byte);
void epp_write_control(struct epp *epp, uint8_t control_byte);
int epp_check_for_timeout(struct epp *epp);
int epp_clear_timeout(struct epp *epp);

