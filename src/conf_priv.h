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
/**
 * @file conf_priv.h
 * @brief Private declarations shared between conf.c, conf_util.c, and
 *        conf_icmds.c.
 *
 * This header is intentionally not installed; it declares the internal
 * output-buffer API, the expat XML parser wrapper types, and helper function
 * prototypes used only within the lcec_conf configuration tool.
 */
#ifndef _LCEC_CONF_PRIV_H_
#define _LCEC_CONF_PRIV_H_

#include <expat.h>

/** @brief Size in bytes of the read buffer used when feeding data to expat. */
#define BUFFSIZE 8192

struct LCEC_CONF_XML_HANLDER;

/**
 * @brief Runtime state of an active expat XML parse instance.
 *
 * Both the main conf.c parser and the init-commands parser in conf_icmds.c
 * embed this struct (as their first member) so that XML callbacks can be
 * passed around as a common base type and then cast to the richer state struct.
 */
typedef struct LCEC_CONF_XML_INST {
  XML_Parser parser;                        /**< The underlying expat parser handle. */
  const struct LCEC_CONF_XML_HANLDER *states; /**< Pointer to the state-transition table
                                               *   (array terminated by a sentinel entry). */
  int state;                                /**< Current parser state, an integer drawn from
                                             *   the relevant @c LCEC_*_TYPE_T enum. */
} LCEC_CONF_XML_INST_T;

/**
 * @brief One row in an XML element state-transition table.
 *
 * The parser walks this table on every element open/close event to decide
 * whether the element is expected in the current state and which callbacks
 * to invoke.  The table must be terminated by an entry whose @c el member
 * is @c NULL (conventionally written as @c "NULL" in the source).
 */
typedef struct LCEC_CONF_XML_HANLDER {
  const char *el;          /**< XML element name to match (e.g. @c "master"). */
  int state_from;          /**< Parser state required for this transition to fire. */
  int state_to;            /**< Parser state entered when the element's opening tag is seen. */
  /** @brief Optional callback invoked when the element's opening tag is parsed.
   *  @param inst   Pointer to the current XML parse instance.
   *  @param next   The @c state_to value for informational use.
   *  @param attr   Null-terminated array of alternating name/value strings. */
  void (*start_handler)(struct LCEC_CONF_XML_INST *inst, int next, const char **attr);
  /** @brief Optional callback invoked when the element's closing tag is parsed.
   *  @param inst   Pointer to the current XML parse instance.
   *  @param next   The @c state_from value (the state being restored). */
  void (*end_handler)(struct LCEC_CONF_XML_INST *inst, int next);
} LCEC_CONF_XML_HANLDER_T;

/**
 * @brief Linked-list node wrapping one allocation inside the output buffer.
 *
 * The output buffer is a singly-linked list of these items; each node is
 * allocated with @c calloc and holds @c len bytes of payload data immediately
 * following the struct header (i.e. at address @c (header + 1)).
 */
typedef struct LCEC_CONF_OUTBUF_ITEM {
  size_t len;                         /**< Byte length of the payload following this header. */
  struct LCEC_CONF_OUTBUF_ITEM *next; /**< Pointer to the next item, or @c NULL if last. */
} LCEC_CONF_OUTBUF_ITEM_T;

/**
 * @brief Head/tail descriptor for the dynamically-grown output buffer.
 *
 * The output buffer accumulates configuration records during XML parsing
 * and is later serialised into the shared-memory segment in a single pass
 * by @ref copyFreeOutputBuffer().
 */
typedef struct {
  LCEC_CONF_OUTBUF_ITEM_T *head; /**< First item in the list, or @c NULL if empty. */
  LCEC_CONF_OUTBUF_ITEM_T *tail; /**< Last item in the list, used for O(1) append. */
  size_t len;                    /**< Running total of all payload bytes in the list. */
} LCEC_CONF_OUTBUF_T;

/** @brief Name of the HAL component; defined in conf_util.c, used in error messages. */
extern char *modname;

/**
 * @brief Initialise an output buffer to the empty state.
 * @param buf  Pointer to the buffer descriptor to initialise.
 */
void initOutputBuffer(LCEC_CONF_OUTBUF_T *buf);

/**
 * @brief Append a zero-filled payload block to the output buffer.
 * @param buf  Output buffer to append to.
 * @param len  Number of payload bytes to allocate.
 * @return Pointer to the zeroed payload area on success, or @c NULL on
 *         allocation failure (an error message is printed to @c stderr).
 */
void *addOutputBuffer(LCEC_CONF_OUTBUF_T *buf, size_t len);

/**
 * @brief Copy every payload block from the buffer into @p dest, then free all nodes.
 * @param buf   Output buffer whose contents are to be flushed.
 * @param dest  Destination memory area.  If @c NULL the data is discarded and
 *              only the memory is freed (useful for cleanup on error paths).
 * @note After this call the buffer is empty and all its node memory is freed.
 */
void copyFreeOutputBuffer(LCEC_CONF_OUTBUF_T *buf, void *dest);

/**
 * @brief Parse init-commands from an EtherCAT ESI-style XML file.
 * @param slave      Slave record whose sdoConfigLength / idnConfigLength
 *                   fields are updated as commands are parsed.
 * @param outputBuf  Shared output buffer to which CoE/SoE records are appended.
 * @param filename   Path to the XML file containing @c \<EtherCATMailbox\> data.
 * @return 0 on success, 1 on any parse or I/O error.
 */
int parseIcmds(LCEC_CONF_SLAVE_T *slave, LCEC_CONF_OUTBUF_T *outputBuf, const char *filename);

/**
 * @brief Initialise an expat XML parse instance with a state-transition table.
 * @param inst    Instance to initialise; the caller is responsible for
 *                allocating the surrounding state struct.
 * @param states  State-transition table terminated by a sentinel row.
 * @return 0 on success, 1 if the expat parser could not be created.
 * @note On success the caller must eventually call @c XML_ParserFree(inst->parser).
 */
int initXmlInst(LCEC_CONF_XML_INST_T *inst, const LCEC_CONF_XML_HANLDER_T *states);

/**
 * @brief Parse a hex-encoded byte string into a byte buffer.
 *
 * Hex digits may be separated by whitespace (spaces, tabs, newlines).
 * Each pair of hex digits produces one output byte.
 *
 * @param s     Input string of hex characters (need not be NUL-terminated if
 *              @p slen is non-negative).
 * @param slen  Number of characters to consume, or -1 to consume until NUL.
 * @param buf   Output buffer to write decoded bytes into, or @c NULL to
 *              perform a dry-run that only counts the output bytes.
 * @return Number of decoded bytes on success, or -1 if the input contains an
 *         invalid character or ends mid-nibble.
 */
int parseHex(const char *s, int slen, uint8_t *buf);

#endif
