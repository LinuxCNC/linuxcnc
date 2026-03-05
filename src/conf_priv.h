//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
#ifndef _LCEC_CONF_PRIV_H_
#define _LCEC_CONF_PRIV_H_

#include <expat.h>

#define BUFFSIZE 8192

struct LCEC_CONF_XML_HANLDER;

typedef struct LCEC_CONF_XML_INST {
  XML_Parser parser;
  const struct LCEC_CONF_XML_HANLDER *states;
  int state;
} LCEC_CONF_XML_INST_T;

typedef struct LCEC_CONF_XML_HANLDER {
  const char *el;
  int state_from;
  int state_to;
  void (*start_handler)(struct LCEC_CONF_XML_INST *inst, int next, const char **attr);
  void (*end_handler)(struct LCEC_CONF_XML_INST *inst, int next);
} LCEC_CONF_XML_HANLDER_T;

typedef struct LCEC_CONF_OUTBUF_ITEM {
  size_t len;
  struct LCEC_CONF_OUTBUF_ITEM *next;
} LCEC_CONF_OUTBUF_ITEM_T;

typedef struct {
  LCEC_CONF_OUTBUF_ITEM_T *head;
  LCEC_CONF_OUTBUF_ITEM_T *tail;
  size_t len;
} LCEC_CONF_OUTBUF_T;

extern char *modname;

void initOutputBuffer(LCEC_CONF_OUTBUF_T *buf);
void *addOutputBuffer(LCEC_CONF_OUTBUF_T *buf, size_t len);
void copyFreeOutputBuffer(LCEC_CONF_OUTBUF_T *buf, void *dest);

int parseIcmds(LCEC_CONF_SLAVE_T *slave, LCEC_CONF_OUTBUF_T *outputBuf, const char *filename);

int initXmlInst(LCEC_CONF_XML_INST_T *inst, const LCEC_CONF_XML_HANLDER_T *states);

int parseHex(const char *s, int slen, uint8_t *buf);

#endif
