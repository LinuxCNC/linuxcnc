//
//  Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>

#include "lcec_conf.h"
#include "lcec_conf_priv.h"

char *modname = "lcec_conf";

static void xml_start_handler(void *data, const char *el, const char **attr);
static void xml_end_handler(void *data, const char *el);

void initOutputBuffer(LCEC_CONF_OUTBUF_T *buf) {
  buf->head = NULL;
  buf->tail = NULL;
  buf->len = 0;
}

void *addOutputBuffer(LCEC_CONF_OUTBUF_T *buf, size_t len) {

  void *p = calloc(1, sizeof(LCEC_CONF_OUTBUF_ITEM_T) + len);
  if (p == NULL) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for config token\n", modname);
    return NULL;
  }

  // setup header
  LCEC_CONF_OUTBUF_ITEM_T *header = p;
  p += sizeof(LCEC_CONF_OUTBUF_ITEM_T);
  header->len = len;
  buf->len += len;

  // update list
  if (buf->head == NULL) {
    buf->head = header;
  }
  if (buf->tail != NULL) {
    buf->tail->next = header;
  }
  buf->tail = header;

  return p;
}

void copyFreeOutputBuffer(LCEC_CONF_OUTBUF_T *buf, void *dest) {
  void *p;

  while (buf->head != NULL) {
    p = buf->head;
    if (dest != NULL) {
      memcpy(dest, p + sizeof(LCEC_CONF_OUTBUF_ITEM_T), buf->head->len);
      dest += buf->head->len;
    }
    buf->head = buf->head->next;
    free(p);
  }
}

int initXmlInst(LCEC_CONF_XML_INST_T *inst, const LCEC_CONF_XML_HANLDER_T *states) {
  // create xml parser
  inst->parser = XML_ParserCreate(NULL);
  if (inst->parser == NULL) {
    return 1;
  }

  // setup data
  inst->states = states;
  inst->state = 0;
  XML_SetUserData(inst->parser, inst);

  // setup handlers
  XML_SetElementHandler(inst->parser, xml_start_handler, xml_end_handler);

  return 0;
}

static void xml_start_handler(void *data, const char *el, const char **attr) {
  LCEC_CONF_XML_INST_T *inst = (LCEC_CONF_XML_INST_T *) data;
  const LCEC_CONF_XML_HANLDER_T *state;

  for (state = inst->states; state->el != NULL; state++) {
    if (inst->state == state->state_from && (strcmp(el, state->el) == 0)) {
      if (state->start_handler != NULL) {
        state->start_handler(inst, state->state_to, attr);
      }
      inst->state = state->state_to;
      return;
    } 
  }

  fprintf(stderr, "%s: ERROR: unexpected node %s found\n", modname, el);
  XML_StopParser(inst->parser, 0);
}

static void xml_end_handler(void *data, const char *el) {
  LCEC_CONF_XML_INST_T *inst = (LCEC_CONF_XML_INST_T *) data;
  const LCEC_CONF_XML_HANLDER_T *state;

  for (state = inst->states; state->el != NULL; state++) {
    if (inst->state == state->state_to && (strcmp(el, state->el) == 0)) {
      if (state->end_handler != NULL) {
        state->end_handler(inst, state->state_from);
      }
      inst->state = state->state_from;
      return;
    } 
  }

  fprintf(stderr, "%s: ERROR: unexpected close tag %s found\n", modname, el);
  XML_StopParser(inst->parser, 0);
}

int parseHex(const char *s, int slen, uint8_t *buf) {
  char c;
  int len;
  int nib;
  uint8_t tmp;

  for (len = 0, nib = 0, tmp = 0; (slen == -1 || slen > 0) && (c = *s) != 0; s++) {
    // update remaining length
    if (slen > 0) {
      slen --;
    }

    // skip blanks if no current nibble
    if (!nib && strchr(" \t\r\n", c)) {
      continue;
    }

    // get nibble value
    if (c >= '0' && c <= '9') {
      c = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      c = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      c = c - 'A' + 10;
    } else {
      return -1;
    }

    // store nibble
    if (nib) {
      tmp |= c & 0x0f;
      if (buf) {
        *(buf++) = tmp;
      }
      len++;
    } else {
      tmp = c << 4;
    }
    nib = !nib;
  }

  // nibble must not be active
  if (nib) {
    return -1;
  }

  return len;
}

