/**
 * @file conf_icmds.c
 * @brief Init-command (CoE SDO and SoE IDN) configuration parser.
 *
 * Reads an EtherCAT ESI-style XML file structured as:
 * @code
 *   <EtherCATMailbox>
 *     <CoE>
 *       <InitCmds>
 *         <InitCmd CompleteAccess="0">
 *           <Index>0x6040</Index>
 *           <SubIndex>0</SubIndex>
 *           <Data>06 00</Data>
 *         </InitCmd>
 *       </InitCmds>
 *     </CoE>
 *     <SoE>
 *       <InitCmds>
 *         <InitCmd>
 *           <Transition>PS</Transition>
 *           <DriveNo>0</DriveNo>
 *           <IDN>32768</IDN>
 *           <Data>01 00</Data>
 *         </InitCmd>
 *       </InitCmds>
 *     </SoE>
 *   </EtherCATMailbox>
 * @endcode
 *
 * Successfully parsed commands are appended to the shared output buffer as
 * @ref LCEC_CONF_SDOCONF_T or @ref LCEC_CONF_IDNCONF_T records, and the
 * parent slave's @c sdoConfigLength / @c idnConfigLength accumulators are
 * updated accordingly.
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

/**
 * @brief Internal XML element state tags for the init-commands parser.
 *
 * These values are used as the @c state / @c state_from / @c state_to
 * fields of the @ref LCEC_CONF_XML_HANLDER_T transition table and as the
 * @c inst->state discriminator inside callbacks.
 */
typedef enum {
  icmdTypeNone = 0,          /**< Initial / idle state. */
  icmdTypeMailbox,           /**< Inside @c \<EtherCATMailbox\>. */
  icmdTypeCoe,               /**< Inside @c \<CoE\>. */
  icmdTypeCoeIcmds,          /**< Inside @c \<CoE\>/\<InitCmds\>. */
  icmdTypeCoeIcmd,           /**< Inside a single @c \<CoE\>/\<InitCmd\>. */
  icmdTypeCoeIcmdTrans,      /**< Inside @c \<Transition\> of a CoE InitCmd. */
  icmdTypeCoeIcmdComment,    /**< Inside @c \<Comment\> of a CoE InitCmd (ignored). */
  icmdTypeCoeIcmdTimeout,    /**< Inside @c \<Timeout\> of a CoE InitCmd (ignored). */
  icmdTypeCoeIcmdCcs,        /**< Inside @c \<Ccs\> of a CoE InitCmd (ignored). */
  icmdTypeCoeIcmdIndex,      /**< Inside @c \<Index\> of a CoE InitCmd. */
  icmdTypeCoeIcmdSubindex,   /**< Inside @c \<SubIndex\> of a CoE InitCmd. */
  icmdTypeCoeIcmdData,       /**< Inside @c \<Data\> of a CoE InitCmd. */
  icmdTypeSoe,               /**< Inside @c \<SoE\>. */
  icmdTypeSoeIcmds,          /**< Inside @c \<SoE\>/\<InitCmds\>. */
  icmdTypeSoeIcmd,           /**< Inside a single @c \<SoE\>/\<InitCmd\>. */
  icmdTypeSoeIcmdTrans,      /**< Inside @c \<Transition\> of a SoE InitCmd. */
  icmdTypeSoeIcmdComment,    /**< Inside @c \<Comment\> of a SoE InitCmd (ignored). */
  icmdTypeSoeIcmdTimeout,    /**< Inside @c \<Timeout\> of a SoE InitCmd (ignored). */
  icmdTypeSoeIcmdOpcode,     /**< Inside @c \<OpCode\> of a SoE InitCmd (ignored). */
  icmdTypeSoeIcmdDriveno,    /**< Inside @c \<DriveNo\> of a SoE InitCmd. */
  icmdTypeSoeIcmdIdn,        /**< Inside @c \<IDN\> of a SoE InitCmd. */
  icmdTypeSoeIcmdElements,   /**< Inside @c \<Elements\> of a SoE InitCmd (ignored). */
  icmdTypeSoeIcmdAttribute,  /**< Inside @c \<Attribute\> of a SoE InitCmd (ignored). */
  icmdTypeSoeIcmdData        /**< Inside @c \<Data\> of a SoE InitCmd. */
} LCEC_ICMD_TYPE_T;

/**
 * @brief Parser state for the init-commands XML file.
 *
 * Extends @ref LCEC_CONF_XML_INST_T by embedding it as the first member so
 * that a pointer to this struct can be cast to @ref LCEC_CONF_XML_INST_T*
 * and vice-versa in XML callbacks.
 */
typedef struct {
  LCEC_CONF_XML_INST_T xml;          /**< Base XML parser instance; MUST be first. */

  LCEC_CONF_SLAVE_T *currSlave;      /**< Slave record whose length fields are updated. */
  LCEC_CONF_OUTBUF_T *outputBuf;     /**< Shared output buffer to append records to. */

  LCEC_CONF_SDOCONF_T *currSdoConf;  /**< CoE SDO record being filled (inside a CoE InitCmd). */
  LCEC_CONF_IDNCONF_T *currIdnConf;  /**< SoE IDN record being filled (inside a SoE InitCmd). */
} LCEC_CONF_ICMDS_STATE_T;

static void xml_data_handler(void *data, const XML_Char *s, int len);

static void icmdTypeCoeIcmdStart(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void icmdTypeCoeIcmdEnd(LCEC_CONF_XML_INST_T *inst, int next);
static void icmdTypeSoeIcmdStart(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void icmdTypeSoeIcmdEnd(LCEC_CONF_XML_INST_T *inst, int next);

/** @brief State-transition table for the init-commands XML parser. */
static const LCEC_CONF_XML_HANLDER_T xml_states[] = {
  { "EtherCATMailbox", icmdTypeNone, icmdTypeMailbox, NULL, NULL },
  { "CoE", icmdTypeMailbox, icmdTypeCoe, NULL, NULL },
  { "InitCmds", icmdTypeCoe, icmdTypeCoeIcmds, NULL, NULL },
  { "InitCmd", icmdTypeCoeIcmds, icmdTypeCoeIcmd, icmdTypeCoeIcmdStart, icmdTypeCoeIcmdEnd },
  { "Transition", icmdTypeCoeIcmd, icmdTypeCoeIcmdTrans, NULL, NULL },
  { "Comment", icmdTypeCoeIcmd, icmdTypeCoeIcmdComment, NULL, NULL },
  { "Timeout", icmdTypeCoeIcmd, icmdTypeCoeIcmdTimeout, NULL, NULL },
  { "Ccs", icmdTypeCoeIcmd, icmdTypeCoeIcmdCcs, NULL, NULL },
  { "Index", icmdTypeCoeIcmd, icmdTypeCoeIcmdIndex, NULL, NULL },
  { "SubIndex", icmdTypeCoeIcmd, icmdTypeCoeIcmdSubindex, NULL, NULL },
  { "Data", icmdTypeCoeIcmd, icmdTypeCoeIcmdData, NULL, NULL },
  { "SoE", icmdTypeMailbox, icmdTypeSoe, NULL, NULL },
  { "InitCmds", icmdTypeSoe, icmdTypeSoeIcmds, NULL, NULL },
  { "InitCmd", icmdTypeSoeIcmds, icmdTypeSoeIcmd, icmdTypeSoeIcmdStart, icmdTypeSoeIcmdEnd },
  { "Transition", icmdTypeSoeIcmd, icmdTypeSoeIcmdTrans, NULL, NULL },
  { "Comment", icmdTypeSoeIcmd, icmdTypeSoeIcmdComment, NULL, NULL },
  { "Timeout", icmdTypeSoeIcmd, icmdTypeSoeIcmdTimeout, NULL, NULL },
  { "OpCode", icmdTypeSoeIcmd, icmdTypeSoeIcmdOpcode, NULL, NULL },
  { "DriveNo", icmdTypeSoeIcmd, icmdTypeSoeIcmdDriveno, NULL, NULL },
  { "IDN", icmdTypeSoeIcmd, icmdTypeSoeIcmdIdn, NULL, NULL },
  { "Elements", icmdTypeSoeIcmd, icmdTypeSoeIcmdElements, NULL, NULL },
  { "Attribute", icmdTypeSoeIcmd, icmdTypeSoeIcmdAttribute, NULL, NULL },
  { "Data", icmdTypeSoeIcmd, icmdTypeSoeIcmdData, NULL, NULL },
  { "NULL", -1, -1, NULL, NULL }
};

static long int parse_int(LCEC_CONF_ICMDS_STATE_T *state, const char *s, int len, long int min, long int max);
static int parse_data(LCEC_CONF_ICMDS_STATE_T *state, const char *s, int len);

/**
 * @brief Parse CoE SDO and SoE IDN init commands from an EtherCAT ESI XML file.
 *
 * Opens @p filename and feeds it to an expat XML parser driven by the
 * @ref xml_states transition table.  As the parser encounters
 * @c \<CoE\>/\<InitCmd\> elements it allocates @ref LCEC_CONF_SDOCONF_T records
 * in @p outputBuf; @c \<SoE\>/\<InitCmd\> elements produce @ref LCEC_CONF_IDNCONF_T
 * records.  The @c sdoConfigLength and @c idnConfigLength fields of @p slave
 * are updated to reflect the total bytes consumed.
 *
 * @param slave      Slave descriptor whose length accumulators are updated.
 * @param outputBuf  Shared output buffer that receives the new records.
 * @param filename   Path to the ESI-style XML file to parse.
 * @return 0 on success, 1 on any I/O or parse error.
 */
int parseIcmds(LCEC_CONF_SLAVE_T *slave, LCEC_CONF_OUTBUF_T *outputBuf, const char *filename) {
  int ret = 1;
  int done;
  char buffer[BUFFSIZE];
  FILE *file;
  LCEC_CONF_ICMDS_STATE_T state;

  // open file
  file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "%s: ERROR: unable to open config file %s\n", modname, filename);
    goto fail1;
  }

  // create xml parser
  memset(&state, 0, sizeof(state));
  if (initXmlInst((LCEC_CONF_XML_INST_T *) &state, xml_states)) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for parser\n", modname);
    goto fail2;
  }

  // setup handlers
  XML_SetCharacterDataHandler(state.xml.parser, xml_data_handler);

  state.currSlave = slave;
  state.outputBuf = outputBuf;
  for (done=0; !done;) {
    // read block
    int len = fread(buffer, 1, BUFFSIZE, file);
    if (ferror(file)) {
      fprintf(stderr, "%s: ERROR: Couldn't read from file %s\n", modname, filename);
      goto fail3;
    }

    // check for EOF
    done = feof(file);

    // parse current block
    if (!XML_Parse(state.xml.parser, buffer, len, done)) {
      fprintf(stderr, "%s: ERROR: Parse error at line %u: %s\n", modname,
        (unsigned int)XML_GetCurrentLineNumber(state.xml.parser),
        XML_ErrorString(XML_GetErrorCode(state.xml.parser)));
      goto fail3;
    }
  }

  // everything is fine
  ret = 0;

fail3:
  XML_ParserFree(state.xml.parser);
fail2:
  fclose(file);
fail1:
  return ret;
}

/**
 * @brief Expat character-data handler for the init-commands parser.
 *
 * Called by expat for text content inside XML elements.  Dispatches on
 * the current parser state to update the appropriate field of the active
 * @ref LCEC_CONF_SDOCONF_T or @ref LCEC_CONF_IDNCONF_T record:
 *  - @c icmdTypeCoeIcmdIndex / @c icmdTypeCoeIcmdSubindex : sets @c index /
 *    @c subindex of the current CoE SDO record.
 *  - @c icmdTypeCoeIcmdData : appends hex-decoded bytes to the SDO payload.
 *  - @c icmdTypeCoeIcmdTrans : validates that transition is "IP" or "PS"
 *    (other states are silently accepted for forward compatibility).
 *  - @c icmdTypeSoeIcmdTrans : maps "IP"→PREOP, "PS"→PREOP, "SO"→SAFEOP.
 *  - @c icmdTypeSoeIcmdDriveno / @c icmdTypeSoeIcmdIdn : fills drive/IDN fields.
 *  - @c icmdTypeSoeIcmdData : appends hex-decoded bytes to the IDN payload.
 *
 * @param data  Expat user data; cast to @ref LCEC_CONF_ICMDS_STATE_T* via
 *              @ref LCEC_CONF_XML_INST_T*.
 * @param s     Character data (not NUL-terminated).
 * @param len   Number of valid characters in @p s.
 */
static void xml_data_handler(void *data, const XML_Char *s, int len) {
  LCEC_CONF_XML_INST_T *inst = (LCEC_CONF_XML_INST_T *) data;
  LCEC_CONF_ICMDS_STATE_T *state = (LCEC_CONF_ICMDS_STATE_T *) inst;

  switch (inst->state) {
    case icmdTypeCoeIcmdTrans:
      if (len == 2) {
        if (strncmp("IP", s, len) == 0) {
          return;
        }
        if (strncmp("PS", s, len) == 0) {
          return;
        }
      }
      fprintf(stderr, "%s: ERROR: Invalid Transition state\n", modname);
      XML_StopParser(inst->parser, 0);
      return;
    case icmdTypeCoeIcmdIndex:
      state->currSdoConf->index = parse_int(state, s, len, 0, 0xffff);
      return;
    case icmdTypeCoeIcmdSubindex:
      if (state->currSdoConf->subindex != LCEC_CONF_SDO_COMPLETE_SUBIDX) {
        state->currSdoConf->subindex = parse_int(state, s, len, 0, 0xff);
      }
      return;
    case icmdTypeCoeIcmdData:
      state->currSdoConf->length += parse_data(state, s, len);
      return;

    case icmdTypeSoeIcmdTrans:
      if (len == 2) {
        if (strncmp("IP", s, len) == 0) {
          state->currIdnConf->state = EC_AL_STATE_PREOP;
          return;
        }
        if (strncmp("PS", s, len) == 0) {
          state->currIdnConf->state = EC_AL_STATE_PREOP;
          return;
        }
        if (strncmp("SO", s, len) == 0) {
          state->currIdnConf->state = EC_AL_STATE_SAFEOP;
          return;
        }
      }
      fprintf(stderr, "%s: ERROR: Invalid Transition state\n", modname);
      XML_StopParser(inst->parser, 0);
      return;
    case icmdTypeSoeIcmdDriveno:
      state->currIdnConf->drive = parse_int(state, s, len, 0, 7);
      return;
    case icmdTypeSoeIcmdIdn:
      state->currIdnConf->idn = parse_int(state, s, len, 0, 0xffff);
      return;
    case icmdTypeSoeIcmdData:
      state->currIdnConf->length += parse_data(state, s, len);
      return;
  }
}

/**
 * @brief Start-element callback for a CoE @c \<InitCmd\> element.
 *
 * Allocates a new @ref LCEC_CONF_SDOCONF_T record in the output buffer,
 * initialises its fields to sentinel values (@c index=0xffff, @c subindex=0xff),
 * and checks for the optional @c CompleteAccess attribute.  If @c CompleteAccess
 * is non-zero, @c subindex is set to @ref LCEC_CONF_SDO_COMPLETE_SUBIDX so
 * that subsequent @c \<SubIndex\> text is ignored.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_ICMDS_STATE_T*.
 * @param next  State to transition to (unused here; state is set by the caller).
 * @param attr  Attribute name/value pairs; only @c CompleteAccess is recognised.
 * @note On allocation failure the parser is stopped.
 */
static void icmdTypeCoeIcmdStart(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_ICMDS_STATE_T *state = (LCEC_CONF_ICMDS_STATE_T *) inst;

  state->currSdoConf = addOutputBuffer(state->outputBuf, sizeof(LCEC_CONF_SDOCONF_T));

  if (state->currSdoConf == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currSdoConf->confType = lcecConfTypeSdoConfig;
  state->currSdoConf->index = 0xffff;
  state->currSdoConf->subindex = 0xff;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse CompleteAccess
    if (strcmp(name, "CompleteAccess") == 0) {
      if (atoi(val)) {
        state->currSdoConf->subindex = LCEC_CONF_SDO_COMPLETE_SUBIDX;
      }
      continue;
    }
  }
}

/**
 * @brief End-element callback for a CoE @c \<InitCmd\> element.
 *
 * Validates that the current @ref LCEC_CONF_SDOCONF_T record has both an
 * @c index and a @c subindex (i.e. they differ from the 0xffff/0xff sentinels
 * set in @ref icmdTypeCoeIcmdStart).  Updates the parent slave's
 * @c sdoConfigLength to include the full record size plus its data payload.
 * Stops the parser with an error message if validation fails.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_ICMDS_STATE_T*.
 * @param next  State being restored (unused here).
 */
static void icmdTypeCoeIcmdEnd(LCEC_CONF_XML_INST_T *inst, int next) {
  LCEC_CONF_ICMDS_STATE_T *state = (LCEC_CONF_ICMDS_STATE_T *) inst;

  if (state->currSdoConf->index == 0xffff) {
    fprintf(stderr, "%s: ERROR: sdoConfig has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  if (state->currSdoConf->subindex == 0xff) {
    fprintf(stderr, "%s: ERROR: sdoConfig has no subIdx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currSlave->sdoConfigLength += sizeof(LCEC_CONF_SDOCONF_T) + state->currSdoConf->length;
}

/**
 * @brief Start-element callback for a SoE @c \<InitCmd\> element.
 *
 * Allocates a new @ref LCEC_CONF_IDNCONF_T record in the output buffer with
 * fields initialised to sentinel values (drive=0, idn=0xffff, state=0).
 * No attributes are defined on this element; all field values come from
 * child text nodes handled by @ref xml_data_handler.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_ICMDS_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Attribute list (expected empty; any attributes are ignored).
 * @note On allocation failure the parser is stopped.
 */
static void icmdTypeSoeIcmdStart(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_ICMDS_STATE_T *state = (LCEC_CONF_ICMDS_STATE_T *) inst;

  state->currIdnConf = addOutputBuffer(state->outputBuf, sizeof(LCEC_CONF_IDNCONF_T));

  if (state->currIdnConf == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currIdnConf->confType = lcecConfTypeIdnConfig;
  state->currIdnConf->drive = 0;
  state->currIdnConf->idn = 0xffff;
  state->currIdnConf->state = 0;
}

/**
 * @brief End-element callback for a SoE @c \<InitCmd\> element.
 *
 * Validates that the current @ref LCEC_CONF_IDNCONF_T record has both a
 * valid @c idn (≠ 0xffff) and a non-zero @c state.  Updates the parent
 * slave's @c idnConfigLength.  Stops the parser on validation failure.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_ICMDS_STATE_T*.
 * @param next  State being restored (unused here).
 */
static void icmdTypeSoeIcmdEnd(LCEC_CONF_XML_INST_T *inst, int next) {
  LCEC_CONF_ICMDS_STATE_T *state = (LCEC_CONF_ICMDS_STATE_T *) inst;

  if (state->currIdnConf->idn == 0xffff) {
    fprintf(stderr, "%s: ERROR: idnConfig has no idn attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  if (state->currIdnConf->state == 0) {
    fprintf(stderr, "%s: ERROR: idnConfig has no state attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currSlave->idnConfigLength += sizeof(LCEC_CONF_IDNCONF_T) + state->currIdnConf->length;
}

/**
 * @brief Parse a bounded integer from a non-NUL-terminated character buffer.
 *
 * Copies at most @p len characters into a temporary stack buffer, calls
 * @c strtol() with base 0 (allowing decimal, hex @c 0x, or octal @c 0),
 * and verifies the result lies within [@p min, @p max].  Stops the parser
 * and returns 0 on any error.
 *
 * @param state  Parser state (provides access to the expat parser handle).
 * @param s      Input character data (not NUL-terminated).
 * @param len    Number of valid characters in @p s; must be < 32.
 * @param min    Inclusive lower bound for the parsed value.
 * @param max    Inclusive upper bound for the parsed value.
 * @return Parsed value on success, 0 on error (parser is stopped in that case).
 */
static long int parse_int(LCEC_CONF_ICMDS_STATE_T *state, const char *s, int len, long int min, long int max) {
  char buf[32];
  char *end;
  long int ret;

  if (s == NULL || len == 0) {
    fprintf(stderr, "%s: ERROR: Missing number value\n", modname);
    XML_StopParser(state->xml.parser, 0);
    return 0;
  }

  if (len >= sizeof(buf)) {
    fprintf(stderr, "%s: ERROR: Number value size exceeded\n", modname);
    XML_StopParser(state->xml.parser, 0);
    return 0;
  }

  strncpy(buf, s, len);
  buf[len] = 0;

  ret = strtol(buf, &end, 0);
  if (*end != 0 || ret < min || ret > max) {
    fprintf(stderr, "%s: ERROR: Invalid number value '%s'\n", modname, s);
    XML_StopParser(state->xml.parser, 0);
    return 0;
  }

  return ret;
}

/**
 * @brief Decode hex-encoded binary data and append it to the output buffer.
 *
 * Calls @ref parseHex() twice: first with @p buf == NULL to determine the
 * decoded byte count, then allocates that many bytes via @ref addOutputBuffer()
 * and fills them.  Returns the number of bytes appended so the caller can
 * accumulate a running total in the parent record's @c length field.
 * Stops the parser on invalid hex or allocation failure.
 *
 * @param state  Parser state (provides access to output buffer and parser handle).
 * @param s      Hex character data (not NUL-terminated).
 * @param len    Number of valid characters in @p s.
 * @return Number of decoded bytes appended, or 0 on error.
 */
static int parse_data(LCEC_CONF_ICMDS_STATE_T *state, const char *s, int len) {
  uint8_t *p;
  int size;

  // get size
  size = parseHex(s, len, NULL);
  if (size < 0) {
    fprintf(stderr, "%s: ERROR: Invalid data\n", modname);
    XML_StopParser(state->xml.parser, 0);
    return 0;
  }

  // allocate memory
  p = (uint8_t *) addOutputBuffer(state->outputBuf, size);
  if (p == NULL) {
    XML_StopParser(state->xml.parser, 0);
    return 0;
  }

  // parse data
  parseHex(s, len, p);
  return size;
}
