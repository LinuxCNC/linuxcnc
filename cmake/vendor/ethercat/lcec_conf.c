//
//  Copyright (C) 2012 Sascha Ittner <sascha.ittner@modusoft.de>
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
#include <signal.h>
#include <sys/eventfd.h>

#include "rtapi.h"
#include "hal.h"

#include "lcec_rtapi.h"
#include "lcec_conf.h"
#include "lcec_conf_priv.h"

#include "lcec_stmds5k.h"
#include "lcec_el6900.h"
#include "lcec_el1918_logic.h"
#include "lcec_el70x1.h"
#include "lcec_el7411.h"

typedef struct {
  const char *name;
  int id;
  hal_type_t type;
  int pdoMappingCount;
} LCEC_CONF_MODPARAM_DESC_T;

typedef struct {
  const char *name;
  LCEC_SLAVE_TYPE_T type;
  const LCEC_CONF_MODPARAM_DESC_T *modParams;
} LCEC_CONF_TYPELIST_T;

typedef struct {
  hal_u32_t *master_count;
  hal_u32_t *slave_count;
} LCEC_CONF_HAL_T;

static const LCEC_CONF_MODPARAM_DESC_T slaveStMDS5kParams[] = {
  { "isMultiturn", LCEC_STMDS5K_PARAM_MULTITURN, HAL_BIT, 0 } ,
  { "extEnc", LCEC_STMDS5K_PARAM_EXTENC, HAL_U32, LCEC_STMDS5K_EXTINC_PDOS } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL6900Params[] = {
  { "fsoeSlaveIdx", LCEC_EL6900_PARAM_SLAVEID, HAL_U32, LCEC_EL6900_PARAM_SLAVEID_PDOS } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL1918_LOGICParams[] = {
  { "fsoeSlaveIdx", LCEC_EL1918_LOGIC_PARAM_SLAVEID, HAL_U32, LCEC_EL1918_LOGIC_PARAM_SLAVEID_PDOS } ,
  { "stdInCount", LCEC_EL1918_LOGIC_PARAM_STDINCOUNT, HAL_U32, LCEC_EL1918_LOGIC_PARAM_STDINCOUNT_PDOS } ,
  { "stdOutCount", LCEC_EL1918_LOGIC_PARAM_STDOUTCOUNT, HAL_U32, LCEC_EL1918_LOGIC_PARAM_STDOUTCOUNT_PDOS } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL70x1Params[] = {
  { "maxCurrent", LCEC_EL70x1_PARAM_MAX_CURR, HAL_U32, 0 } ,
  { "redCurrent", LCEC_EL70x1_PARAM_RED_CURR, HAL_U32, 0 } ,
  { "nomVoltage", LCEC_EL70x1_PARAM_NOM_VOLT, HAL_U32, 0 } ,
  { "coilRes", LCEC_EL70x1_PARAM_COIL_RES, HAL_U32, 0 } ,
  { "motorEMF", LCEC_EL70x1_PARAM_MOTOR_EMF, HAL_U32, 0 } ,
  { NULL }
};


static const LCEC_CONF_MODPARAM_DESC_T slaveEL7411Params[] = {

  { "dcLinkNominal", LCEC_EL7411_PARAM_DCLINK_NOM, HAL_U32, 0 } ,
  { "dcLinkMin", LCEC_EL7411_PARAM_DCLINK_MIN, HAL_U32, 0 } ,
  { "dcLinkMax", LCEC_EL7411_PARAM_DCLINK_MAX, HAL_U32, 0 } ,
  { "maxCurrent", LCEC_EL7411_PARAM_MAX_CURR, HAL_U32, 0 } ,
  { "ratedCurrent", LCEC_EL7411_PARAM_RATED_CURR, HAL_U32, 0 } ,
  { "ratedVoltage", LCEC_EL7411_PARAM_RATED_VOLT, HAL_U32, 0 } ,
  { "polePairs", LCEC_EL7411_PARAM_POLE_PAIRS, HAL_U32, 0 } ,
  { "coilRes", LCEC_EL7411_PARAM_RESISTANCE, HAL_U32, 0 } ,
  { "coilInd", LCEC_EL7411_PARAM_INDUCTANCE, HAL_U32, 0 } ,
  { "torqueConst", LCEC_EL7411_PARAM_TOURQUE_CONST, HAL_U32, 0 } ,
  { "voltageConst", LCEC_EL7411_PARAM_VOLTAGE_CONST, HAL_U32, 0 } ,
  { "rotorInertia", LCEC_EL7411_PARAM_ROTOR_INERTIA, HAL_U32, 0 } ,
  { "maxSpeed", LCEC_EL7411_PARAM_MAX_SPEED, HAL_U32, 0 } ,
  { "ratedSpeed", LCEC_EL7411_PARAM_RATED_SPEED, HAL_U32, 0 } ,
  { "thermalTimeConst", LCEC_EL7411_PARAM_TH_TIME_CONST, HAL_U32, 0 } ,
  { "hallVoltage", LCEC_EL7411_PARAM_HALL_VOLT, HAL_U32, 0 } ,
  { "hallAdjust", LCEC_EL7411_PARAM_HALL_ADJUST, HAL_S32, 0 } ,
  { NULL }
};

static const LCEC_CONF_TYPELIST_T slaveTypes[] = {
  // bus coupler
  { "EK1100", lcecSlaveTypeEK1100, NULL },
  { "EK1101", lcecSlaveTypeEK1101, NULL },
  { "EK1110", lcecSlaveTypeEK1110, NULL },
  { "EK1122", lcecSlaveTypeEK1122, NULL },

  // generic device
  { "generic", lcecSlaveTypeGeneric, NULL },

  // AX5000 servo drives
  { "AX5203", lcecSlaveTypeAX5203, NULL },
  { "AX5206", lcecSlaveTypeAX5206, NULL },

  // digital in
  { "EL1002", lcecSlaveTypeEL1002, NULL },
  { "EL1004", lcecSlaveTypeEL1004, NULL },
  { "EL1008", lcecSlaveTypeEL1008, NULL },
  { "EL1012", lcecSlaveTypeEL1012, NULL },
  { "EL1014", lcecSlaveTypeEL1014, NULL },
  { "EL1018", lcecSlaveTypeEL1018, NULL },
  { "EL1024", lcecSlaveTypeEL1024, NULL },
  { "EL1034", lcecSlaveTypeEL1034, NULL },
  { "EL1084", lcecSlaveTypeEL1084, NULL },
  { "EL1088", lcecSlaveTypeEL1088, NULL },
  { "EL1094", lcecSlaveTypeEL1094, NULL },
  { "EL1098", lcecSlaveTypeEL1098, NULL },
  { "EL1104", lcecSlaveTypeEL1104, NULL },
  { "EL1114", lcecSlaveTypeEL1114, NULL },
  { "EL1124", lcecSlaveTypeEL1124, NULL },
  { "EL1134", lcecSlaveTypeEL1134, NULL },
  { "EL1144", lcecSlaveTypeEL1144, NULL },
  { "EL1252", lcecSlaveTypeEL1252, NULL },
  { "EL1808", lcecSlaveTypeEL1808, NULL },
  { "EL1809", lcecSlaveTypeEL1809, NULL },
  { "EL1819", lcecSlaveTypeEL1819, NULL },

  // digital out
  { "EL2002", lcecSlaveTypeEL2002, NULL },
  { "EL2004", lcecSlaveTypeEL2004, NULL },
  { "EL2008", lcecSlaveTypeEL2008, NULL },
  { "EL2022", lcecSlaveTypeEL2022, NULL },
  { "EL2024", lcecSlaveTypeEL2024, NULL },
  { "EL2032", lcecSlaveTypeEL2032, NULL },
  { "EL2034", lcecSlaveTypeEL2034, NULL },
  { "EL2042", lcecSlaveTypeEL2042, NULL },
  { "EL2084", lcecSlaveTypeEL2084, NULL },
  { "EL2088", lcecSlaveTypeEL2088, NULL },
  { "EL2124", lcecSlaveTypeEL2124, NULL },
  { "EL2202", lcecSlaveTypeEL2202, NULL },
  { "EL2612", lcecSlaveTypeEL2612, NULL },
  { "EL2622", lcecSlaveTypeEL2622, NULL },
  { "EL2634", lcecSlaveTypeEL2634, NULL },
  { "EL2652", lcecSlaveTypeEL2652, NULL },
  { "EL2808", lcecSlaveTypeEL2808, NULL },
  { "EL2798", lcecSlaveTypeEL2798, NULL },
  { "EL2809", lcecSlaveTypeEL2809, NULL },

  { "EP2008", lcecSlaveTypeEP2008, NULL },
  { "EP2028", lcecSlaveTypeEP2028, NULL },
  { "EP2809", lcecSlaveTypeEP2809, NULL },

  // digital in(out
  { "EL1859", lcecSlaveTypeEL1859, NULL },

  // analog in, 2ch, 16 bits
  { "EL3102", lcecSlaveTypeEL3102, NULL },
  { "EL3112", lcecSlaveTypeEL3112, NULL },
  { "EL3122", lcecSlaveTypeEL3122, NULL },
  { "EL3142", lcecSlaveTypeEL3142, NULL },
  { "EL3152", lcecSlaveTypeEL3152, NULL },
  { "EL3162", lcecSlaveTypeEL3162, NULL },

  // analog in, 4ch, 16 bits
  { "EL3164", lcecSlaveTypeEL3164, NULL },

  // analog in, 5ch, 16 bits
  { "EL3255", lcecSlaveTypeEL3255, NULL },

  // analog out, 1ch, 12 bits
  { "EL4001", lcecSlaveTypeEL4001, NULL },
  { "EL4011", lcecSlaveTypeEL4011, NULL },
  { "EL4021", lcecSlaveTypeEL4021, NULL },
  { "EL4031", lcecSlaveTypeEL4031, NULL },

  // analog out, 2ch, 12 bits
  { "EL4002", lcecSlaveTypeEL4002, NULL },
  { "EL4012", lcecSlaveTypeEL4012, NULL },
  { "EL4022", lcecSlaveTypeEL4022, NULL },
  { "EL4032", lcecSlaveTypeEL4032, NULL },

  // analog out, 2ch, 16 bits
  { "EL4102", lcecSlaveTypeEL4102, NULL },
  { "EL4112", lcecSlaveTypeEL4112, NULL },
  { "EL4122", lcecSlaveTypeEL4122, NULL },
  { "EL4132", lcecSlaveTypeEL4132, NULL },

  // analog out, 4ch, 16 bits
  { "EL4104", lcecSlaveTypeEL4104, NULL },
  { "EL4134", lcecSlaveTypeEL4134, NULL },

  // analog out, 8ch, 12 bits
  { "EL4008", lcecSlaveTypeEL4008, NULL },
  { "EL4018", lcecSlaveTypeEL4018, NULL },
  { "EL4028", lcecSlaveTypeEL4028, NULL },
  { "EL4038", lcecSlaveTypeEL4038, NULL },

  // encoder inputs
  { "EL5101", lcecSlaveTypeEL5101, NULL },
  { "EL5151", lcecSlaveTypeEL5151, NULL },
  { "EL5152", lcecSlaveTypeEL5152, NULL },

  // pulse train (stepper) output
  { "EL2521", lcecSlaveTypeEL2521, NULL },

  // stepper
  { "EL7031", lcecSlaveTypeEL7031, slaveEL70x1Params },
  { "EL7041-0052", lcecSlaveTypeEL7041_0052, slaveEL70x1Params },
  { "EL7041-1000", lcecSlaveTypeEL7041_1000, NULL },

  // ac servo
  { "EL7201-9014", lcecSlaveTypeEL7201_9014, NULL },
  { "EL7211", lcecSlaveTypeEL7211, NULL },
  { "EL7221", lcecSlaveTypeEL7221, NULL },

  // dc servo
  { "EL7342", lcecSlaveTypeEL7342, NULL },

  // BLDC
  { "EL7411", lcecSlaveTypeEL7411, slaveEL7411Params },

  // power suppply
  { "EL9505", lcecSlaveTypeEL9505, NULL },
  { "EL9508", lcecSlaveTypeEL9508, NULL },
  { "EL9510", lcecSlaveTypeEL9510, NULL },
  { "EL9512", lcecSlaveTypeEL9512, NULL },
  { "EL9515", lcecSlaveTypeEL9515, NULL },
  { "EL9576", lcecSlaveTypeEL9576, NULL },

  // FSoE devices
  { "EL6900", lcecSlaveTypeEL6900, slaveEL6900Params },
  { "EL1918_LOGIC", lcecSlaveTypeEL1918_LOGIC, slaveEL1918_LOGICParams },
  { "EL1904", lcecSlaveTypeEL1904, NULL },
  { "EL2904", lcecSlaveTypeEL2904, NULL },

  // multi axis interface
  { "EM7004", lcecSlaveTypeEM7004, NULL },

  // stoeber MDS5000 series
  { "StMDS5k", lcecSlaveTypeStMDS5k, slaveStMDS5kParams },

  // Delta ASDA series
  { "DeASDA", lcecSlaveTypeDeASDA, NULL },

  // Omron G5 series
  { "R88D-KNA5L-ECT", lcecSlaveTypeOmrG5_KNA5L, NULL },
  { "R88D-KN01L-ECT", lcecSlaveTypeOmrG5_KN01L, NULL },
  { "R88D-KN02L-ECT", lcecSlaveTypeOmrG5_KN02L, NULL },
  { "R88D-KN04L-ECT", lcecSlaveTypeOmrG5_KN04L, NULL },
  { "R88D-KN01H-ECT", lcecSlaveTypeOmrG5_KN01H, NULL },
  { "R88D-KN02H-ECT", lcecSlaveTypeOmrG5_KN02H, NULL },
  { "R88D-KN04H-ECT", lcecSlaveTypeOmrG5_KN04H, NULL },
  { "R88D-KN08H-ECT", lcecSlaveTypeOmrG5_KN08H, NULL },
  { "R88D-KN10H-ECT", lcecSlaveTypeOmrG5_KN10H, NULL },
  { "R88D-KN15H-ECT", lcecSlaveTypeOmrG5_KN15H, NULL },
  { "R88D-KN20H-ECT", lcecSlaveTypeOmrG5_KN20H, NULL },
  { "R88D-KN30H-ECT", lcecSlaveTypeOmrG5_KN30H, NULL },
  { "R88D-KN50H-ECT", lcecSlaveTypeOmrG5_KN50H, NULL },
  { "R88D-KN75H-ECT", lcecSlaveTypeOmrG5_KN75H, NULL },
  { "R88D-KN150H-ECT", lcecSlaveTypeOmrG5_KN150H, NULL },
  { "R88D-KN06F-ECT", lcecSlaveTypeOmrG5_KN06F, NULL },
  { "R88D-KN10F-ECT", lcecSlaveTypeOmrG5_KN10F, NULL },
  { "R88D-KN15F-ECT", lcecSlaveTypeOmrG5_KN15F, NULL },
  { "R88D-KN20F-ECT", lcecSlaveTypeOmrG5_KN20F, NULL },
  { "R88D-KN30F-ECT", lcecSlaveTypeOmrG5_KN30F, NULL },
  { "R88D-KN50F-ECT", lcecSlaveTypeOmrG5_KN50F, NULL },
  { "R88D-KN75F-ECT", lcecSlaveTypeOmrG5_KN75F, NULL },
  { "R88D-KN150F-ECT", lcecSlaveTypeOmrG5_KN150F, NULL },

  // modusoft PH3LM2RM converter
  { "Ph3LM2RM", lcecSlaveTypePh3LM2RM, NULL },

  { NULL }
};

static int hal_comp_id;
static LCEC_CONF_HAL_T *conf_hal_data;
static int shmem_id;

static int exitEvent;

typedef struct {
  LCEC_CONF_XML_INST_T xml;

  LCEC_CONF_MASTER_T *currMaster;
  const LCEC_CONF_TYPELIST_T *currSlaveType;
  LCEC_CONF_SLAVE_T *currSlave;
  LCEC_CONF_SYNCMANAGER_T *currSyncManager;
  LCEC_CONF_PDO_T *currPdo;
  LCEC_CONF_SDOCONF_T *currSdoConf;
  LCEC_CONF_IDNCONF_T *currIdnConf;
  LCEC_CONF_PDOENTRY_T *currPdoEntry;
  uint8_t currComplexBitOffset;

  LCEC_CONF_OUTBUF_T outputBuf;
} LCEC_CONF_XML_STATE_T;

static void parseMasterAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseSlaveAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseDcConfAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseWatchdogAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseSdoConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseIdnConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseDataRawAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseInitCmdsAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseSyncManagerAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parsePdoAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parsePdoEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseComplexEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseModParamAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);

static const LCEC_CONF_XML_HANLDER_T xml_states[] = {
  { "masters", lcecConfTypeNone, lcecConfTypeMasters, NULL, NULL },
  { "master", lcecConfTypeMasters, lcecConfTypeMaster, parseMasterAttrs, NULL },
  { "slave", lcecConfTypeMaster, lcecConfTypeSlave, parseSlaveAttrs, NULL },
  { "dcConf", lcecConfTypeSlave, lcecConfTypeDcConf, parseDcConfAttrs, NULL },
  { "watchdog", lcecConfTypeSlave, lcecConfTypeWatchdog, parseWatchdogAttrs, NULL },
  { "sdoConfig", lcecConfTypeSlave, lcecConfTypeSdoConfig, parseSdoConfigAttrs, NULL },
  { "sdoDataRaw", lcecConfTypeSdoConfig, lcecConfTypeSdoDataRaw, parseDataRawAttrs, NULL },
  { "idnConfig", lcecConfTypeSlave, lcecConfTypeIdnConfig, parseIdnConfigAttrs, NULL },
  { "idnDataRaw", lcecConfTypeIdnConfig, lcecConfTypeIdnDataRaw, parseDataRawAttrs, NULL },
  { "initCmds", lcecConfTypeSlave, lcecConfTypeInitCmds, parseInitCmdsAttrs, NULL },
  { "syncManager", lcecConfTypeSlave, lcecConfTypeSyncManager, parseSyncManagerAttrs, NULL },
  { "pdo", lcecConfTypeSyncManager, lcecConfTypePdo, parsePdoAttrs, NULL },
  { "pdoEntry", lcecConfTypePdo, lcecConfTypePdoEntry, parsePdoEntryAttrs, NULL },
  { "complexEntry", lcecConfTypePdoEntry, lcecConfTypeComplexEntry, parseComplexEntryAttrs, NULL },
  { "modParam", lcecConfTypeSlave, lcecConfTypeModParam, parseModParamAttrs, NULL },
  { "NULL", -1, -1, NULL, NULL }
};

static int parseSyncCycle(LCEC_CONF_XML_STATE_T *state, const char *nptr);

static void exitHandler(int sig) {
  uint64_t u = 1;
  if (write(exitEvent, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, "%s: ERROR: error writing exit event\n", modname);
  }
}

int main(int argc, char **argv) {
  int ret = 1;
  char *filename;
  int done;
  char buffer[BUFFSIZE];
  FILE *file;
  LCEC_CONF_NULL_T *end;
  void *shmem_ptr;
  LCEC_CONF_HEADER_T *header;
  uint64_t u;
  LCEC_CONF_XML_STATE_T state;

  // initialize component
  hal_comp_id = hal_init(modname);
  if (hal_comp_id < 1) {
    fprintf(stderr, "%s: ERROR: hal_init failed\n", modname);
    goto fail0;
  }

  // allocate hal memory
  conf_hal_data = hal_malloc(sizeof(LCEC_CONF_HAL_T));
  if (conf_hal_data == NULL) {
    fprintf(stderr, "%s: ERROR: unable to allocate HAL shared memory\n", modname);
    goto fail1;
  }

  // register pins
  if (hal_pin_u32_newf(HAL_OUT, &(conf_hal_data->master_count), hal_comp_id, "%s.conf.master-count", LCEC_MODULE_NAME) != 0) {
    fprintf(stderr, "%s: ERROR: unable to register pin %s.conf.master-count\n", modname, LCEC_MODULE_NAME);
    goto fail1;
  }
  if (hal_pin_u32_newf(HAL_OUT, &(conf_hal_data->slave_count), hal_comp_id, "%s.conf.slave-count", LCEC_MODULE_NAME) != 0) {
    fprintf(stderr, "%s: ERROR: unable to register pin %s.conf.slave-count\n", modname, LCEC_MODULE_NAME);
    goto fail1;
  }
  *(conf_hal_data->master_count) = 0;
  *(conf_hal_data->slave_count) = 0;

  // initialize signal handling
  exitEvent = eventfd(0, 0);
  if (exitEvent == -1) {
    fprintf(stderr, "%s: ERROR: unable to create exit event\n", modname);
    goto fail1;
  }
  signal(SIGINT, exitHandler);
  signal(SIGTERM, exitHandler);

  // get config file name
  if (argc != 2) {
    fprintf(stderr, "%s: ERROR: invalid arguments\n", modname);
    goto fail2;
  }
  filename = argv[1];

  // open file
  file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "%s: ERROR: unable to open config file %s\n", modname, filename);
    goto fail2;
  }

  // create xml parser
  memset(&state, 0, sizeof(state));
  if (initXmlInst((LCEC_CONF_XML_INST_T *) &state, xml_states)) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for parser\n", modname);
    goto fail3;
  }

  initOutputBuffer(&state.outputBuf);
  for (done=0; !done;) {
    // read block
    int len = fread(buffer, 1, BUFFSIZE, file);
    if (ferror(file)) {
      fprintf(stderr, "%s: ERROR: Couldn't read from file %s\n", modname, filename);
      goto fail4;
    }

    // check for EOF
    done = feof(file);

    // parse current block
    if (!XML_Parse(state.xml.parser, buffer, len, done)) {
      fprintf(stderr, "%s: ERROR: Parse error at line %u: %s\n", modname,
        (unsigned int)XML_GetCurrentLineNumber(state.xml.parser),
        XML_ErrorString(XML_GetErrorCode(state.xml.parser)));
      goto fail4;
    }
  }

  // set end marker
  end = addOutputBuffer(&state.outputBuf, sizeof(LCEC_CONF_NULL_T));
  if (end == NULL) {
      goto fail4;
  }
  end->confType = lcecConfTypeNone;

  // setup shared mem for config
  shmem_id = rtapi_shmem_new(LCEC_CONF_SHMEM_KEY, hal_comp_id, sizeof(LCEC_CONF_HEADER_T) + state.outputBuf.len);
  if ( shmem_id < 0 ) {
    fprintf(stderr, "%s: ERROR: couldn't allocate user/RT shared memory\n", modname);
    goto fail4;
  }
  if (lcec_rtapi_shmem_getptr(shmem_id, &shmem_ptr) < 0) {
    fprintf(stderr, "%s: ERROR: couldn't map user/RT shared memory\n", modname);
    goto fail5;
  }

  // setup header
  header = shmem_ptr;
  shmem_ptr += sizeof(LCEC_CONF_HEADER_T);
  header->magic = LCEC_CONF_SHMEM_MAGIC;
  header->length = state.outputBuf.len;

  // copy data and free buffer
  copyFreeOutputBuffer(&state.outputBuf, shmem_ptr);

  // everything is fine
  ret = 0;
  hal_ready(hal_comp_id);

  // wait for SIGTERM
  if (read(exitEvent, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, "%s: ERROR: error reading exit event\n", modname);
  }

fail5:
  rtapi_shmem_delete(shmem_id, hal_comp_id);
fail4:
  copyFreeOutputBuffer(&state.outputBuf, NULL);
  XML_ParserFree(state.xml.parser);
fail3:
  fclose(file);
fail2:
  close(exitEvent);
fail1:
  hal_exit(hal_comp_id);
fail0:
  return ret;
}

static void parseMasterAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_MASTER_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_MASTER_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeMaster;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      p->index = atoi(val);
      continue;
    }

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(p->name, val, LCEC_CONF_STR_MAXLEN);
      p->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // parse appTimePeriod
    if (strcmp(name, "appTimePeriod") == 0) {
      p->appTimePeriod = atol(val);
      continue;
    }

    // parse refClockSyncCycles
    if (strcmp(name, "refClockSyncCycles") == 0) {
      p->refClockSyncCycles = atoll(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid master attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set default name
  if (p->name[0] == 0) {
    snprintf(p->name, LCEC_CONF_STR_MAXLEN, "%d", p->index);
  }

  (*(conf_hal_data->master_count))++;
  state->currMaster = p;
}

static void parseSlaveAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_SLAVE_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_SLAVE_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  const LCEC_CONF_TYPELIST_T *slaveType = NULL;
  p->confType = lcecConfTypeSlave;
  p->type = lcecSlaveTypeInvalid;

  // pre parse slave type to avoid attribute ordering problems
  const char **iter = attr;
  while(*iter) {
    const char *name = *(iter++);
    const char *val = *(iter++);

    // parse slave type
    if (strcmp(name, "type") == 0) {
      for (slaveType = slaveTypes; slaveType->name != NULL; slaveType++) {
        if (strcmp(val, slaveType->name) == 0) {
          break;
        }
      }
      if (slaveType->name == NULL) {
        fprintf(stderr, "%s: ERROR: Invalid slave type %s\n", modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->type = slaveType->type;
      continue;
    }
  }

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // skip slave type (already parsed)
    if (strcmp(name, "type") == 0) {
      continue;
    }

    // parse index
    if (strcmp(name, "idx") == 0) {
      p->index = atoi(val);
      continue;
    }

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(p->name, val, LCEC_CONF_STR_MAXLEN);
      p->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // generic only attributes
    if (p->type == lcecSlaveTypeGeneric) {
      // parse vid (hex value)
      if (strcmp(name, "vid") == 0) {
        p->vid = strtol(val, NULL, 16);
        continue;
      }

      // parse pid (hex value)
      if (strcmp(name, "pid") == 0) {
        p->pid = strtol(val, NULL, 16);
        continue;
      }

      // parse configPdos
      if (strcmp(name, "configPdos") == 0) {
        p->configPdos = (strcasecmp(val, "true") == 0);
        continue;
      }
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid slave attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set default name
  if (p->name[0] == 0) {
    snprintf(p->name, LCEC_CONF_STR_MAXLEN, "%d", p->index);
  }

  // type is required
  if (p->type == lcecSlaveTypeInvalid) {
    fprintf(stderr, "%s: ERROR: Slave has no type attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (*(conf_hal_data->slave_count))++;
  state->currSlaveType = slaveType;
  state->currSlave = p;
}

static void parseDcConfAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_DC_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_DC_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeDcConf;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse assignActivate (hex value)
    if (strcmp(name, "assignActivate") == 0) {
      p->assignActivate = strtol(val, NULL, 16);
      continue;
    }

    // parse sync0Cycle
    if (strcmp(name, "sync0Cycle") == 0) {
      p->sync0Cycle = parseSyncCycle(state, val);
      continue;
    }

    // parse sync0Shift
    if (strcmp(name, "sync0Shift") == 0) {
      p->sync0Shift = atoi(val);
      continue;
    }

    // parse sync1Cycle
    if (strcmp(name, "sync1Cycle") == 0) {
      p->sync1Cycle = parseSyncCycle(state, val);
      continue;
    }

    // parse sync1Shift
    if (strcmp(name, "sync1Shift") == 0) {
      p->sync1Shift = atoi(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid dcConfig attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }
}

static void parseWatchdogAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_WATCHDOG_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_WATCHDOG_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeWatchdog;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse divider
    if (strcmp(name, "divider") == 0) {
      p->divider = atoi(val);
      continue;
    }

    // parse intervals
    if (strcmp(name, "intervals") == 0) {
      p->intervals = atoi(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid watchdog attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }
}

static void parseSdoConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_SDOCONF_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_SDOCONF_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeSdoConfig;
  p->index = 0xffff;
  p->subindex = 0xff;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid sdoConfig idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // parse subIdx
    if (strcmp(name, "subIdx") == 0) {
      if (strcasecmp(val, "complete") == 0) {
        p->subindex = LCEC_CONF_SDO_COMPLETE_SUBIDX;
        continue;
      }
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xff) {
        fprintf(stderr, "%s: ERROR: Invalid sdoConfig subIdx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->subindex = tmp;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid sdoConfig attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xffff) {
    fprintf(stderr, "%s: ERROR: sdoConfig has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // subIdx is required
  if (p->subindex == 0xff) {
    fprintf(stderr, "%s: ERROR: sdoConfig has no subIdx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currSdoConf = p;
  state->currSlave->sdoConfigLength += sizeof(LCEC_CONF_SDOCONF_T);
}

static void parseIdnConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_IDNCONF_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_IDNCONF_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeIdnConfig;
  p->drive = 0;
  p->idn = 0xffff;
  p->state = 0;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "drive") == 0) {
      tmp = atoi(val);
      if (tmp < 0 || tmp > 7) {
        fprintf(stderr, "%s: ERROR: Invalid idnConfig drive %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }

      p->drive = tmp;
      continue;
    }

    // parse idn
    if (strcmp(name, "idn") == 0) {
      char pfx = val[0];
      if (pfx == 0) {
        fprintf(stderr, "%s: ERROR: Missing idnConfig idn value\n", modname);
        XML_StopParser(inst->parser, 0);
        return;
      }

      pfx = toupper(pfx);

      tmp = 0xffff;
      if (pfx == 'S' || pfx == 'P') {
        int set;
        int block;
	if (sscanf(val, "%c-%d-%d", &pfx, &set, &block) == 3) {
          if (set < 0 || set >= (1 << 3)) {
            fprintf(stderr, "%s: ERROR: Invalid idnConfig idn set %d\n", modname, set);
            XML_StopParser(inst->parser, 0);
            return;
          }

          if (block < 0 || block >= (1 << 12)) {
            fprintf(stderr, "%s: ERROR: Invalid idnConfig idn block %d\n", modname, block);
            XML_StopParser(inst->parser, 0);
            return;
          }

          tmp = (set << 12) | block;
          if (pfx == 'P') {
            tmp |= (15 << 1);
          }
        }
      } else if (pfx >= '0' && pfx <= '9') {
        tmp = atoi(val);
      }

      if (tmp == 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid idnConfig idn value '%s'\n", modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }

      p->idn = tmp;
      continue;
    }

    // parse state
    if (strcmp(name, "drive") == 0) {
      if (strcmp(val, "PREOP") == 0) {
        p->state = EC_AL_STATE_PREOP;
      } else if (strcmp(val, "SAFEOP") == 0) {
        p->state = EC_AL_STATE_SAFEOP;
      } else {
        fprintf(stderr, "%s: ERROR: Invalid idnConfig state '%s'\n", modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }

      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid idnConfig attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idn is required
  if (p->idn == 0xffff) {
    fprintf(stderr, "%s: ERROR: idnConfig has no idn attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // state is required
  if (p->state == 0) {
    fprintf(stderr, "%s: ERROR: idnConfig has no state attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currIdnConf = p;
  state->currSlave->idnConfigLength += sizeof(LCEC_CONF_IDNCONF_T);
}

static void parseDataRawAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int len;
  uint8_t *p;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse data
    if (strcmp(name, "data") == 0) {
      len = parseHex(val, -1, NULL);
      if (len < 0) {
        fprintf(stderr, "%s: ERROR: Invalid dataRaw data\n", modname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      if (len > 0) {
        p = (uint8_t *) addOutputBuffer(&state->outputBuf, len);
        if (p != NULL) {
          parseHex(val, -1, p);
          switch (inst->state) {
            case lcecConfTypeSdoConfig:
              state->currSdoConf->length += len;
              state->currSlave->sdoConfigLength += len;
              break;
            case lcecConfTypeIdnConfig:
              state->currIdnConf->length += len;
              state->currSlave->idnConfigLength += len;
              break;
          }
        }
      }
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid pdoEntry attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }
}

static void parseInitCmdsAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  const char *filename = NULL;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse filename
    if (strcmp(name, "filename") == 0) {
      filename = val;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid syncManager attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // filename is required
  if (filename == NULL || *filename == 0) {
    fprintf(stderr, "%s: ERROR: initCmds has no filename attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // try to parse initCmds
  if (parseIcmds(state->currSlave, &state->outputBuf, filename)) {
    XML_StopParser(inst->parser, 0);
    return;
  }
}

static void parseSyncManagerAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_SYNCMANAGER_T *p;

  // only allowed on generic slave
  if (state->currSlave->type != lcecSlaveTypeGeneric) {
    fprintf(stderr, "%s: ERROR: syncManager is only allowed on generic slaves\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_SYNCMANAGER_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeSyncManager;
  p->index = 0xff;
  p->dir = EC_DIR_INVALID;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = atoi(val);
      if (tmp < 0 || tmp >= EC_MAX_SYNC_MANAGERS) {
        fprintf(stderr, "%s: ERROR: Invalid syncManager idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // parse dir
    if (strcmp(name, "dir") == 0) {
      if (strcasecmp(val, "in") == 0) {
        p->dir = EC_DIR_INPUT;
        continue;
      }
      if (strcasecmp(val, "out") == 0) {
        p->dir = EC_DIR_OUTPUT;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid syncManager dir %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid syncManager attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xff) {
    fprintf(stderr, "%s: ERROR: syncManager has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // dir is required
  if (p->dir == EC_DIR_INVALID) {
    fprintf(stderr, "%s: ERROR: syncManager has no dir attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (state->currSlave->syncManagerCount)++;
  state->currSyncManager = p;
}

static void parsePdoAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_PDO_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_PDO_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypePdo;
  p->index = 0xffff;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid pdo idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid pdo attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xffff) {
    fprintf(stderr, "%s: ERROR: pdo has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (state->currSlave->pdoCount)++;
  (state->currSyncManager->pdoCount)++;
  state->currPdo = p;
}

static void parsePdoEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  int floatReq;
  LCEC_CONF_PDOENTRY_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_PDOENTRY_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  floatReq = 0;
  p->confType = lcecConfTypePdoEntry;
  p->index = 0xffff;
  p->subindex = 0xff;
  p->floatScale = 1.0;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid pdoEntry idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // parse subIdx
    if (strcmp(name, "subIdx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xff) {
        fprintf(stderr, "%s: ERROR: Invalid pdoEntry subIdx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->subindex = tmp;
      continue;
    }

    // parse bitLen
    if (strcmp(name, "bitLen") == 0) {
      tmp = atoi(val);
      if (tmp <= 0 || tmp > LCEC_CONF_GENERIC_MAX_BITLEN) {
        fprintf(stderr, "%s: ERROR: Invalid pdoEntry bitLen %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->bitLength = tmp;
      continue;
    }

    // parse halType
    if (strcmp(name, "halType") == 0) {
      if (strcasecmp(val, "bit") == 0) {
        p->halType = HAL_BIT;
        continue;
      }
      if (strcasecmp(val, "s32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_S32;
        continue;
      }
      if (strcasecmp(val, "u32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_U32;
        continue;
      }
      if (strcasecmp(val, "float") == 0) {
        p->subType = lcecPdoEntTypeFloatSigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      if (strcasecmp(val, "float-unsigned") == 0) {
        p->subType = lcecPdoEntTypeFloatUnsigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      if (strcasecmp(val, "complex") == 0) {
        p->subType = lcecPdoEntTypeComplex;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid pdoEntry halType %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // parse scale
    if (strcmp(name, "scale") == 0) {
      floatReq = 1;
      p->floatScale = atof(val);
      continue;
    }

    // parse offset
    if (strcmp(name, "offset") == 0) {
      floatReq = 1;
      p->floatOffset = atof(val);
      continue;
    }

    // parse halPin
    if (strcmp(name, "halPin") == 0) {
      strncpy(p->halPin, val, LCEC_CONF_STR_MAXLEN);
      p->halPin[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid pdoEntry attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xffff) {
    fprintf(stderr, "%s: ERROR: pdoEntry has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // subIdx is required
  if (p->subindex == 0xff) {
    fprintf(stderr, "%s: ERROR: pdoEntry has no subIdx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // bitLen is required
  if (p->bitLength == 0) {
    fprintf(stderr, "%s: ERROR: pdoEntry has no bitLen attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // pin name must not be given for complex subtype
  if (p->subType == lcecPdoEntTypeComplex && p->halPin[0] != 0) {
    fprintf(stderr, "%s: ERROR: pdoEntry has halPin attributes but pin type is 'complex'\n", modname);
    XML_StopParser(inst->parser, 0);
  }

  // check for float type if required
  if (floatReq && p->halType != HAL_FLOAT) {
    fprintf(stderr, "%s: ERROR: pdoEntry has scale/offset attributes but pin type is not 'float'\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (state->currSlave->pdoEntryCount)++;
  if (p->halPin[0] != 0) {
    (state->currSlave->pdoMappingCount)++;
  }
  (state->currPdo->pdoEntryCount)++;
  state->currPdoEntry = p;
  state->currComplexBitOffset = 0;
}

static void parseComplexEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  int floatReq;
  LCEC_CONF_COMPLEXENTRY_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_COMPLEXENTRY_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  floatReq = 0;
  p->confType = lcecConfTypeComplexEntry;
  p->bitOffset = state->currComplexBitOffset;
  p->floatScale = 1.0;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse bitLen
    if (strcmp(name, "bitLen") == 0) {
      tmp = atoi(val);
      if (tmp <= 0 || tmp > LCEC_CONF_GENERIC_MAX_SUBPINS) {
        fprintf(stderr, "%s: ERROR: Invalid complexEntry bitLen %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      if ((state->currComplexBitOffset + tmp) > state->currPdoEntry->bitLength) {
        fprintf(stderr, "%s: ERROR: complexEntry bitLen sum exceeded pdoEntry bitLen %d\n", modname, state->currPdoEntry->bitLength);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->bitLength = tmp;
      continue;
    }

    // parse halType
    if (strcmp(name, "halType") == 0) {
      if (strcasecmp(val, "bit") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_BIT;
        continue;
      }
      if (strcasecmp(val, "s32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_S32;
        continue;
      }
      if (strcasecmp(val, "u32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_U32;
        continue;
      }
      if (strcasecmp(val, "float") == 0) {
        p->subType = lcecPdoEntTypeFloatSigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      if (strcasecmp(val, "float-unsigned") == 0) {
        p->subType = lcecPdoEntTypeFloatUnsigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid complexEntry halType %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // parse scale
    if (strcmp(name, "scale") == 0) {
      floatReq = 1;
      p->floatScale = atof(val);
      continue;
    }

    // parse offset
    if (strcmp(name, "offset") == 0) {
      floatReq = 1;
      p->floatOffset = atof(val);
      continue;
    }

    // parse halPin
    if (strcmp(name, "halPin") == 0) {
      strncpy(p->halPin, val, LCEC_CONF_STR_MAXLEN);
      p->halPin[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid complexEntry attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // bitLen is required
  if (p->bitLength == 0) {
    fprintf(stderr, "%s: ERROR: complexEntry has no bitLen attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // check for float type if required
  if (floatReq && p->halType != HAL_FLOAT) {
    fprintf(stderr, "%s: ERROR: complexEntry has scale/offset attributes but pin type is not 'float'\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  if (p->halPin[0] != 0) {
    (state->currSlave->pdoMappingCount)++;
  }
  state->currComplexBitOffset += p->bitLength;
}

static void parseModParamAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  const char *pname, *pval;
  const LCEC_CONF_MODPARAM_DESC_T *modParams;

  if (state->currSlaveType->modParams == NULL) {
    fprintf(stderr, "%s: ERROR: modParam not allowed for this slave\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  LCEC_CONF_MODPARAM_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_MODPARAM_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeModParam;

  pname = NULL;
  pval = NULL;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // get name
    if (strcmp(name, "name") == 0) {
      pname = val;
      continue;
    }

    // get value
    if (strcmp(name, "value") == 0) {
      pval = val;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid modParam attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // name is required
  if (pname == NULL || pname[0] == 0) {
    fprintf(stderr, "%s: ERROR: modParam has no name attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // value is required
  if (pval == NULL || pval[0] == 0) {
    fprintf(stderr, "%s: ERROR: modParam has no value attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // search for matching param name
  for (modParams = state->currSlaveType->modParams; modParams->name != NULL; modParams++) {
    if (strcmp(pname, modParams->name) == 0) {
      break;
    }
  }
  if (modParams->name == NULL) {
    fprintf(stderr, "%s: ERROR: Invalid modParam '%s'\n", modname, pname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set id
  p->id = modParams->id;

  // try to parse value
  char *s = NULL;
  switch (modParams->type) {
    case HAL_BIT:
      if ((strcmp("1", pval) == 0) || (strcasecmp("TRUE", pval) == 0)) {
        p->value.bit = 1;
      } else if ((strcmp("0", pval) == 0) || (strcasecmp("FALSE", pval)) == 0) {
        p->value.bit = 0;
      } else {
        fprintf(stderr, "%s: ERROR: Invalid modParam bit value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;
    case HAL_U32:
      p->value.u32 = strtoul(pval, &s, 0);
      if (*s != 0) {
        fprintf(stderr, "%s: ERROR: Invalid modParam u32 value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;
    case HAL_S32:
      p->value.s32 = strtol(pval, &s, 0);
      if (*s != 0) {
        fprintf(stderr, "%s: ERROR: Invalid modParam s32 value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;
    case HAL_FLOAT:
      p->value.flt = strtod(pval, &s);
      if (*s != 0) {
        fprintf(stderr, "%s: ERROR: Invalid modParam float value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;
    default:
      p->value.u32 = 0;
      break;
  }

  (state->currSlave->pdoMappingCount) += modParams->pdoMappingCount; 
  (state->currSlave->modParamCount)++;
}

static int parseSyncCycle(LCEC_CONF_XML_STATE_T *state, const char *nptr) {
  // chack for master period multiples
  if (*nptr == '*') {
    nptr++;
    return atoi(nptr) * state->currMaster->appTimePeriod;
  }

  // custom value
  return atoi(nptr);
}

