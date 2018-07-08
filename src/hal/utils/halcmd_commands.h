/* Copyright (C) 2007 Jeff Epler <jepler@unpythonic.net>
 * Copyright (C) 2003 John Kasunich
 *                     <jmkasunich AT users DOT sourceforge DOT net>
 *
 *  Other contributers:
 *                     Martin Kuhnle
 *                     <mkuhnle AT users DOT sourceforge DOT net>
 *                     Alex Joni
 *                     <alex_joni AT users DOT sourceforge DOT net>
 *                     Benn Lipkowitz
 *                     <fenn AT users DOT sourceforge DOT net>
 *                     Stephen Wille Padnos
 *                     <swpadnos AT users DOT sourceforge DOT net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General
 *  Public License as published by the Free Software Foundation.
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 *  ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 *  TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 *  harming persons must have provisions for completely removing power
 *  from all motors, etc, before persons enter any danger area.  All
 *  machinery must be designed to comply with local and national safety
 *  codes, and the authors of this software can not, and do not, take
 *  any responsibility for such compliance.
 *
 *  This code was written as part of the EMC HAL project.  For more
 *  information, go to www.linuxcnc.org.
 */

#ifndef HALCMD_COMMANDS_H
#define HALCMD_COMMANDS_H
#include "halcmd.h"
#include <sys/types.h>
#include <sys/wait.h>

RTAPI_BEGIN_DECLS

extern int do_addf_cmd(char *funct, char *thread, char *tokens[]);
extern int do_alias_cmd(char *pinparam, char *name, char *alias);
extern int do_unalias_cmd(char *pinparam, char *name);
extern int do_delf_cmd(char *funct, char *thread);
extern int do_echo_cmd();
extern int do_unecho_cmd();
extern int do_linkps_cmd(char *pin, char *signal);
extern int do_linksp_cmd(char *signal, char *pin);
extern int do_start_cmd();
extern int do_stop_cmd();
extern int do_help_cmd(char *command);
extern int do_lock_cmd(char *command);
extern int do_unlock_cmd(char *command);
extern int do_linkpp_cmd(char *first_pin_name, char *second_pin_name);
extern int do_newsig_cmd(char *name, char *type);
#if 0  /* newinst deferred to version 2.2 */
extern int do_newinst_cmd(char *comp_name, char *inst_name);
#endif
extern int do_net_cmd(char *signame, char *pins[]);
extern int do_setp_cmd(char *name, char *value);
extern int do_getp_cmd(char *name);
extern int do_sets_cmd(char *name, char *value);
extern int do_gets_cmd(char *name);
extern int do_ptype_cmd(char *name);
extern int do_stype_cmd(char *name);
extern int do_show_cmd(char *type, char **patterns);
extern int do_list_cmd(char *type, char **patterns);
extern int do_source_cmd(char *type);
extern int do_status_cmd(char *type);
extern int do_delsig_cmd(char *mod_name);
extern int do_loadrt_cmd(char *mod_name, char *args[]);
extern int do_unlinkp_cmd(char *mod_name);
extern int do_unload_cmd(char *mod_name);
extern int do_unloadrt_cmd(char *mod_name);
extern int do_unloadusr_cmd(char *mod_name);
extern int do_loadusr_cmd(char *args[]);
extern int do_waitusr_cmd(char *comp_name);
extern int do_save_cmd(char *type, char *filename);
extern int do_setexact_cmd(void);

pid_t hal_systemv_nowait(char *const argv[]);
int hal_systemv(char *const argv[]);

extern int scriptmode, comp_id;

RTAPI_END_DECLS

#endif
