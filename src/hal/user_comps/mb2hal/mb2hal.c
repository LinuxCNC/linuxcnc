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

modbus_param_t *gbl_mb_links = NULL;
int gbl_n_mb_links = 0;
int *tmp_mb_links_num = NULL; //for thread parameter use only
mb_tx_t *gbl_mb_tx = NULL;
int gbl_n_mb_tx = 0;

char *hal_mod_name = "mb2hal";
int hal_mod_id = -1;
int init_debug = debugMAX; // until readed in config file
double slowdown = 0; // until readed in config file

pthread_t thrd[256];
int quit_flag; //tell the threads to quit (SIGTERM o SIGQUIT) (unloadusr mb2hal).

/*
 * Main: parse args, open ini file, parse ini file (transaction strcutures),
 * init links (links structures), init and create hal pins,
 * create a thread for each link , and wait forever
 */

int main(int argc, char **argv)
{
    char *fnct_name = "main";
    FILE *ini_file_ptr = NULL;
    char *ini_file_path = NULL;
    pthread_attr_t thrd_attr;
    int counter;
    int ret;

    init_debug = debugERR; //until indicated in config file

    if (parse_main_args(&ini_file_path, argc, argv) != 0) {
        ERR(init_debug, "Unable to parse arguments");
        return -1;
    }

    ini_file_ptr = fopen(ini_file_path, "r");
    if (ini_file_ptr == NULL) {
        ERR(init_debug, "Unable to open ini file [%s]", ini_file_path);
        return -1;
    }

    if (parse_ini_file(ini_file_ptr) != 0) {
        ERR(init_debug, "Unable to parse INI file [%s]", ini_file_path);
        goto QUIT_CLEANUP;
    }
    OK(init_debug, "parse_ini_file done OK");

    if (init_gbl_mb_links() != retOK) {
        ERR(init_debug, "Unable to initialize gbl_mb_links");
        goto QUIT_CLEANUP;
    }
    OK(init_debug, "init_gbl_mb_links done OK");

    if (init_gbl_mb_tx() != retOK) {
        ERR(init_debug, "Unable to initialize gbl_mb_tx");
        goto QUIT_CLEANUP;
    }
    OK(init_debug, "init_gbl_mb_tx done OK");

    hal_mod_id = hal_init(hal_mod_name);
    if (hal_mod_id < 0) {
        ERR(init_debug, "Unable to initialize HAL component [%s]", hal_mod_name);
        goto QUIT_CLEANUP;
    }
    if (create_HAL_pins() != retOK) {
        ERR(init_debug, "Unable to create HAL pins");
        goto QUIT_CLEANUP;
    }
    hal_ready(hal_mod_id);
    OK(init_debug, "HAL components created OK");

    quit_flag = 0; //tell the threads to quit (SIGTERM o SIGQUIT) (unloadusr mb2hal).
    signal(SIGINT, quit_signal);
    //unloadusr and unload commands of halrun
    signal(SIGTERM, quit_signal);

    /* Each link has it's own thread */
    pthread_attr_init(&thrd_attr);
    pthread_attr_setdetachstate(&thrd_attr, PTHREAD_CREATE_DETACHED);
    for (counter = 0; counter < gbl_n_mb_links; counter++) {
        ret = pthread_create(&thrd[counter], &thrd_attr, link_loop_and_logic, (void *) &tmp_mb_links_num[counter]);
        if (ret != 0) {
            ERR(init_debug, "Unable to start thread for link number %d", counter);
        }
        OK(init_debug, "Link thread loop and logic %d created OK", counter);
    }

    while (quit_flag == 0) {
        sleep(1);
    }

QUIT_CLEANUP:
    quit_cleanup();
    OK(init_debug, "going to exit!");
    return 0;
}

/*
 * One thread loop for each link
 * The LOGIC is here
 * link_params is (modbus_param_t *)
 */

void *link_loop_and_logic(void *tmp_mb_links_num)
{
    char *fnct_name = "link_loop";
    int mb_tx_num;
    int ret, ret_available, ret_connected;
    mb_tx_t *tmp_mb_tx_ptr = NULL;
    modbus_param_t *link_params = NULL;
    int mb_links_num;

    if (tmp_mb_links_num == NULL) {
        ERR(init_debug, "NULL pointer");
        return NULL;
    }
    mb_links_num = *((int *)tmp_mb_links_num);
    if (mb_links_num < 0 || mb_links_num >= gbl_n_mb_links) {
        ERR(init_debug, "parameter out of range tmp_mb_links_num [%d]", mb_links_num);
        return NULL;
    }
    link_params = &gbl_mb_links[mb_links_num];

    while (1) {

        for (mb_tx_num = 0; mb_tx_num < gbl_n_mb_tx; mb_tx_num++) {

            if (quit_flag != 0) { //tell the threads to quit (SIGTERM o SGIQUIT) (unloadusr mb2hal).
                return NULL;
            }

            tmp_mb_tx_ptr = &gbl_mb_tx[mb_tx_num];

            DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] going to TEST availability",
                mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd);

            //corresponding link and time (update_rate)
            if (is_this_tx_available(mb_links_num, mb_tx_num, &ret_available) != retOK) {
                ERR(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] is_this_tx_available ERR",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd);
                return NULL;
            }
            if (ret_available == 0) {
                DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] NOT available",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd);
                continue;
            }

            DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] going to TEST connection",
                mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd);

            //first time connection or reconnection
            if (get_tx_connection(mb_tx_num, &ret_connected) != retOK) {
                ERR(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] get_tx_connection ERR",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd);
                return NULL;
            }
            if (ret_connected == 0) {
                DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] NOT connected",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd);
                continue;
            }

            //set the low level mb_link debug according to each mb_tx
            link_params->print_errors = tmp_mb_tx_ptr->cfg_lk_param.print_errors;
            link_params->debug = tmp_mb_tx_ptr->cfg_lk_param.debug;

            DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] prt_err[%d] lk_dbg[%d] going to EXECUTE transaction",
                mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd,
                link_params->print_errors, link_params->debug);

            switch (tmp_mb_tx_ptr->mb_tx_code) {
            case mbtx_02_READ_DISCRETE_INPUTS:
                ret = fnct_02_read_discrete_inputs(tmp_mb_tx_ptr);
                break;
            case mbtx_03_READ_HOLDING_REGISTERS:
                ret = fnct_03_read_holding_registers(tmp_mb_tx_ptr);
                break;
            case mbtx_04_READ_INPUT_REGISTERS:
                ret = fnct_04_read_input_registers(tmp_mb_tx_ptr);
                break;
            case mbtx_15_WRITE_MULTIPLE_COILS:
                ret = fnct_15_write_multiple_coils(tmp_mb_tx_ptr);
                break;
            case mbtx_16_WRITE_MULTIPLE_REGISTERS:
                ret = fnct_16_write_multiple_registers(tmp_mb_tx_ptr);
                break;
            default:
                ret = -1;
                ERR(tmp_mb_tx_ptr->cfg_debug, "case error with mb_tx_code %d [%s] in mb_tx_num[%d]",
                    tmp_mb_tx_ptr->mb_tx_code, tmp_mb_tx_ptr->cfg_mb_tx_code_name, mb_tx_num);
                break;
            }

            if (quit_flag != 0) { //tell the threads to quit (SIGTERM o SGIQUIT) (unloadusr mb2hal).
                return NULL;
            }

            if (ret != retOK && link_params->fd < 0) { //link failure
                tmp_mb_tx_ptr->num_errors++;
                ERR(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] link failure, going to close link",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd);
                modbus_close(link_params);
                link_params->fd = -1;
            }
            else if (ret != retOK) {  //transaction failure
                tmp_mb_tx_ptr->num_errors++;
                ERR(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] transaction failure, num_errors[%d]",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd, tmp_mb_tx_ptr->num_errors);
            }
            else { //OK
                OK(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] transaction OK, update_HZ[%0.03f]",
                   mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd, 1.0/(get_time()-tmp_mb_tx_ptr->last_time_ok));
                tmp_mb_tx_ptr->last_time_ok = get_time();
                tmp_mb_tx_ptr->num_errors=0;
            }

            //set the next (waiting) time for update rate
            tmp_mb_tx_ptr->next_time = get_time() + tmp_mb_tx_ptr->time_increment;

            //wait time for serial lines
            if (tmp_mb_tx_ptr->cfg_lk_param.type_com == RTU) {
                DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] SERIAL_DELAY_MS activated [%d]",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd, tmp_mb_tx_ptr->lp_serial_delay_ms);
                usleep(tmp_mb_tx_ptr->lp_serial_delay_ms * 1000);
            }

            //wait time to slowdown activity (debugging)
            if (slowdown > 0) {
                DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] slowdown activated [%0.3f]",
                    mb_tx_num, tmp_mb_tx_ptr->mb_links_num, mb_links_num, link_params->fd, slowdown);
                usleep(slowdown * 1000 * 1000);
            }
        }  //end for

    } //end while

    return NULL;
}

/*
 * Check if the transaction is available for this link
 */

retCode is_this_tx_available(const int mb_links_num, const int mb_tx_num, int *ret_available)
{
    char *fnct_name = "is_this_tx_available";
    int tmp_mb_links_num;
    mb_tx_t *tmp_mb_tx_ptr;

    if (mb_tx_num < 0 || mb_tx_num > gbl_n_mb_tx) {
        ERR(init_debug, "parameter out of range mb_tx_num [%d]", mb_tx_num);
        return retERR;
    }
    tmp_mb_tx_ptr = &gbl_mb_tx[mb_tx_num];

    if (ret_available == NULL) {
        ERR(tmp_mb_tx_ptr->cfg_debug, "NULL pointer");
        return retERR;
    }

    tmp_mb_links_num = gbl_mb_tx[mb_tx_num].mb_links_num;
    if (tmp_mb_links_num < 0 || tmp_mb_links_num >= gbl_n_mb_links) {
        ERR(tmp_mb_tx_ptr->cfg_debug, "parameter out of range mb_link_num [%d]", tmp_mb_links_num);
        return retERR;
    }

    *ret_available = 0; //defaults to not available

    //the tx is not of this link
    if (mb_links_num != tmp_mb_links_num) {
        return retOK;
    }

    //not now
    if (get_time() < tmp_mb_tx_ptr->next_time) {
        return retOK;
    }

    *ret_available = 1; //is available
    return retOK;
}

/*
 * First time connection or reconnection
 */

retCode get_tx_connection(const int mb_tx_num, int *ret_connected)
{
    char *fnct_name = "get_tx_connection";
    int ret;
    int tmp_mb_links_num;
    mb_tx_t *tmp_mb_tx_ptr;

    if (mb_tx_num < 0 || mb_tx_num > gbl_n_mb_tx) {
        ERR(init_debug, "parameter out of range mb_tx_num [%d]", mb_tx_num);
        return retERR;
    }
    tmp_mb_tx_ptr = &gbl_mb_tx[mb_tx_num];

    if (ret_connected == NULL) {
        ERR(tmp_mb_tx_ptr->cfg_debug, "NULL pointer");
        return retERR;
    }

    tmp_mb_links_num = gbl_mb_tx[mb_tx_num].mb_links_num;
    if (tmp_mb_links_num < 0 || tmp_mb_links_num >= gbl_n_mb_links) {
        ERR(tmp_mb_tx_ptr->cfg_debug, "parameter out of range mb_link_num [%d]", tmp_mb_links_num);
        return retERR;
    }

    *ret_connected = 0; //defaults to not connected

    //equals to link_params in thread
    if (gbl_mb_links[tmp_mb_tx_ptr->mb_links_num].fd < 0) {
        ret = modbus_connect(&gbl_mb_links[tmp_mb_tx_ptr->mb_links_num]);
        if (ret != 0 || gbl_mb_links[tmp_mb_tx_ptr->mb_links_num].fd < 0) {
            gbl_mb_links[tmp_mb_tx_ptr->mb_links_num].fd = -1; //some times ret was < 0 and
            //                                                 //gbl_mb_links[tmp_mb_tx_ptr->mb_links_num].fd > 0
            ERR(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] cannot connect to link, ret[%d] fd[%d]",
                mb_tx_num, tmp_mb_tx_ptr->mb_links_num, ret, gbl_mb_links[tmp_mb_tx_ptr->mb_links_num].fd);
            return retOK; //not connected
        }
        DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] new connection -> fd[%d]",
            mb_tx_num, tmp_mb_tx_ptr->mb_links_num, gbl_mb_links[tmp_mb_tx_ptr->mb_links_num].fd);
    }
    else {
        DBG(tmp_mb_tx_ptr->cfg_debug, "mb_tx_num[%d] mb_links[%d] already connected to fd[%d]",
            mb_tx_num, tmp_mb_tx_ptr->mb_links_num, gbl_mb_links[tmp_mb_tx_ptr->mb_links_num].fd);

    }

    *ret_connected = 1; //is connected (fd >= 0)
    return retOK;
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

    quit_flag = 1; //tell the threads to quit (SIGTERM o SIGQUIT) (unloadusr mb2hal).
    DBG(init_debug, "signal [%d] received", signal);
}

void quit_cleanup(void)
{
    char *fnct_name = "quit_cleanup";
    int counter, ret;

    DBG(init_debug, "started");

    for (counter = 0; counter < gbl_n_mb_links; counter++) {
        if (&gbl_mb_links[counter] != NULL) {
            modbus_close(&gbl_mb_links[counter]);
        }
    }
    if (gbl_mb_links != NULL) {
        free(gbl_mb_links);
    }
    gbl_n_mb_links = 0;
    gbl_mb_links = NULL;

    if (gbl_mb_tx != NULL) {
        free(gbl_mb_tx);
    }
    gbl_n_mb_tx = 0;
    gbl_mb_tx = NULL;

    if (hal_mod_id >= 0) {
        ret = hal_exit(hal_mod_id);
        DBG(init_debug, "unloading HAL module [%d] ret[%d]", hal_mod_id, ret);
    }

    DBG(init_debug, "done OK");
}
