
/** This file, 'hal_cmds.h', is a HAL component that provides processing
    of command lines to the hal.  It is a user space component.
    It is the basis of halcmd, and details on the command structure
    can be found in 'man halcmd'
*/

/** Copyright (C) 2003 John Kasunich
                       <jmkasunich AT users DOT sourceforge DOT net>

    Other contributers:
                       Martin Kuhnle
                       <mkuhnle AT users DOT sourceforge DOT net>
                       Alex Joni
                       <alex_joni AT users DOT sourceforge DOT net>
                       Jonathan Stark
                       <zwisk AT users DOT sourceforge DOT net>

*/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2.1 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the EMC HAL project.  For more
    information, go to www.linuxcnc.org.
*/

#ifndef ULAPI
#error This is a user mode component only!
#endif

#ifndef _HAL_CMDS_H_
#define _HAL_CMDS_H_

#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* private HAL decls */

/***********************************************************************
*                        FUNCTION DECLARATIONS                         *
************************************************************************/

#define MAX_TOK 20
#define MAX_CMD_LEN 1024

extern int parse_cmd(char *tokens[]);
extern int do_link_cmd(char *pin, char *sig);
extern int do_newsig_cmd(char *name, char *type);
extern int do_setp_cmd(char *name, char *value);
extern int do_sets_cmd(char *name, char *value);
extern int do_show_cmd(char *type);
extern int do_loadrt_cmd(char *mod_name, char *args[]);
extern int do_delsig_cmd(char *mod_name);
extern int do_unloadrt_cmd(char *mod_name);
extern int unloadrt_comp(char *mod_name);
extern void print_comp_list(void);
extern void print_pin_list(void);
extern void print_sig_list(void);
extern void print_param_list(void);
extern void print_funct_list(void);
extern void print_thread_list(void);
extern char *data_type(int type);
extern char *data_dir(int dir);
extern char *data_arrow1(int dir);
extern char *data_arrow2(int dir);
extern char *data_value(int type, void *valptr);
extern int do_save_cmd(char *type);
extern void save_signals(void);
extern void save_links(int arrows);
extern void save_nets(int arrows);
extern void save_params(void);
extern void save_threads(void);

extern int run_command(const char *command);
extern int run_script(const char *filename);
extern int run_script_file(FILE *infile);

/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

extern int comp_id;


#endif // _HAL_CMDS_H_
