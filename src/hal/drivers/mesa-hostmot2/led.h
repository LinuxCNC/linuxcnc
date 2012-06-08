//
// On-board LEDs
//

typedef struct {
    hal_bit_t *led;
} hm2_led_instance_t ;

typedef struct {
    int num_instances ;
    hm2_led_instance_t *instance;

    u32 written_buff ;

    u32 led_addr;
    u32 *led_reg;
} hm2_led_t ;

int hm2_led_parse_md(hostmot2_t *hm2, int md_index);


