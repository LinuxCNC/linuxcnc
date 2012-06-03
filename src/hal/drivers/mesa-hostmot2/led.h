//
// On-board LEDs
//

typedef struct {
    hal_bit_t *led;
} hm2_led_hal_t ;

typedef struct {
    int num_instances ;

    u32 written_buff ;

    u32 led_addr;
    u32 *led_reg;
} hm2_led_t ;


