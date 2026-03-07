/**
 * @file conf_util.c
 * @brief Configuration utility functions shared between conf.c and
 *        conf_icmds.c.
 *
 * Provides three subsystems:
 *  - A dynamically-growing, linked-list output buffer used during XML parsing
 *    to accumulate configuration records before they are serialised into
 *    shared memory.
 *  - A thin wrapper around the expat XML parser that drives a state-machine
 *    defined by a caller-supplied transition table.
 *  - A hex-string decoder used for raw SDO/IDN data payloads.
 *
 * @copyright Copyright (C) 2018-2026 Sascha Ittner <sascha.ittner@modusoft.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>

#include "conf.h"
#include "conf_priv.h"

/** @brief Module name used in error messages; defined here, declared extern in conf_priv.h. */
char *modname = "lcec_conf";

static void xml_start_handler(void *data, const char *el, const char **attr);
static void xml_end_handler(void *data, const char *el);

/**
 * @brief Initialise an output buffer to the empty state.
 *
 * Sets all fields of @p buf to zero / NULL so that subsequent calls to
 * @ref addOutputBuffer() can safely append to it.
 *
 * @param buf  Pointer to the buffer descriptor to initialise.
 */
void initOutputBuffer(LCEC_CONF_OUTBUF_T *buf) {
  buf->head = NULL;
  buf->tail = NULL;
  buf->len = 0;
}

/**
 * @brief Append a zeroed payload block to the output buffer.
 *
 * Allocates a new @ref LCEC_CONF_OUTBUF_ITEM_T node immediately followed by
 * @p len zero-filled bytes and links it onto the tail of @p buf.  The total
 * byte count tracked by @c buf->len is incremented by @p len.
 *
 * @param buf  Output buffer to which the new block is appended.
 * @param len  Number of payload bytes to allocate and zero-fill.
 * @return Pointer to the start of the zeroed payload area on success,
 *         or @c NULL if memory allocation fails (an error is printed to
 *         @c stderr).
 * @note The caller receives a pointer to the payload, not to the node header.
 *       The header is managed internally and must not be accessed directly.
 */
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

/**
 * @brief Serialise the output buffer into a flat memory region, then free all nodes.
 *
 * Iterates the linked list from @c buf->head to the end, copying each payload
 * block contiguously into @p dest (if non-NULL) and freeing the node memory
 * regardless.  After this call the buffer is empty and all associated heap
 * memory has been released.
 *
 * @param buf   Output buffer to drain.
 * @param dest  Destination area that receives the concatenated payloads in
 *              insertion order.  Pass @c NULL to discard the data and only
 *              free memory (e.g. on error paths).
 * @note The caller must ensure that @p dest has at least @c buf->len bytes of
 *       writable space before calling this function.
 */
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

/**
 * @brief Initialise an expat XML parse instance with a state-transition table.
 *
 * Creates an expat @c XML_Parser, stores the @p states table pointer, sets
 * the initial parser state to 0, and registers the internal element-open and
 * element-close handlers that drive the state machine.
 *
 * @param inst    Caller-allocated instance to initialise.  The struct must
 *                be at the start of (or identical to) the caller's larger
 *                state struct so that callbacks can safely cast @p inst back
 *                to the richer type.
 * @param states  Null-terminated state-transition table (last row has
 *                @c el == @c NULL with state values of -1).
 * @return 0 on success, 1 if @c XML_ParserCreate() fails.
 * @note The caller is responsible for calling @c XML_ParserFree(inst->parser)
 *       when done, even on early failure paths after a successful init.
 */
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

/**
 * @brief Expat element-open callback that drives the state machine.
 *
 * Searches the transition table (@c inst->states) for a row whose @c el
 * matches @p el and whose @c state_from matches the current state.  If found,
 * the optional @c start_handler is called and the state is advanced to
 * @c state_to.  An unrecognised element causes the parser to be stopped with
 * an error message.
 *
 * @param data  Expat user data pointer; cast to @ref LCEC_CONF_XML_INST_T*.
 * @param el    Name of the opening XML element.
 * @param attr  Null-terminated array of alternating attribute name/value pairs.
 */
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

/**
 * @brief Expat element-close callback that drives the state machine.
 *
 * Searches the transition table for a row whose @c el matches @p el and whose
 * @c state_to matches the current state.  If found, the optional
 * @c end_handler is called and the state is restored to @c state_from.  An
 * unrecognised closing tag stops the parser.
 *
 * @param data  Expat user data pointer; cast to @ref LCEC_CONF_XML_INST_T*.
 * @param el    Name of the closing XML element.
 */
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

/**
 * @brief Decode a hex-encoded byte string into a byte buffer.
 *
 * Processes @p slen characters (or until NUL if @p slen is -1).  Each pair of
 * consecutive hex digits is decoded into one output byte.  Whitespace
 * characters (space, tab, CR, LF) between byte pairs are silently skipped;
 * whitespace mid-pair is an error.  Any other non-hex character returns -1.
 *
 * When @p buf is @c NULL the function performs a dry run: it validates the
 * input and returns the number of bytes that would be written, without
 * writing anything.  This is useful for pre-sizing an allocation.
 *
 * @param s     Input character string.
 * @param slen  Number of characters to process, or -1 to process until NUL.
 * @param buf   Output buffer receiving decoded bytes, or @c NULL for a dry run.
 * @return Number of decoded bytes on success, or -1 on invalid input
 *         (unrecognised character, or the string ends in the middle of a byte).
 */
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
