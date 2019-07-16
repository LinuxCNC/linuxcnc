/* Classic Ladder Project */
/* Copyright (C) 2001-2003 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* May 2003 */
/* Part written by Thomas Gleixner */
/* --------------------------------- */
/* Config file sizes to alloc parser */
/* --------------------------------- */
/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, write to the Free Software */
/* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include <locale.h>
#include <libintl.h>
#define _(x) gettext(x)
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "classicladder.h"
#include "global.h"

#define TYPE_INT	1
#define TYPE_STRING	2

struct cfg_cfg {
	char	*name;
	int	type;
	void	*data;
};


/* Main configuration */
static struct cfg_cfg maincfg[] = {
	{ "NBR_RUNGS", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_rungs },
	{ "NBR_BITS", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_bits },
	{ "NBR_WORDS", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_words },
#ifdef OLD_TIMERS_MONOS_SUPPORT
	{ "NBR_TIMERS", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_timers },
	{ "NBR_MONOSTABLES", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_monostables },
#endif
	{ "NBR_PHYS_INPUTS", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_phys_inputs },
	{ "NBR_PHYS_OUTPUTS", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_phys_outputs },
	{ "NBR_ARITHM_EXPR", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_arithm_expr },
	{ "NBR_SECTIONS", TYPE_INT, (void *) &GeneralParamsMirror.SizesInfos.nbr_sections },
#ifdef MODBUS_IO_MASTER
	{ "MODBUS_MASTER_SERIAL_PORT", TYPE_STRING, (void *) ModbusSerialPortNameUsed },
	{ "MODBUS_MASTER_SERIAL_SPEED", TYPE_INT, (void *) &ModbusSerialSpeed },
	{ "MODBUS_MASTER_SERIAL_USE_RTS_TO_SEND", TYPE_INT, (void *) &ModbusSerialUseRtsToSend },
        { "MODBUS_MASTER_ELEMENT_OFFSET", TYPE_INT, (void *) &ModbusEleOffset },
	{ "MODBUS_MASTER_TIME_INTER_FRAME", TYPE_INT, (void *) &ModbusTimeInterFrame },
	{ "MODBUS_MASTER_TIME_OUT_RECEIPT", TYPE_INT, (void *) &ModbusTimeOutReceipt },
	{ "MODBUS_MASTER_TIME_AFTER_TRANSMIT", TYPE_INT, (void *) &ModbusTimeAfterTransmit },
#endif
	{ NULL, 0, NULL },
};

/**
* 	Read a config file and find matching entries
*	Store the configuration value
*/
static int read_configfile (char *fname, struct cfg_cfg *cfg)
{
       
	FILE	*fp;
	char	line[255];
	char	*val;
	int	i;
	
	 printf(_("INFO CLASSICLADDER---Reading MODBUS config file -%s\n"),fname);
	fp = fopen (fname, "r");
	if (fp == NULL) {
		fprintf (stderr, _("Cannot open %s file !!!\n"), fname);
		return -1;
	}
	
	while (!feof(fp)) {
		if (fgets (line, 254, fp) == NULL)
			break;

		if (line[0] == '#')
			continue;
			
		val = strchr (line, '=');
		if (!val)
			continue;
		*val++ = 0x0;
		
		for (i = 0; cfg[i].name != NULL; i++) {
			if (strcmp (cfg[i].name, line) != 0)
				continue;
			switch (cfg[i].type) {
			case TYPE_INT:	
				*((int *)(cfg[i].data)) = atoi (val);
				break;
			case TYPE_STRING: {
				char *p = strchr (val, '\n');
				if (p)
					*p = 0x0;
				strcpy ((char *)cfg[i].data, val);
				break;
			}	
			default:
				fprintf (stderr, _("Unknown configtype for %s: %d\n"), line, cfg[i].type);
				break;
			}	
			break;	
		}
	}
	fclose (fp);
	return 0;
}

int read_config (char * fname)
{
	return read_configfile (fname, maincfg);
}
