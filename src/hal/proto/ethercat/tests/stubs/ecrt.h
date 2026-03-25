/* Stub ecrt.h for test builds — replaces the real IgH EtherCAT master header */
#ifndef _ECRT_H_
#define _ECRT_H_

#include <stdint.h>
#include <stddef.h>

/* -----------------------------------------------------------------------
 * Forward-declared opaque handle types
 * ----------------------------------------------------------------------- */
typedef struct ec_master       ec_master_t;
typedef struct ec_slave_config ec_slave_config_t;
typedef struct ec_domain       ec_domain_t;

/* -----------------------------------------------------------------------
 * Direction and watchdog enumerations
 * ----------------------------------------------------------------------- */
typedef enum {
    EC_DIR_INVALID = 0,
    EC_DIR_OUTPUT,
    EC_DIR_INPUT,
    EC_DIR_COUNT,
} ec_direction_t;

typedef enum {
    EC_WD_DEFAULT = 0,
    EC_WD_ENABLE,
    EC_WD_DISABLE,
} ec_watchdog_mode_t;

/* AL (Application Layer) state */
typedef enum {
    EC_AL_STATE_INIT    = 1,
    EC_AL_STATE_PREOP   = 2,
    EC_AL_STATE_SAFEOP  = 4,
    EC_AL_STATE_OP      = 8,
} ec_al_state_t;

/* -----------------------------------------------------------------------
 * PDO entry / PDO / sync-manager info structures
 * (used in static initialiser arrays in device drivers)
 * ----------------------------------------------------------------------- */
typedef struct {
    uint16_t index;
    uint8_t  subindex;
    uint8_t  bit_length;
} ec_pdo_entry_info_t;

typedef struct {
    uint16_t             index;
    unsigned int         n_entries;
    ec_pdo_entry_info_t *entries;
} ec_pdo_info_t;

typedef struct {
    uint8_t             index;
    ec_direction_t      dir;
    unsigned int        n_pdos;
    ec_pdo_info_t      *pdos;
    ec_watchdog_mode_t  watchdog_mode;
} ec_sync_info_t;

/* -----------------------------------------------------------------------
 * PDO entry registration structure
 * This is what LCEC_PDO_INIT writes into and what the test counts.
 * ----------------------------------------------------------------------- */
typedef struct {
    uint16_t      position;
    uint32_t      vendor_id;
    uint32_t      product_code;
    uint16_t      index;
    uint8_t       subindex;
    unsigned int *offset;
    unsigned int *bit_position;
} ec_pdo_entry_reg_t;

/* -----------------------------------------------------------------------
 * State structures
 * ----------------------------------------------------------------------- */
typedef struct {
    uint32_t     slaves_responding;
    uint32_t     al_states;
    unsigned int link_up;
} ec_master_state_t;

typedef struct {
    unsigned int online;
    unsigned int redundancy_active;
    unsigned int operational;
    uint8_t      al_state;
} ec_slave_config_state_t;

/* -----------------------------------------------------------------------
 * Process-data access macros (used in read/write callbacks, not in init)
 * ----------------------------------------------------------------------- */
#define EC_READ_BIT(ptr, pos)  ((*((uint8_t *)(ptr)) >> (pos)) & 0x01)
#define EC_WRITE_BIT(ptr, pos, val) \
    do { *((uint8_t *)(ptr)) = \
        (*((uint8_t *)(ptr)) & ~(1 << (pos))) | \
        ((((uint8_t)(val)) & 0x01) << (pos)); } while (0)

#define EC_READ_U8(ptr)   (*((uint8_t  *)(ptr)))
#define EC_READ_U16(ptr)  (*((uint16_t *)(ptr)))
#define EC_READ_U32(ptr)  (*((uint32_t *)(ptr)))
#define EC_READ_U64(ptr)  (*((uint64_t *)(ptr)))
#define EC_READ_S8(ptr)   (*((int8_t   *)(ptr)))
#define EC_READ_S16(ptr)  (*((int16_t  *)(ptr)))
#define EC_READ_S32(ptr)  (*((int32_t  *)(ptr)))
#define EC_READ_S64(ptr)  (*((int64_t  *)(ptr)))
#define EC_READ_REAL(ptr) (*((float    *)(ptr)))

#define EC_WRITE_U8(ptr, val)  do { *((uint8_t  *)(ptr)) = (val); } while (0)
#define EC_WRITE_U16(ptr, val) do { *((uint16_t *)(ptr)) = (val); } while (0)
#define EC_WRITE_U32(ptr, val) do { *((uint32_t *)(ptr)) = (val); } while (0)
#define EC_WRITE_S8(ptr, val)  do { *((int8_t   *)(ptr)) = (val); } while (0)
#define EC_WRITE_S16(ptr, val) do { *((int16_t  *)(ptr)) = (val); } while (0)
#define EC_WRITE_S32(ptr, val) do { *((int32_t  *)(ptr)) = (val); } while (0)

/* -----------------------------------------------------------------------
 * SDO / IDN configuration stubs
 * Implementations in test_pdo_counts.c — all return 0 (success).
 * ----------------------------------------------------------------------- */
extern int ecrt_slave_config_sdo8(ec_slave_config_t *sc,
                                  uint16_t index, uint8_t subindex,
                                  uint8_t value);
extern int ecrt_slave_config_sdo16(ec_slave_config_t *sc,
                                   uint16_t index, uint8_t subindex,
                                   uint16_t value);
extern int ecrt_slave_config_sdo32(ec_slave_config_t *sc,
                                   uint16_t index, uint8_t subindex,
                                   uint32_t value);
extern int ecrt_slave_config_sdo(ec_slave_config_t *sc,
                                 uint16_t index, uint8_t subindex,
                                 const uint8_t *data, size_t data_size);
extern int ecrt_slave_config_complete_sdo(ec_slave_config_t *sc,
                                          uint16_t index,
                                          const uint8_t *data,
                                          size_t data_size);
extern int ecrt_slave_config_idn(ec_slave_config_t *sc,
                                 uint8_t drive_no, uint16_t idn,
                                 ec_al_state_t state,
                                 const uint8_t *data, size_t data_size);
extern int ecrt_master_sdo_upload(ec_master_t *master,
                                  uint16_t slave_position,
                                  uint16_t index, uint8_t subindex,
                                  uint8_t *target, size_t target_size,
                                  size_t *result_size, uint32_t *abort_code);

#endif
