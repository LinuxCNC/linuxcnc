// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * tests/test_pdo_counts.c — Build-time PDO-count regression test
 *
 * Exercises every driver in typelist[] and verifies that proc_init writes
 * exactly slave->pdo_entry_count PDO entries (the same invariant enforced at
 * runtime by main.c).
 *
 * Build (from the tests/ directory):
 *   make -C tests
 *
 * Or manually:
 *   cd tests
 *   make
 *
 * Returns 0 on success, 1 if any driver fails the count check.
 *
 * Design notes
 * ------------
 * All external APIs that proc_init functions call are stubbed here so that
 * the test compiles and links without the real RTAPI, HAL, or EtherCAT
 * master libraries.  The real LCEC_PDO_INIT macro (from src/lcec.h) is used
 * unchanged; it dereferences **pdo_entry_regs and increments the pointer,
 * which is exactly what we measure.
 *
 * Entries in typelist[] are skipped when:
 *   - proc_init == NULL          (bus couplers — nothing to count)
 *   - is_fsoe_logic != 0        (FSoE logic slaves need cross-slave modparams)
 *   - proc_preinit returns != 0 (slave depends on neighbours, e.g. AX5805)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* Pull in the real lcec.h (types, LCEC_PDO_INIT macro, …) and priv.h
 * (lcec_typelist_t, extern typelist[]).  The stub headers in tests/stubs/
 * shadow hal.h, ecrt.h, and the rtapi_*.h headers so the build succeeds
 * without LinuxCNC or EtherLab installed. */
#include "../src/priv.h"

/* =========================================================================
 * Globals referenced by driver code
 * ========================================================================= */

int comp_id = 42;
ec_master_state_t global_ms;

/* =========================================================================
 * HAL stub implementations
 * ========================================================================= */

/* hal_malloc — drivers use this to allocate their private HAL data.
 * Return zeroed memory so pointer dereferences in init functions are safe. */
void *hal_malloc(long int size) {
    return calloc(1, (size_t)size);
}

int hal_pin_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
                void **data_ptr_addr, int comp_id_arg) {
    (void)name; (void)type; (void)dir; (void)data_ptr_addr; (void)comp_id_arg;
    return 0;
}

int hal_param_new(const char *name, hal_type_t type, hal_pin_dir_t dir,
                  void *data_addr, int comp_id_arg) {
    (void)name; (void)type; (void)dir; (void)data_addr; (void)comp_id_arg;
    return 0;
}

/* =========================================================================
 * RTAPI stub implementations
 * ========================================================================= */

void rtapi_print_msg(int level, const char *fmt, ...) {
    (void)level;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* =========================================================================
 * EtherCAT master API stubs
 * ========================================================================= */

int ecrt_slave_config_sdo8(ec_slave_config_t *sc,
                            uint16_t index, uint8_t subindex,
                            uint8_t value) {
    (void)sc; (void)index; (void)subindex; (void)value;
    return 0;
}

int ecrt_slave_config_sdo16(ec_slave_config_t *sc,
                             uint16_t index, uint8_t subindex,
                             uint16_t value) {
    (void)sc; (void)index; (void)subindex; (void)value;
    return 0;
}

int ecrt_slave_config_sdo32(ec_slave_config_t *sc,
                             uint16_t index, uint8_t subindex,
                             uint32_t value) {
    (void)sc; (void)index; (void)subindex; (void)value;
    return 0;
}

int ecrt_slave_config_sdo(ec_slave_config_t *sc,
                           uint16_t index, uint8_t subindex,
                           const uint8_t *data, size_t data_size) {
    (void)sc; (void)index; (void)subindex; (void)data; (void)data_size;
    return 0;
}

int ecrt_slave_config_complete_sdo(ec_slave_config_t *sc,
                                    uint16_t index,
                                    const uint8_t *data,
                                    size_t data_size) {
    (void)sc; (void)index; (void)data; (void)data_size;
    return 0;
}

int ecrt_slave_config_idn(ec_slave_config_t *sc,
                           uint8_t drive_no, uint16_t idn,
                           ec_al_state_t state,
                           const uint8_t *data, size_t data_size) {
    (void)sc; (void)drive_no; (void)idn; (void)state;
    (void)data; (void)data_size;
    return 0;
}

int ecrt_master_sdo_upload(ec_master_t *master,
                            uint16_t slave_position,
                            uint16_t index, uint8_t subindex,
                            uint8_t *target, size_t target_size,
                            size_t *result_size, uint32_t *abort_code) {
    (void)master; (void)slave_position; (void)index; (void)subindex;
    if (target)       memset(target, 0, target_size);
    if (result_size)  *result_size = target_size;
    if (abort_code)   *abort_code  = 0;
    return 0;
}

/* =========================================================================
 * lcec slave API stubs (declared in src/lcec.h, normally in src/slave.c)
 * ========================================================================= */

int lcec_read_sdo(struct lcec_slave *slave,
                  uint16_t index, uint8_t subindex,
                  uint8_t *target, size_t size) {
    (void)slave; (void)index; (void)subindex;
    if (target) memset(target, 0, size);
    return 0;
}

int lcec_read_idn(struct lcec_slave *slave,
                  uint8_t drive_no, uint16_t idn,
                  uint8_t *target, size_t size) {
    (void)slave; (void)drive_no; (void)idn;
    if (target) memset(target, 0, size);
    return 0;
}

/*
 * lcec_pin_newf / lcec_pin_newf_list:
 *   Drivers call these to register HAL pins.  The real implementation
 *   allocates HAL shared memory and stores a pointer at *data_ptr_addr.
 *   Drivers then write to the pin via *(chan->some_pin) = value, so
 *   data_ptr_addr must be set to a valid (zeroed) block, otherwise
 *   those writes would NULL-dereference and segfault.
 */
int lcec_pin_newf(hal_type_t type, hal_pin_dir_t dir,
                  void **data_ptr_addr, const char *fmt, ...) {
    (void)type; (void)dir; (void)fmt;
    if (data_ptr_addr) {
        *data_ptr_addr = calloc(1, sizeof(hal_u64_t));  /* enough for any HAL scalar */
        if (!*data_ptr_addr) return -ENOMEM;
    }
    return 0;
}

int lcec_pin_newf_list(void *base, const lcec_pindesc_t *list, ...) {
    const lcec_pindesc_t *p;
    if (!base || !list) return 0;
    for (p = list; p->type != HAL_TYPE_UNSPECIFIED; p++) {
        void **data_ptr_addr = (void **)((char *)base + p->offset);
        *data_ptr_addr = calloc(1, sizeof(hal_u64_t));
        if (!*data_ptr_addr) return -ENOMEM;
    }
    return 0;
}

int lcec_param_newf(hal_type_t type, hal_pin_dir_t dir,
                    void *data_addr, const char *fmt, ...) {
    (void)type; (void)dir; (void)data_addr; (void)fmt;
    return 0;
}

int lcec_param_newf_list(void *base, const lcec_pindesc_t *list, ...) {
    (void)base; (void)list;
    return 0;
}

/* Also stub the private _newfv variants declared in priv.h */
int lcec_pin_newfv(hal_type_t type, hal_pin_dir_t dir,
                   void **data_ptr_addr, const char *fmt, va_list ap) {
    (void)type; (void)dir; (void)fmt; (void)ap;
    if (data_ptr_addr) {
        *data_ptr_addr = calloc(1, sizeof(hal_u64_t));
        if (!*data_ptr_addr) return -ENOMEM;
    }
    return 0;
}

int lcec_pin_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap) {
    const lcec_pindesc_t *p;
    (void)ap;
    if (!base || !list) return 0;
    for (p = list; p->type != HAL_TYPE_UNSPECIFIED; p++) {
        void **data_ptr_addr = (void **)((char *)base + p->offset);
        *data_ptr_addr = calloc(1, sizeof(hal_u64_t));
        if (!*data_ptr_addr) return -ENOMEM;
    }
    return 0;
}

int lcec_param_newfv(hal_type_t type, hal_pin_dir_t dir,
                     void *data_addr, const char *fmt, va_list ap) {
    (void)type; (void)dir; (void)data_addr; (void)fmt; (void)ap;
    return 0;
}

int lcec_param_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap) {
    (void)base; (void)list; (void)ap;
    return 0;
}

LCEC_CONF_MODPARAM_VAL_T *lcec_modparam_get(struct lcec_slave *slave, int id) {
    (void)slave; (void)id;
    return NULL;
}

lcec_slave_t *lcec_slave_by_index(struct lcec_master *master, int index) {
    (void)master; (void)index;
    return NULL;
}

void copy_fsoe_data(struct lcec_slave *slave,
                    unsigned int slave_offset,
                    unsigned int master_offset) {
    (void)slave; (void)slave_offset; (void)master_offset;
}

/* =========================================================================
 * lcec sync helpers (declared in src/lcec.h, normally in src/slave.c)
 * ========================================================================= */

void lcec_syncs_init(lcec_syncs_t *syncs) {
    if (syncs) memset(syncs, 0, sizeof(*syncs));
}

void lcec_syncs_add_sync(lcec_syncs_t *syncs,
                         ec_direction_t dir,
                         ec_watchdog_mode_t watchdog_mode) {
    (void)syncs; (void)dir; (void)watchdog_mode;
}

void lcec_syncs_add_pdo_info(lcec_syncs_t *syncs, uint16_t index) {
    (void)syncs; (void)index;
}

void lcec_syncs_add_pdo_entry(lcec_syncs_t *syncs,
                               uint16_t index, uint8_t subindex,
                               uint8_t bit_length) {
    (void)syncs; (void)index; (void)subindex; (void)bit_length;
}

/* =========================================================================
 * PDO-count test harness
 * ========================================================================= */

/* Maximum PDO entries any single driver can register — sized generously */
#define PDO_BUF_SIZE 512

int main(void) {
    const lcec_typelist_t *t;
    int pass = 0, fail = 0, skip = 0;

    for (t = typelist; t->type != lcecSlaveTypeInvalid; t++) {
        lcec_master_t     master;
        lcec_slave_t      slave;
        ec_pdo_entry_reg_t pdo_buf[PDO_BUF_SIZE];
        ec_pdo_entry_reg_t *pdo_ptr;
        int actual, expected, rc;

        /* Skip bus couplers and devices with no proc_init */
        if (t->proc_init == NULL) {
            printf("SKIP  type=%-3d (no proc_init)\n", (int)t->type);
            skip++;
            continue;
        }

        /* Skip FSoE logic slaves — they require cross-slave modparam setup
         * (e.g. EL6900, EL1918_LOGIC) and cannot be tested in isolation. */
        if (t->is_fsoe_logic) {
            printf("SKIP  type=%-3d (is_fsoe_logic)\n", (int)t->type);
            skip++;
            continue;
        }

        /* Initialise a minimal zeroed master and slave */
        memset(&master, 0, sizeof(master));
        memset(&slave,  0, sizeof(slave));

        slave.master          = &master;
        slave.vid             = t->vid;
        slave.pid             = t->pid;
        slave.pdo_entry_count = t->pdo_entry_count;
        slave.proc_preinit    = t->proc_preinit;
        slave.proc_init       = t->proc_init;
        slave.is_fsoe_logic   = t->is_fsoe_logic;

        /* Run proc_preinit if present — it may update pdo_entry_count */
        if (slave.proc_preinit != NULL) {
            rc = slave.proc_preinit(&slave);
            if (rc != 0) {
                /* preinit failure usually means slave needs a neighbour
                 * (e.g. AX5805 needs a preceding AX5xxx) — skip gracefully */
                printf("SKIP  type=%-3d (preinit returned %d)\n",
                       (int)t->type, rc);
                skip++;
                continue;
            }
        }

        /* Prepare the PDO buffer */
        memset(pdo_buf, 0, sizeof(pdo_buf));
        pdo_ptr = pdo_buf;

        /* Call the real proc_init — it uses LCEC_PDO_INIT to populate
         * entries and advance *pdo_entry_regs */
        rc = slave.proc_init(comp_id, &slave, &pdo_ptr);
        if (rc != 0) {
            printf("FAIL  type=%-3d (proc_init returned %d)\n",
                   (int)t->type, rc);
            fail++;
            continue;
        }

        actual   = (int)(pdo_ptr - pdo_buf);
        expected = slave.pdo_entry_count;

        if (actual == expected) {
            printf("PASS  type=%-3d  pdo_count=%d\n", (int)t->type, actual);
            pass++;
        } else {
            printf("FAIL  type=%-3d  expected=%d  actual=%d\n",
                   (int)t->type, expected, actual);
            fail++;
        }
    }

    printf("\nResults: %d PASS, %d FAIL, %d SKIP\n", pass, fail, skip);
    return fail > 0 ? 1 : 0;
}
