#include "mb2hal.h"

retCode parse_main_args(int argc, char **argv)
{
    char *fnct_name = "parse_main_args";
    char *arg_filename;
    char *tmp;

    if (argc != 2) {
        ERR(gbl.init_dbg, "wrong number of args");
        return retERR;
    }

    if (strncmp(argv[1], "config=", 7) != 0) { //has config file
        ERR(gbl.init_dbg, "config parameter not found");
        return retERR;
    }

    arg_filename = argv[1];
    arg_filename += 7;

    if (*arg_filename == '\"') { //file name is quoted
        arg_filename++;
        tmp = arg_filename;
        while (*tmp != '\"' && *tmp != 0) {
            tmp++;
        }
        *tmp = 0; //remove trailing quote
    }
    gbl.ini_file_path = strdup(arg_filename);

    return retOK;
}

retCode parse_ini_file()
{
    char *fnct_name = "parse_ini_file";
    int counter;

    if (gbl.ini_file_ptr == NULL) {
        ERR(gbl.init_dbg, "gbl.ini_file_ptr NULL pointer");
        return retERR;
    }

    if (parse_common_section() != retOK) {
        ERR(gbl.init_dbg, "parse_common_section failed");
        return retERR;
    }

    //default = one link per transaction
    //realloc will be used if there are common links
    gbl.mb_links = malloc(sizeof(mb_link_t) * gbl.tot_mb_tx);
    if (gbl.mb_links == NULL) {
        ERR(gbl.init_dbg, "malloc gbl.mb_links failed [%s]", strerror(errno));
        return retERR;
    }
    memset(gbl.mb_links, 0, sizeof(mb_link_t) * gbl.tot_mb_tx);

    gbl.mb_tx = malloc(sizeof(mb_tx_t) * gbl.tot_mb_tx);
    if (gbl.mb_tx == NULL) {
        ERR(gbl.init_dbg, "malloc gbl.mb_tx failed [%s]", strerror(errno));
        return retERR;
    }
    memset(gbl.mb_tx, 0, sizeof(mb_tx_t) * gbl.tot_mb_tx);

    for (counter = 0; counter < gbl.tot_mb_tx; counter++) {
        if (parse_transaction_section(counter) != retOK) {
            ERR(gbl.init_dbg, "parse_transaction_section %d failed", counter);
            return retERR;
        }
        OK(gbl.init_dbg, "parse_transaction_section %d OK", counter);
    }

    return retOK;
}

retCode parse_common_section()
{
    char *fnct_name = "parse_common_section";
    char *section = "MB2HAL_INIT", *tag;
    const char *tmpstr;

    if (gbl.ini_file_ptr == NULL) {
        ERR(gbl.init_dbg, "gbl.ini_file_ptr NULL pointer");
        return retERR;
    }

    tag     = "INIT_DEBUG"; //optional
    iniFindInt(gbl.ini_file_ptr, tag, section, &gbl.init_dbg);
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, gbl.init_dbg);

    tag    = "HAL_MODULE_NAME"; //optional
    tmpstr = iniFind(gbl.ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        gbl.hal_mod_name = strdup(tmpstr);
    }
    //else already initilizaed by default
    DBG(gbl.init_dbg, "[%s] [%s] [%s]", section, tag, gbl.hal_mod_name);

    tag     = "SLOWDOWN"; //optional
    iniFindDouble(gbl.ini_file_ptr, tag, section, &gbl.slowdown);
    DBG(gbl.init_dbg, "[%s] [%s] [%0.3f]", section, tag, gbl.slowdown);

    tag     = "TOTAL_TRANSACTIONS"; //required
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &gbl.tot_mb_tx) != 0) {
        ERR(gbl.init_dbg, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (gbl.tot_mb_tx <= 0) {
        ERR(gbl.init_dbg, "[%s] [%s] [%d], must be > 0", section, tag, gbl.tot_mb_tx);
        return retERR;
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, gbl.tot_mb_tx);

    return retOK;
}


retCode parse_transaction_section(const int mb_tx_num)
{
    char *fnct_name = "parse_transaction_section";
    char section[20];
    char *tag;
    const char *tmpstr;
    mb_tx_t *this_mb_tx;

    if (gbl.ini_file_ptr == NULL) {
        ERR(gbl.init_dbg, "gbl.ini_file_ptr NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > gbl.tot_mb_tx) {
        ERR(gbl.init_dbg, "out of range");
        return retERR;
    }

    this_mb_tx = &gbl.mb_tx[mb_tx_num];

    if (gbl.ini_file_ptr == NULL || mb_tx_num < 0 || mb_tx_num > gbl.tot_mb_tx) {
        ERR(gbl.init_dbg, "parameter error");
        return retERR;
    }

    snprintf(section, sizeof(section)-1, "TRANSACTION_%02d", mb_tx_num);

    tag = "LINK_TYPE"; //required 1st time, then optional
    tmpstr = iniFind(gbl.ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        if (strcasecmp(tmpstr, "tcp") == retOK) {
            this_mb_tx->cfg_link_type = linkTCP;
            strcpy(this_mb_tx->cfg_link_type_str, tmpstr);
        }
        else if (strcasecmp(tmpstr, "serial") == retOK) {
            this_mb_tx->cfg_link_type = linkRTU;
            strcpy(this_mb_tx->cfg_link_type_str, tmpstr);
        }
        else {
            this_mb_tx->cfg_link_type = -1;
            strcpy(this_mb_tx->cfg_link_type_str, "");
            ERR(gbl.init_dbg, "[%s] [%s] [%s] is not valid", section, tag, tmpstr);
            return retERR;
        }
    }
    else {
        if (mb_tx_num > 0) { //previous value
            this_mb_tx->cfg_link_type = gbl.mb_tx[mb_tx_num-1].cfg_link_type;
            strcpy(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str);
        }
        else { //required 1rst time
            this_mb_tx->cfg_link_type = -1;
            strcpy(this_mb_tx->cfg_link_type_str, "");
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%s] [%d]", section, tag, this_mb_tx->cfg_link_type_str, this_mb_tx->cfg_link_type);

    if (this_mb_tx->cfg_link_type == linkTCP) { //tcp
        if (parse_tcp_subsection(section, mb_tx_num) != retOK) {
            ERR(gbl.init_dbg, "parsing error");
            return retERR;
        }
    }
    else { //serial
        if (parse_serial_subsection(section, mb_tx_num) != retOK) {
            ERR(gbl.init_dbg, "parsing error");
            return retERR;
        }
    }

    tag = "MB_SLAVE_ID"; //1st time required
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->mb_tx_slave_id) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->mb_tx_slave_id = gbl.mb_tx[mb_tx_num-1].mb_tx_slave_id;
            }
            else {
                this_mb_tx->mb_tx_slave_id = -1;
                ERR(gbl.init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->mb_tx_slave_id = -1;
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_slave_id);

    tag = "FIRST_ELEMENT"; //required
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->mb_tx_1st_addr) != 0) {
        ERR(gbl.init_dbg, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (this_mb_tx->mb_tx_1st_addr < 0) {
        ERR(gbl.init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->mb_tx_1st_addr);
        return retERR;
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_1st_addr);

    tag = "NELEMENTS";  //required
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->mb_tx_nelem) != 0) {
        ERR(gbl.init_dbg, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem < 1) {
        ERR(gbl.init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->mb_tx_nelem);
        return retERR;
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_nelem);

    tag = "MAX_UPDATE_RATE"; //optional
    this_mb_tx->cfg_update_rate = 0; //default: 0=infinit
    if (iniFindDouble(gbl.ini_file_ptr, tag, section, &this_mb_tx->cfg_update_rate) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_update_rate = gbl.mb_tx[mb_tx_num-1].cfg_update_rate;
            }
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%0.3f]", section, tag, this_mb_tx->cfg_update_rate);

    tag = "MB_RESPONSE_TIMEOUT_MS"; //optional
    this_mb_tx->mb_response_timeout_ms = MB2HAL_DEFAULT_MB_RESPONSE_TIMEOUT_MS; //default
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->mb_response_timeout_ms) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->mb_response_timeout_ms = gbl.mb_tx[mb_tx_num-1].mb_response_timeout_ms;
            }
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_response_timeout_ms);

    tag = "MB_BYTE_TIMEOUT_MS"; //optional
    this_mb_tx->mb_byte_timeout_ms = MB2HAL_DEFAULT_MB_BYTE_TIMEOUT_MS; //default
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->mb_byte_timeout_ms) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->mb_byte_timeout_ms = gbl.mb_tx[mb_tx_num-1].mb_byte_timeout_ms;
            }
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_byte_timeout_ms);

    tag = "DEBUG"; //optional
    this_mb_tx->cfg_debug = debugERR; //default
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->cfg_debug) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_debug = gbl.mb_tx[mb_tx_num-1].cfg_debug;
            }
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_debug);

    tag = "MB_TX_CODE"; //required
    tmpstr = iniFind(gbl.ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        int i;
        for (i=0 ; i<mbtxMAX; i++) {
            if (strcasecmp(tmpstr, gbl.mb_tx_fncts[i]) == 0) {
                this_mb_tx->mb_tx_fnct = i;
                strncpy(this_mb_tx->mb_tx_fnct_name, tmpstr, sizeof(this_mb_tx->mb_tx_fnct_name)-1);
                break;
            }
        }
        if (this_mb_tx->mb_tx_fnct <= mbtxERR || this_mb_tx->mb_tx_fnct >= mbtxMAX) {
            ERR(gbl.init_dbg, "[%s] [%s] [%s] out of range", section, tag, tmpstr);
            return retERR;
        }
    }
    else {
        ERR(gbl.init_dbg, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_fnct_name, this_mb_tx->mb_tx_fnct);

    tag = "HAL_TX_NAME"; //optional
    tmpstr = iniFind(gbl.ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->hal_tx_name, tmpstr, HAL_NAME_LEN);
    }
    else {
        sprintf(this_mb_tx->hal_tx_name, "%02d", mb_tx_num);
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->hal_tx_name);

    /*
        str = iniFind(gbl.ini_file_ptr, "PINNAME", mb_tx_name);
        if (str != NULL) {
            pin_name = malloc(strlen(str) + 1);
            strcpy(pin_name, str);	// convert a const string into one
            // we can modify
        }
        else {
            pin_name = malloc(1);	// empty string
            *pin_name = 0;
        }
        if (mb_tx->name[0] != 0) {
            strncpy(mb_tx_name, mb_tx->name, HAL_NAME_LEN);
        }
        else {
            sprintf(mb_tx_name, "%02d", mb_tx_num);
        }
        memcpy(&gbl.mb_tx[mb_tx_num], mb_tx, sizeof(mb_tx_t));
        rc = create_pins(mb_tx_name, &gbl.mb_tx[mb_tx_num], pin_name);
        free(pin_name);
        if (rc != retOK) {
            ERR(gbl.init_dbg, "Failed to create pins");
            return retERR;
        }
    */

    return retOK;
}

retCode parse_tcp_subsection(const char *section, const int mb_tx_num)
{
    char *fnct_name="parse_tcp_subsection";
    char *tag;
    const char *tmpstr;
    mb_tx_t *this_mb_tx;

    if (gbl.ini_file_ptr == NULL || section == NULL) {
        ERR(gbl.init_dbg, "gbl.ini_file_ptr NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > gbl.tot_mb_tx) {
        ERR(gbl.init_dbg, "out of range");
        return retERR;
    }

    this_mb_tx = &gbl.mb_tx[mb_tx_num];

    tag = "TCP_IP"; //required 1st time, then optional
    tmpstr = iniFind(gbl.ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->cfg_tcp_ip, tmpstr, sizeof(this_mb_tx->cfg_tcp_ip)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                strcpy(this_mb_tx->cfg_tcp_ip, gbl.mb_tx[mb_tx_num-1].cfg_tcp_ip);
            }
            else {
                strcpy(this_mb_tx->cfg_tcp_ip, "");
                ERR(gbl.init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            strcpy(this_mb_tx->cfg_tcp_ip, "");
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->cfg_tcp_ip);

    tag = "TCP_PORT"; //optional
    this_mb_tx->cfg_tcp_port = MB2HAL_DEFAULT_TCP_PORT; //default
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->cfg_tcp_port) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_tcp_port = gbl.mb_tx[mb_tx_num-1].cfg_tcp_port;
            }
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_tcp_port);

    return retOK;
}

retCode parse_serial_subsection(const char *section, const int mb_tx_num)
{
    char *fnct_name="parse_serial_subsection";
    char *tag;
    const char *tmpstr;
    mb_tx_t *this_mb_tx;

    if (gbl.ini_file_ptr == NULL || section == NULL) {
        ERR(gbl.init_dbg, "gbl.ini_file_ptr NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > gbl.tot_mb_tx) {
        ERR(gbl.init_dbg, "out of range");
        return retERR;
    }

    this_mb_tx = &gbl.mb_tx[mb_tx_num];

    tag = "SERIAL_PORT"; //required 1st time
    tmpstr = iniFind(gbl.ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->cfg_serial_device, tmpstr, sizeof(this_mb_tx->cfg_serial_device)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                strcpy(this_mb_tx->cfg_serial_device, gbl.mb_tx[mb_tx_num-1].cfg_serial_device);
            }
            else {
                strcpy(this_mb_tx->cfg_serial_device, "");
                ERR(gbl.init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            strcpy(this_mb_tx->cfg_serial_device, "");
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->cfg_serial_device);

    tag = "SERIAL_BAUD"; //1st time required
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_baud) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_baud = gbl.mb_tx[mb_tx_num-1].cfg_serial_baud;
            }
            else {
                this_mb_tx->cfg_serial_baud = -1;
                ERR(gbl.init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->cfg_serial_baud = -1;
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_baud);

    tag = "SERIAL_BITS"; //1st time required
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_data_bit) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_data_bit = gbl.mb_tx[mb_tx_num-1].cfg_serial_data_bit;
            }
            else {
                this_mb_tx->cfg_serial_data_bit = -1;
                ERR(gbl.init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->cfg_serial_data_bit = -1;
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_int_in(4, this_mb_tx->cfg_serial_data_bit, 5, 6, 7, 8) != retOK) {
        ERR(gbl.init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->cfg_serial_data_bit);
        return retERR;
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_data_bit);

    tag = "SERIAL_PARITY"; //required 1st time
    tmpstr = iniFind(gbl.ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->cfg_serial_parity, tmpstr, sizeof(this_mb_tx->cfg_serial_parity)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                strcpy(this_mb_tx->cfg_serial_parity, gbl.mb_tx[mb_tx_num-1].cfg_serial_parity);
            }
            else {
                strcpy(this_mb_tx->cfg_serial_parity, "");
                ERR(gbl.init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            strcpy(this_mb_tx->cfg_serial_parity, "");
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_str_in(3, this_mb_tx->cfg_serial_parity, "even", "odd", "none") != retOK) {
        ERR(gbl.init_dbg, "[%s] [%s] [%s] out of range", section, tag, this_mb_tx->cfg_serial_parity);
        return retERR;
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->cfg_serial_parity);

    tag = "SERIAL_STOP"; //1st time required
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_stop_bit) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_stop_bit = gbl.mb_tx[mb_tx_num-1].cfg_serial_stop_bit;
            }
            else {
                this_mb_tx->cfg_serial_stop_bit = -1;
                ERR(gbl.init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->cfg_serial_stop_bit = -1;
            ERR(gbl.init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_int_in(2, this_mb_tx->cfg_serial_stop_bit, 1, 2) != retOK) {
        ERR(gbl.init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->cfg_serial_stop_bit);
        return retERR;
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_stop_bit);

    tag = "SERIAL_DELAY_MS"; //optional
    this_mb_tx->cfg_serial_delay_ms = 0; //default
    if (iniFindInt(gbl.ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_delay_ms) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, gbl.mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_delay_ms = gbl.mb_tx[mb_tx_num-1].cfg_serial_delay_ms;
            }
        }
    }
    DBG(gbl.init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_delay_ms);

    return retOK;
}

retCode check_int_in(int n_args, const int int_value, ...)
{
    va_list ap;
    int counter;

    va_start(ap, int_value);

    for (counter = 2; counter < (n_args+2); counter++) {
        if (int_value == va_arg(ap, int)) {
            va_end(ap);
            return retOK; //found
        }
    }

    va_end(ap);
    return retERR; //not found
}

retCode check_str_in(int n_args, const char *str_value, ...)
{
    va_list ap;
    int counter;

    if (str_value == NULL) {
        return retERR; //not found
    }

    va_start(ap, str_value);

    for (counter = 2; counter < (n_args+2); counter++) {
        if (strcasecmp(str_value, va_arg(ap, char *)) == 0) {
            va_end(ap);
            return retOK; //found
        }
    }

    va_end(ap);
    return retERR; //not found
}

/*
 * init global (unrepeated) modbus links (gbl.mb_links[])
 * (serial or tcp connections)
 */
retCode init_mb_links()
{
    char *fnct_name="init_mb_links";
    int tx_counter, lk_counter;
    int isNewLink;
    mb_link_t *this_mb_link;
    mb_tx_t   *this_mb_tx;

    //group common links transactions
    gbl.tot_mb_links = 0; //total and next available unused link

    for (tx_counter = 0; tx_counter < gbl.tot_mb_tx; tx_counter++) {
        this_mb_tx   = &gbl.mb_tx[tx_counter];

        isNewLink = 1; //Default to true
        if (this_mb_tx->cfg_link_type == linkRTU) { //serial
            for (lk_counter = 0; lk_counter < gbl.tot_mb_links; lk_counter++) {
                if (strcasecmp(this_mb_tx->cfg_serial_device, gbl.mb_links[lk_counter].lp_serial_device) == 0) {
                    isNewLink = 0; //repeated link
                    this_mb_tx->mb_link_num = lk_counter; //each tx know its own link
                    break;
                }
            }
        }
        else { //tcp
            for (lk_counter = 0; lk_counter < gbl.tot_mb_links; lk_counter++) {
                if (strcasecmp(this_mb_tx->cfg_tcp_ip, gbl.mb_links[lk_counter].lp_tcp_ip) == 0) {
                    isNewLink = 0; //repeated link
                    this_mb_tx->mb_link_num = lk_counter; //each tx know its own link
                    break;
                }
            }
        }
        if (isNewLink != 0) { //initialize new link
            this_mb_tx->mb_link_num = gbl.mb_links[gbl.tot_mb_links].mb_link_num  = gbl.tot_mb_links; //next available unused link
            this_mb_link = &gbl.mb_links[gbl.tot_mb_links];

            this_mb_link->lp_link_type = this_mb_tx->cfg_link_type;

            if (this_mb_link->lp_link_type == linkRTU) { //serial
                strncpy(this_mb_link->lp_serial_device, this_mb_tx->cfg_serial_device, MB2HAL_MAX_DEVICE_LENGTH-1);
                this_mb_link->lp_serial_baud=this_mb_tx->cfg_serial_baud;

                if (strcasecmp(this_mb_tx->cfg_serial_parity, "even") == 0) {
                    this_mb_link->lp_serial_parity = 'E';
                }
                else if (strcasecmp(this_mb_tx->cfg_serial_parity, "odd") == 0) {
                    this_mb_link->lp_serial_parity = 'O';
                }
                else { //default = parity none
                    this_mb_link->lp_serial_parity = 'N';
                }

                this_mb_link->lp_serial_data_bit=this_mb_tx->cfg_serial_data_bit;
                this_mb_link->lp_serial_stop_bit=this_mb_tx->cfg_serial_stop_bit;

                this_mb_link->modbus = modbus_new_rtu(this_mb_link->lp_serial_device,
                                                      this_mb_link->lp_serial_baud, this_mb_link->lp_serial_parity,
                                                      this_mb_link->lp_serial_data_bit, this_mb_link->lp_serial_stop_bit);
                if (this_mb_link->modbus == NULL) {
                    ERR(gbl.init_dbg, "modbus_new_rtu failed [%s] [%d] [%c] [%d] [%d]", this_mb_link->lp_serial_device,
                        this_mb_link->lp_serial_baud, this_mb_link->lp_serial_parity, this_mb_link->lp_serial_data_bit,
                        this_mb_link->lp_serial_stop_bit);
                    return retERR;
                }
            }
            else { //tcp
                strncpy(this_mb_link->lp_tcp_ip, this_mb_tx->cfg_tcp_ip, sizeof(this_mb_tx->cfg_tcp_ip)-1);
                this_mb_link->lp_tcp_port=this_mb_tx->cfg_tcp_port;

                this_mb_link->modbus = modbus_new_tcp(this_mb_link->lp_tcp_ip, this_mb_link->lp_tcp_port);
                if (this_mb_link->modbus == NULL) {
                    ERR(gbl.init_dbg, "modbus_new_tcp failed [%s] [%d]", this_mb_link->lp_tcp_ip, this_mb_link->lp_tcp_port);
                    return retERR;
                }
            }

            if (this_mb_link->modbus)

            {
                gbl.tot_mb_links++;    //set new total and next available unused link
            }
        }
    }

    //DEBUG messagess if needed
    for (lk_counter = 0; lk_counter < gbl.tot_mb_links; lk_counter++) {
        this_mb_link = &gbl.mb_links[lk_counter];

        if (this_mb_link->lp_link_type == linkRTU) { //serial
            DBG(gbl.init_dbg, "LINK %d (RTU) link_type[%d] device[%s] baud[%d] data[%d] parity[%c] stop[%d] fd[%d]",
                lk_counter, this_mb_link->lp_link_type, this_mb_link->lp_serial_device,
                this_mb_link->lp_serial_baud, this_mb_link->lp_serial_data_bit, this_mb_link->lp_serial_parity,
                this_mb_link->lp_serial_stop_bit, modbus_get_socket(this_mb_link->modbus));
        }
        else { //tcp
            DBG(gbl.init_dbg, "LINK %d (TCP) link_type[%d] IP[%s] port[%d] fd[%d]",
                lk_counter, this_mb_link->lp_link_type, this_mb_link->lp_tcp_ip,
                this_mb_link->lp_tcp_port, modbus_get_socket(this_mb_link->modbus));
        }
    }

    gbl.mb_links = realloc(gbl.mb_links, sizeof(mb_link_t) * gbl.tot_mb_links);
    if (gbl.mb_links == NULL) {
        DBG(gbl.init_dbg, "realloc gbl.mb_links failed [%s]", strerror(errno));
        return retERR;
    }

    return retOK;
}

/*
 * init more parameters of global modbus transactions (gbl.mb_tx)
 */
retCode init_mb_tx()
{
    char *fnct_name="init_mb_tx";
    int tx_counter;
    mb_tx_t   *this_mb_tx;

    for (tx_counter = 0; tx_counter < gbl.tot_mb_tx; tx_counter++) {
        this_mb_tx = &gbl.mb_tx[tx_counter];

        this_mb_tx->mb_tx_num = tx_counter;
        this_mb_tx->protocol_debug = (this_mb_tx->cfg_debug >= debugDEBUG)? 1 : 0;

        if (this_mb_tx->cfg_update_rate > 0) {
            this_mb_tx->time_increment = 1.0 / this_mb_tx->cfg_update_rate; //wait time between tx
        }
        this_mb_tx->next_time = 0; //next time for this tx

        DBG(gbl.init_dbg, "MB_TX %d lk_n[%d] tx_n[%d] cfg_dbg[%d] lk_dbg[%d] t_inc[%0.3f] nxt_t[%0.3f]",
            tx_counter, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_num, this_mb_tx->cfg_debug,
            this_mb_tx->protocol_debug, this_mb_tx->time_increment, this_mb_tx->next_time);
    }

    return retOK;
}
