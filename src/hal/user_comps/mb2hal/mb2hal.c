/*
 * mb2hal.c
 * Userspace HAL component to communicate with one or more Modbus devices.
 *
 * Victor Rocco, adapted from Les Newell's modbuscomms.c which is
 * Copyright (C) 2009-2012 Les Newell <les@sheetcam.com>
 * source code in http://wiki.linuxcnc.org/cgi-bin/wiki.pl?ContributedComponents
 *
 * Copyright (C) 2012 Victor Rocco <victor_rocco AT hotmail DOT com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#include "mb2hal.h"

gbl_t gbl;

/*
 * Main: init global params, parse args, open ini file, parse ini file
 * (transaction strcutures), init links (links structures), init and
 * create hal pins, create a thread for each link , and wait forever
 */

int main(int argc, char **argv)
{
    char *fnct_name = "main";
    pthread_attr_t thrd_attr;
    int counter;
    int ret;

    set_init_gbl_params();

    if (parse_main_args(argc, argv) != 0) {
        ERR(gbl.init_dbg, "Unable to parse arguments");
        return -1;
    }

    gbl.ini_file_ptr = fopen(gbl.ini_file_path, "r");
    if (gbl.ini_file_ptr == NULL) {
        ERR(gbl.init_dbg, "Unable to open INI file [%s]", gbl.ini_file_path);
        return -1;
    }

    if (parse_ini_file() != 0) {
        ERR(gbl.init_dbg, "Unable to parse INI file [%s]", gbl.ini_file_path);
        goto QUIT_CLEANUP;
    }
    OK(gbl.init_dbg, "parse_ini_file done OK");

    if (init_mb_links() != retOK) {
        ERR(gbl.init_dbg, "init_mb_links failed");
        goto QUIT_CLEANUP;
    }
    OK(gbl.init_dbg, "init_gbl.mb_link done OK");

    if (init_mb_tx() != retOK) {
        ERR(gbl.init_dbg, "init_mb_tx failed");
        goto QUIT_CLEANUP;
    }
    OK(gbl.init_dbg, "init_gbl.mb_tx done OK");

    gbl.hal_mod_id = hal_init(gbl.hal_mod_name);
    if (gbl.hal_mod_id < 0) {
        ERR(gbl.init_dbg, "Unable to initialize HAL component [%s]", gbl.hal_mod_name);
        goto QUIT_CLEANUP;
    }
    if (create_HAL_pins() != retOK) {
        ERR(gbl.init_dbg, "Unable to create HAL pins");
        goto QUIT_CLEANUP;
    }
    hal_ready(gbl.hal_mod_id);
    OK(gbl.init_dbg, "HAL components created OK");

    gbl.quit_flag = 0; //tell the threads to quit (SIGTERM o SIGQUIT) (unloadusr mb2hal).
    signal(SIGINT, quit_signal);
    //unloadusr and unload commands of halrun
    signal(SIGTERM, quit_signal);

    /* Each link has it's own thread */
    pthread_attr_init(&thrd_attr);
    pthread_attr_setdetachstate(&thrd_attr, PTHREAD_CREATE_DETACHED);
    for (counter = 0; counter < gbl.tot_mb_links; counter++) {
        ret = pthread_create(&gbl.mb_links[counter].thrd, &thrd_attr, link_loop_and_logic, (void *) &gbl.mb_links[counter].mb_link_num);
        if (ret != 0) {
            ERR(gbl.init_dbg, "Unable to start thread for link number %d", counter);
        }
        OK(gbl.init_dbg, "Link thread loop and logic %d created OK", counter);
    }

    OK(gbl.init_dbg, "%s is running", gbl.hal_mod_name);
    while (gbl.quit_flag == 0) {
        sleep(1);
    }

QUIT_CLEANUP:
    quit_cleanup();
    OK(gbl.init_dbg, "going to exit!");
    return 0;
}

/*
 * One thread loop for each link
 * The LOGIC is here
 * thrd_link_num is the corresponding link of this thread (int *)
 */

void *link_loop_and_logic(void *thrd_link_num)
{
    char *fnct_name = "link_loop_and_logic";
    int ret, ret_available, ret_connected;
    int tx_counter;
    mb_tx_t   *this_mb_tx = NULL;
    int        this_mb_tx_num;
    mb_link_t *this_mb_link = NULL;
    int        this_mb_link_num;

    if (thrd_link_num == NULL) {
        ERR(gbl.init_dbg, "NULL pointer");
        return NULL;
    }
    this_mb_link_num = *((int *)thrd_link_num);
    if (this_mb_link_num < 0 || this_mb_link_num >= gbl.tot_mb_links) {
        ERR(gbl.init_dbg, "parameter out of range this_mb_link_num[%d]", this_mb_link_num);
        return NULL;
    }
    this_mb_link = &gbl.mb_links[this_mb_link_num];

    while (1) {

        for (tx_counter = 0; tx_counter < gbl.tot_mb_tx; tx_counter++) {

            if (gbl.quit_flag != 0) { //tell the threads to quit (SIGTERM o SGIQUIT) (unloadusr mb2hal).
                return NULL;
            }

            this_mb_tx_num = tx_counter;
            this_mb_tx = &gbl.mb_tx[this_mb_tx_num];

            DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] going to TEST availability",
                this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));

            //corresponding link and time (update_rate)
            if (is_this_tx_ready(this_mb_link_num, this_mb_tx_num, &ret_available) != retOK) {
                ERR(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] is_this_tx_ready ERR",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                return NULL;
            }
            if (ret_available == 0) {
                DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] NOT available",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                continue;
            }

            DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] going to TEST connection",
                this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));

            //first time connection or reconnection, run time parameters setting
            if (get_tx_connection(this_mb_tx_num, &ret_connected) != retOK) {
                ERR(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] get_tx_connection ERR",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                return NULL;
            }
            if (ret_connected == 0) {
                DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] NOT connected",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                continue;
            }

            DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] lk_dbg[%d] going to EXECUTE transaction",
                this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus),
                this_mb_tx->protocol_debug);

            switch (this_mb_tx->mb_tx_fnct) {
            case mbtx_02_READ_DISCRETE_INPUTS:
                ret = fnct_02_read_discrete_inputs(this_mb_tx, this_mb_link);
                break;
            case mbtx_03_READ_HOLDING_REGISTERS:
                ret = fnct_03_read_holding_registers(this_mb_tx, this_mb_link);
                break;
            case mbtx_04_READ_INPUT_REGISTERS:
                ret = fnct_04_read_input_registers(this_mb_tx, this_mb_link);
                break;
            case mbtx_15_WRITE_MULTIPLE_COILS:
                ret = fnct_15_write_multiple_coils(this_mb_tx, this_mb_link);
                break;
            case mbtx_16_WRITE_MULTIPLE_REGISTERS:
                ret = fnct_16_write_multiple_registers(this_mb_tx, this_mb_link);
                break;
            default:
                ret = -1;
                ERR(this_mb_tx->cfg_debug, "case error with mb_tx_fnct %d [%s] in mb_tx_num[%d]",
                    this_mb_tx->mb_tx_fnct, this_mb_tx->mb_tx_fnct_name, this_mb_tx_num);
                break;
            }

            if (gbl.quit_flag != 0) { //tell the threads to quit (SIGTERM o SGIQUIT) (unloadusr mb2hal).
                return NULL;
            }

            if (ret != retOK && modbus_get_socket(this_mb_link->modbus) < 0) { //link failure
                (*this_mb_tx->num_errors)++;
                ERR(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] link failure, going to close link",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                modbus_close(this_mb_link->modbus);
            }
            else if (ret != retOK) {  //transaction failure but link OK
                (**this_mb_tx->num_errors)++;
                ERR(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] transaction failure, num_errors[%d]",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus), **this_mb_tx->num_errors);
            }
            else { //transaction and link OK
                OK(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] transaction OK, update_HZ[%0.03f]",
                   this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus),
                   1.0/(get_time()-this_mb_tx->last_time_ok));
                this_mb_tx->last_time_ok = get_time();
                (**this_mb_tx->num_errors) = 0;
            }

            //set the next (waiting) time for update rate
            this_mb_tx->next_time = get_time() + this_mb_tx->time_increment;

            //wait time for serial lines
            if (this_mb_tx->cfg_link_type == linkRTU) {
                DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] SERIAL_DELAY_MS activated [%d]",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus),
                    this_mb_tx->cfg_serial_delay_ms);
                usleep(this_mb_tx->cfg_serial_delay_ms * 1000);
            }

            //wait time to gbl.slowdown activity (debugging)
            if (gbl.slowdown > 0) {
                DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] gbl.slowdown activated [%0.3f]",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus), gbl.slowdown);
                usleep(gbl.slowdown * 1000 * 1000);
            }
        }  //end for

    } //end while

    return NULL;
}

/*
 * Check if the transaction is available for this link
 */

retCode is_this_tx_ready(const int this_mb_link_num, const int this_mb_tx_num, int *ret_available)
{
    char *fnct_name = "is_this_tx_available";
    mb_tx_t *this_mb_tx;
    int this_mb_tx_link_num;

    if (this_mb_tx_num < 0 || this_mb_tx_num > gbl.tot_mb_tx) {
        ERR(gbl.init_dbg, "parameter out of range this_mb_tx_num[%d]", this_mb_tx_num);
        return retERR;
    }
    this_mb_tx = &gbl.mb_tx[this_mb_tx_num];

    if (ret_available == NULL) {
        ERR(this_mb_tx->cfg_debug, "NULL pointer");
        return retERR;
    }

    this_mb_tx_link_num = this_mb_tx->mb_link_num;
    if (this_mb_tx_link_num < 0 || this_mb_tx_link_num >= gbl.tot_mb_links) {
        ERR(this_mb_tx->cfg_debug, "parameter out of range this_mb_tx_link_num[%d]", this_mb_tx_link_num);
        return retERR;
    }

    *ret_available = 0; //defaults to not available

    //the tx is not of this link
    if (this_mb_link_num != this_mb_tx_link_num) {
        return retOK;
    }

    //not now
    if (get_time() < this_mb_tx->next_time) {
        return retOK;
    }

    *ret_available = 1; //is available
    return retOK;
}

/*
 * First time connection or reconnection
 */

retCode get_tx_connection(const int this_mb_tx_num, int *ret_connected)
{
    char *fnct_name = "get_tx_connection";
    int ret;
    mb_tx_t   *this_mb_tx;
    mb_link_t *this_mb_link;
    int        this_mb_link_num;
    struct timeval timeout;

    if (this_mb_tx_num < 0 || this_mb_tx_num > gbl.tot_mb_tx) {
        ERR(gbl.init_dbg, "parameter out of range this_mb_tx_num[%d]", this_mb_tx_num);
        return retERR;
    }
    this_mb_tx = &gbl.mb_tx[this_mb_tx_num];

    if (ret_connected == NULL) {
        ERR(this_mb_tx->cfg_debug, "NULL pointer");
        return retERR;
    }

    this_mb_link_num = this_mb_tx->mb_link_num;
    if (this_mb_link_num < 0 || this_mb_link_num >= gbl.tot_mb_links) {
        ERR(this_mb_tx->cfg_debug, "parameter out of range this_mb_link_num[%d]", this_mb_link_num);
        return retERR;
    }
    this_mb_link = &gbl.mb_links[this_mb_link_num];

    *ret_connected = 0; //defaults to not connected

    if (modbus_get_socket(this_mb_link->modbus) < 0) {
        ret = modbus_connect(this_mb_link->modbus);
        if (ret != 0 || modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_set_socket(this_mb_link->modbus, -1); //some times ret was < 0 and fd > 0
            ERR(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] cannot connect to link, ret[%d] fd[%d]",
                this_mb_tx_num, this_mb_tx->mb_link_num, ret, modbus_get_socket(this_mb_link->modbus));
            return retOK; //not connected
        }
        DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] new connection -> fd[%d]",
            this_mb_tx_num, this_mb_tx->mb_link_num, modbus_get_socket(this_mb_link->modbus));
    }
    else {
        DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] already connected to fd[%d]",
            this_mb_tx_num, this_mb_tx->mb_link_num, modbus_get_socket(this_mb_link->modbus));
    }

    //set slave id according to each mb_tx
    ret = modbus_set_slave(this_mb_link->modbus, this_mb_tx->mb_tx_slave_id);
    if (ret != 0) {
        ERR(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] cannot set slave [%d]",
            this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id);
        return retOK; //not connected
    }

    //set the low level mb_link debug according to each mb_tx
    modbus_set_debug(this_mb_link->modbus, this_mb_tx->protocol_debug);

    //set response and byte timeout according to each mb_tx
    timeout.tv_sec  = this_mb_tx->mb_response_timeout_ms / 1000;
    timeout.tv_usec = (this_mb_tx->mb_response_timeout_ms % 1000) * 1000;
    modbus_set_response_timeout(this_mb_link->modbus, &timeout);
    //DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] response timeout [%d] ([%d] [%d])",
    //    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_response_timeout_ms,
    //    (int) timeout.tv_sec, (int) timeout.tv_usec);

    timeout.tv_sec  = this_mb_tx->mb_byte_timeout_ms / 1000;
    timeout.tv_usec = (this_mb_tx->mb_byte_timeout_ms % 1000) * 1000;
    modbus_set_byte_timeout(this_mb_link->modbus, &timeout);
    //DBG(this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] byte timeout [%d] ([%d] [%d])",
    //    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_byte_timeout_ms,
    //    (int) timeout.tv_sec, (int) timeout.tv_usec);

    *ret_connected = 1; //is connected (fd >= 0)
    return retOK;
}

void set_init_gbl_params()
{
    gbl.hal_mod_name = "mb2hal"; //until readed in config file
    gbl.hal_mod_id   = -1;
    gbl.init_dbg     = debugERR; //until readed in config file
    gbl.slowdown     = 0;        //until readed in config file
    gbl.mb_tx_fncts[mbtxERR]                         = "";
    gbl.mb_tx_fncts[mbtx_02_READ_DISCRETE_INPUTS]    = "fnct_02_read_discrete_inputs";
    gbl.mb_tx_fncts[mbtx_03_READ_HOLDING_REGISTERS]  = "fnct_03_read_holding_registers";
    gbl.mb_tx_fncts[mbtx_04_READ_INPUT_REGISTERS]    = "fnct_04_read_input_registers";
    gbl.mb_tx_fncts[mbtx_15_WRITE_MULTIPLE_COILS]    = "fnct_15_write_multiple_coils";
    gbl.mb_tx_fncts[mbtx_16_WRITE_MULTIPLE_REGISTERS]= "fnct_16_write_multiple_registers";

    return;
}

double get_time()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_sec + ((double) time.tv_usec / 1000000.0f));
}

/*
 * Called to unload HAL module
 * unloadusr and unload commands
 */

void quit_signal(int signal)
{
    char *fnct_name = "quit_signal";

    gbl.quit_flag = 1; //tell the threads to quit (SIGTERM o SIGQUIT) (unloadusr mb2hal).
    DBG(gbl.init_dbg, "signal [%d] received", signal);
}

void quit_cleanup(void)
{
    char *fnct_name = "quit_cleanup";
    int counter, ret;

    DBG(gbl.init_dbg, "started");

    for (counter = 0; counter < gbl.tot_mb_links; counter++) {
        if (gbl.mb_links[counter].modbus != NULL) {
            modbus_close(gbl.mb_links[counter].modbus);
            modbus_free(gbl.mb_links[counter].modbus);
            gbl.mb_links[counter].modbus = NULL;
        }
    }
    gbl.tot_mb_links = 0;

    if (gbl.mb_tx != NULL) {
        free(gbl.mb_tx);
    }
    gbl.mb_tx = NULL;
    gbl.tot_mb_tx = 0;

    if (gbl.hal_mod_id >= 0) {
        ret = hal_exit(gbl.hal_mod_id);
        DBG(gbl.init_dbg, "unloading HAL module [%d] ret[%d]", gbl.hal_mod_id, ret);
    }

    DBG(gbl.init_dbg, "done OK");
}
