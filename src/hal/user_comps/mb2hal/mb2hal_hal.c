#include "mb2hal.h"

retCode create_HAL_pins()
{
    char *fnct_name = "create_HAL_pins";
    int tx_counter;

    for (tx_counter = 0; tx_counter < gbl_n_mb_tx; tx_counter++) {
        if (create_each_mb_tx_hal_pins(&gbl_mb_tx[tx_counter]) != retOK) {
            ERR(init_debug, "failed to initialize hal pins in tx_num[%d] [%d] [%s]",
                tx_counter, gbl_mb_tx[tx_counter].mb_tx_code, gbl_mb_tx[tx_counter].cfg_mb_tx_code_name);
            return retERR;
        }
    }

    return retOK;
}

retCode create_each_mb_tx_hal_pins(mb_tx_t *mb_tx)
{
    char *fnct_name = "create_each_mb_tx_hal_pins";
    char hal_pin_name[HAL_NAME_LEN + 1];
    int pin_counter;

    if (mb_tx == NULL) {
        ERR(init_debug, "NULL pointer");
        return retERR;
    }

    switch (mb_tx->mb_tx_code) {

    case mbtx_02_READ_DISCRETE_INPUTS:
    case mbtx_15_WRITE_MULTIPLE_COILS:
        mb_tx->bit = hal_malloc(sizeof(hal_bit_t *) * mb_tx->mb_nelements);
        if (mb_tx->bit == NULL) {
            ERR(init_debug, "[%d] [%s] NULL hal_malloc [%d] elements",
                mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, mb_tx->mb_nelements);
            return retERR;
        }
        memset(mb_tx->bit, 0, sizeof(hal_bit_t *) * mb_tx->mb_nelements);
        break;

    case mbtx_03_READ_HOLDING_REGISTERS:
    case mbtx_04_READ_INPUT_REGISTERS:
    case mbtx_16_WRITE_MULTIPLE_REGISTERS:
        mb_tx->float_value= hal_malloc(sizeof(hal_float_t *) * mb_tx->mb_nelements);
        mb_tx->int_value  = hal_malloc(sizeof(hal_s32_t *) * mb_tx->mb_nelements);
        //mb_tx->scale      = hal_malloc(sizeof(hal_float_t) * mb_tx->mb_nelements);
        //mb_tx->offset     = hal_malloc(sizeof(hal_float_t) * mb_tx->mb_nelements);
        //if (mb_tx->float_value == NULL || mb_tx->int_value == NULL
        //        || mb_tx->scale == NULL || mb_tx->offset == NULL) {
        if (mb_tx->float_value == NULL || mb_tx->int_value == NULL) {
            ERR(init_debug, "[%d] [%s] NULL hal_malloc [%d] elements",
                mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, mb_tx->mb_nelements);
            return retERR;
        }
        memset(mb_tx->float_value,     0, sizeof(hal_float_t *) * mb_tx->mb_nelements);
        memset(mb_tx->int_value,       0, sizeof(hal_s32_t *)   * mb_tx->mb_nelements);
        //memset((void *) mb_tx->scale,  0, sizeof(hal_float_t)   * mb_tx->mb_nelements);
        //memset((void *) mb_tx->offset, 0, sizeof(hal_float_t)   * mb_tx->mb_nelements);
        break;

    default:
        ERR(init_debug, "[%d] wrong mb_tx_code", mb_tx->mb_tx_code);
        return retERR;
        break;
    }

    for (pin_counter = 0; pin_counter < mb_tx->mb_nelements; pin_counter++) {

        snprintf(hal_pin_name, HAL_NAME_LEN, "%s.%s.%02d", hal_mod_name, mb_tx->hal_tx_name, pin_counter);
        DBG(init_debug, "mb_tx_num [%d] pin_name [%s]", mb_tx->mb_tx_num, hal_pin_name);

        switch (mb_tx->mb_tx_code) {
        case mbtx_15_WRITE_MULTIPLE_COILS:
            if (0 != hal_pin_bit_newf(HAL_IN, mb_tx->bit + pin_counter, hal_mod_id,
                                      "%s", hal_pin_name)) {
                ERR(init_debug, "[%d] [%s] [%s] hal_pin_bit_newf failed", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
                return retERR;
            }
            *mb_tx->bit[pin_counter] = 0;
            break;
        case mbtx_02_READ_DISCRETE_INPUTS:
            if (0 != hal_pin_bit_newf(HAL_OUT, mb_tx->bit + pin_counter, hal_mod_id,
                                      "%s", hal_pin_name)) {
                ERR(init_debug, "[%d] [%s] [%s] hal_pin_bit_newf failed", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
                return retERR;
            }
            *mb_tx->bit[pin_counter] = 0;
            break;
        case mbtx_04_READ_INPUT_REGISTERS:
        case mbtx_03_READ_HOLDING_REGISTERS:
            if (0 != hal_pin_float_newf(HAL_OUT, mb_tx->float_value + pin_counter, hal_mod_id,
                                        "%s.float", hal_pin_name)) {
                ERR(init_debug, "[%d] [%s] [%s] hal_pin_float_newf failed", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
                return retERR;
            }
            if (0 != hal_pin_s32_newf(HAL_OUT, mb_tx->int_value + pin_counter, hal_mod_id,
                                      "%s.int", hal_pin_name)) {
                ERR(init_debug, "[%d] [%s] [%s] hal_pin_s32_newf failed", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
                return retERR;
            }
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->scale + pin_counter, hal_mod_id,
            //                              "%s.scale", hal_pin_name)) {
            //    ERR(init_debug, "[%d] [%s] [%s]", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
            //    return retERR;
            //}
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->offset + pin_counter, hal_mod_id,
            //                              "%s.offset", hal_pin_name)) {
            //    ERR(init_debug, "[%d] [%s] [%s]", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
            //    return retERR;
            //}
            *mb_tx->float_value[pin_counter] = 0;
            *mb_tx->int_value[pin_counter] = 0;
            //mb_tx->scale[pin_counter] = 1;
            //mb_tx->offset[pin_counter] = 0;
            break;
        case mbtx_16_WRITE_MULTIPLE_REGISTERS:
            if (0 != hal_pin_float_newf(HAL_IN, mb_tx->float_value + pin_counter, hal_mod_id,
                                        "%s", hal_pin_name)) {
                ERR(init_debug, "[%d] [%s] [%s] hal_pin_float_newf failed", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
                return retERR;
            }
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->scale + pin_counter, hal_mod_id,
            //                              "%s.scale", hal_pin_name)) {
            //    ERR(init_debug, "[%d] [%s] [%s]", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name, hal_pin_name);
            //    return retERR;
            //}
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->offset + pin_counter, hal_mod_id,
            //                              "%s.offset", hal_pin_name)) {
            //    ERR(init_debug, "[%d] [%s]", mb_tx->mb_tx_code, mb_tx->cfg_mb_tx_code_name);
            //    return retERR;
            //}
            *mb_tx->float_value[pin_counter] = 0;
            //*mb_tx->int_value[pin_counter] = 0;
            //mb_tx->scale[pin_counter] = 1;
            //mb_tx->offset[pin_counter] = 0;
            break;
        default:
            ERR(init_debug, "[%d]", mb_tx->mb_tx_code);
            return retERR;
            break;
        }
    }

    return retOK;
}
