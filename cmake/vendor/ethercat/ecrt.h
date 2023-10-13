/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT master userspace library.
 *
 *  The IgH EtherCAT master userspace library is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
 *
 *  The IgH EtherCAT master userspace library is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with the IgH EtherCAT master userspace library. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/** \file
 *
 * EtherCAT master application interface.
 *
 * \defgroup ApplicationInterface EtherCAT Application Interface
 *
 * EtherCAT interface for realtime applications. This interface is designed
 * for realtime modules that want to use EtherCAT. There are functions to
 * request a master, to map process data, to communicate with slaves via CoE
 * and to configure and activate the bus.
 *
 * Changes in version 1.5.2:
 *
 * - Added redundancy_active flag to ec_domain_state_t.
 * - Added ecrt_master_link_state() method and ec_master_link_state_t to query
 *   the state of a redundant link.
 * - Added the EC_HAVE_REDUNDANCY define, to check, if the interface contains
 *   redundancy features.
 * - Added ecrt_sdo_request_index() to change SDO index and subindex after
 *   handler creation.
 * - Added interface for retrieving CoE emergency messages, i. e.
 *   ecrt_slave_config_emerg_size(), ecrt_slave_config_emerg_pop(),
 *   ecrt_slave_config_emerg_clear(), ecrt_slave_config_emerg_overruns() and
 *   the defines EC_HAVE_EMERGENCY and EC_COE_EMERGENCY_MSG_SIZE.
 * - Added interface for direct EtherCAT register access: Added data type
 *   ec_reg_request_t and methods ecrt_slave_config_create_reg_request(),
 *   ecrt_reg_request_data(), ecrt_reg_request_state(),
 *   ecrt_reg_request_write(), ecrt_reg_request_read() and the feature flag
 *   EC_HAVE_REG_ACCESS.
 * - Added method to select the reference clock,
 *   ecrt_master_select_reference_clock() and the feature flag
 *   EC_HAVE_SELECT_REF_CLOCK to check, if the method is available.
 * - Added method to get the reference clock time,
 *   ecrt_master_reference_clock_time() and the feature flag
 *   EC_HAVE_REF_CLOCK_TIME to have the possibility to synchronize the master
 *   clock to the reference clock.
 * - Changed the data types of the shift times in ecrt_slave_config_dc() to
 *   int32_t to correctly display negative shift times.
 * - Added ecrt_slave_config_reg_pdo_entry_pos() and the feature flag
 *   EC_HAVE_REG_BY_POS for registering PDO entries with non-unique indices
 *   via their positions in the mapping.
 *
 * Changes in version 1.5:
 *
 * - Added the distributed clocks feature and the respective method
 *   ecrt_slave_config_dc() to configure a slave for cyclic operation, and
 *   ecrt_master_application_time(), ecrt_master_sync_reference_clock() and
 *   ecrt_master_sync_slave_clocks() for offset and drift compensation. The
 *   EC_TIMEVAL2NANO() macro can be used for epoch time conversion, while the
 *   ecrt_master_sync_monitor_queue() and ecrt_master_sync_monitor_process()
 *   methods can be used to monitor the synchrony.
 * - Improved the callback mechanism. ecrt_master_callbacks() now takes two
 *   callback functions for sending and receiving datagrams.
 *   ecrt_master_send_ext() is used to execute the sending of non-application
 *   datagrams.
 * - Added watchdog configuration (method ecrt_slave_config_watchdog(),
 *   #ec_watchdog_mode_t, \a watchdog_mode parameter in ec_sync_info_t and
 *   ecrt_slave_config_sync_manager()).
 * - Added ecrt_slave_config_complete_sdo() method to download an SDO during
 *   configuration via CompleteAccess.
 * - Added ecrt_master_deactivate() to remove the bus configuration.
 * - Added ecrt_open_master() and ecrt_master_reserve() separation for
 *   userspace.
 * - Added bus information interface (methods ecrt_master(),
 *   ecrt_master_get_slave(), ecrt_master_get_sync_manager(),
 *   ecrt_master_get_pdo() and ecrt_master_get_pdo_entry()) to get information
 *   about the currently connected slaves and the PDO entries provided.
 * - Added ecrt_master_sdo_download(), ecrt_master_sdo_download_complete() and
 *   ecrt_master_sdo_upload() methods to let an application transfer SDOs
 *   before activating the master.
 * - Changed the meaning of the negative return values of
 *   ecrt_slave_config_reg_pdo_entry() and ecrt_slave_config_sdo*().
 * - Implemented the Vendor-specific over EtherCAT mailbox protocol. See
 *   ecrt_slave_config_create_voe_handler().
 * - Renamed ec_sdo_request_state_t to #ec_request_state_t, because it is also
 *   used by VoE handlers.
 * - Removed 'const' from argument of ecrt_sdo_request_state(), because the
 *   userspace library has to modify object internals.
 * - Added 64-bit data access macros.
 * - Added ecrt_slave_config_idn() method for storing SoE IDN configurations,
 *   and ecrt_master_read_idn() and ecrt_master_write_idn() to read/write IDNs
 *   ad-hoc via the user-space library.
 * - Added ecrt_master_reset() to initiate retrying to configure slaves.
 *
 * @{
 */

/*****************************************************************************/

#ifndef __ECRT_H__
#define __ECRT_H__

#ifdef __KERNEL__
#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/time.h>
#else
#include <stdlib.h> // for size_t
#include <stdint.h>
#include <sys/time.h> // for struct timeval
#endif

/******************************************************************************
 * Global definitions
 *****************************************************************************/

/** EtherCAT realtime interface major version number.
 */
#define ECRT_VER_MAJOR 1

/** EtherCAT realtime interface minor version number.
 */
#define ECRT_VER_MINOR 5

/** EtherCAT realtime interface patchlevel number.
 */
#define ECRT_VER_PATCH 10

/** EtherCAT realtime interface version word generator.
 */
#define ECRT_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))

/** EtherCAT realtime interface version word.
 */
#define ECRT_VERSION_MAGIC ECRT_VERSION(ECRT_VER_MAJOR, ECRT_VER_MINOR, ECRT_VER_PATCH)

/******************************************************************************
 * Feature flags
 *****************************************************************************/

/** Defined, if the redundancy features are available.
 *
 * I. e. if the \a redundancy_active flag in ec_domain_state_t and the
 * ecrt_master_link_state() method are available.
 */
#define EC_HAVE_REDUNDANCY

/** Defined, if the CoE emergency ring feature is available.
 *
 * I. e. if the ecrt_slave_config_emerg_*() methods are available.
 */
#define EC_HAVE_EMERGENCY

/** Defined, if the register access interface is available.
 *
 * I. e. if the methods ecrt_slave_config_create_reg_request(),
 * ecrt_reg_request_data(), ecrt_reg_request_state(), ecrt_reg_request_write()
 * and ecrt_reg_request_read() are available.
 */
#define EC_HAVE_REG_ACCESS

/** Defined if the method ecrt_master_select_reference_clock() is available.
 */
#define EC_HAVE_SELECT_REF_CLOCK

/** Defined if the method ecrt_master_reference_clock_time() is available.
 */
#define EC_HAVE_REF_CLOCK_TIME

/** Defined if the method ecrt_slave_config_reg_pdo_entry_pos() is available.
 */
#define EC_HAVE_REG_BY_POS

/** Defined if the method ecrt_master_sync_reference_clock_to() is available.
 */
#define EC_HAVE_SYNC_TO

/*****************************************************************************/

/** End of list marker.
 *
 * This can be used with ecrt_slave_config_pdos().
 */
#define EC_END ~0U

/** Maximum number of sync managers per slave.
 */
#define EC_MAX_SYNC_MANAGERS 16

/** Maximum string length.
 *
 * Used in ec_slave_info_t.
 */
#define EC_MAX_STRING_LENGTH 64

/** Maximum number of slave ports. */
#define EC_MAX_PORTS 4

/** Timeval to nanoseconds conversion.
 *
 * This macro converts a Unix epoch time to EtherCAT DC time.
 *
 * \see void ecrt_master_application_time()
 *
 * \param TV struct timeval containing epoch time.
 */
#define EC_TIMEVAL2NANO(TV) \
    (((TV).tv_sec - 946684800ULL) * 1000000000ULL + (TV).tv_usec * 1000ULL)

/** Size of a CoE emergency message in byte.
 *
 * \see ecrt_slave_config_emerg_pop().
 */
#define EC_COE_EMERGENCY_MSG_SIZE 8

/******************************************************************************
 * Data types
 *****************************************************************************/

struct ec_master;
typedef struct ec_master ec_master_t; /**< \see ec_master */

struct ec_slave_config;
typedef struct ec_slave_config ec_slave_config_t; /**< \see ec_slave_config */

struct ec_domain;
typedef struct ec_domain ec_domain_t; /**< \see ec_domain */

struct ec_sdo_request;
typedef struct ec_sdo_request ec_sdo_request_t; /**< \see ec_sdo_request. */

struct ec_foe_request;
typedef struct ec_foe_request ec_foe_request_t; /**< \see ec_foe_request. */

struct ec_voe_handler;
typedef struct ec_voe_handler ec_voe_handler_t; /**< \see ec_voe_handler. */

struct ec_reg_request;
typedef struct ec_reg_request ec_reg_request_t; /**< \see ec_reg_request. */

/*****************************************************************************/

/** Master state.
 *
 * This is used for the output parameter of ecrt_master_state().
 *
 * \see ecrt_master_state().
 */
typedef struct {
    unsigned int slaves_responding; /**< Sum of responding slaves on all
                                      Ethernet devices. */
    unsigned int al_states : 4; /**< Application-layer states of all slaves.
                                  The states are coded in the lower 4 bits.
                                  If a bit is set, it means that at least one
                                  slave in the bus is in the corresponding
                                  state:
                                  - Bit 0: \a INIT
                                  - Bit 1: \a PREOP
                                  - Bit 2: \a SAFEOP
                                  - Bit 3: \a OP */
    unsigned int link_up : 1; /**< \a true, if at least one Ethernet link is
                                up. */
    unsigned int scan_busy : 1; /**< \a true, if a slave rescan is in progress */
} ec_master_state_t;

/*****************************************************************************/

/** Redundant link state.
 *
 * This is used for the output parameter of ecrt_master_link_state().
 *
 * \see ecrt_master_link_state().
 */
typedef struct {
    unsigned int slaves_responding; /**< Sum of responding slaves on the given
                                      link. */
    unsigned int al_states : 4; /**< Application-layer states of the slaves on
                                  the given link.  The states are coded in the
                                  lower 4 bits.  If a bit is set, it means
                                  that at least one slave in the bus is in the
                                  corresponding state:
                                  - Bit 0: \a INIT
                                  - Bit 1: \a PREOP
                                  - Bit 2: \a SAFEOP
                                  - Bit 3: \a OP */
    unsigned int link_up : 1; /**< \a true, if the given Ethernet link is up.
                               */
} ec_master_link_state_t;

/*****************************************************************************/

/** Slave configuration state.
 *
 * This is used as an output parameter of ecrt_slave_config_state().
 *
 * \see ecrt_slave_config_state().
 */
typedef struct  {
    unsigned int online : 1; /**< The slave is online. */
    unsigned int operational : 1; /**< The slave was brought into \a OP state
                                    using the specified configuration. */
    unsigned int al_state : 4; /**< The application-layer state of the slave.
                                 - 1: \a INIT
                                 - 2: \a PREOP
                                 - 4: \a SAFEOP
                                 - 8: \a OP

                                 Note that each state is coded in a different
                                 bit! */
    unsigned int error_flag : 1; /**< The slave has an unrecoverable error. */
    unsigned int ready : 1; /**< The slave is ready for external requests. */
    uint16_t position; /**< Offset of the slave in the ring. */
} ec_slave_config_state_t;

/*****************************************************************************/

/** Master information.
 *
 * This is used as an output parameter of ecrt_master().
 *
 * \see ecrt_master().
 */
typedef struct {
   unsigned int slave_count; /**< Number of slaves in the bus. */
   unsigned int link_up : 1; /**< \a true, if the network link is up. */
   uint8_t scan_busy; /**< \a true, while the master is scanning the bus */
   uint64_t app_time; /**< Application time. */
} ec_master_info_t;

/*****************************************************************************/

/** EtherCAT slave port descriptor.
 */
typedef enum {
    EC_PORT_NOT_IMPLEMENTED, /**< Port is not implemented. */
    EC_PORT_NOT_CONFIGURED, /**< Port is not configured. */
    EC_PORT_EBUS, /**< Port is an E-Bus. */
    EC_PORT_MII /**< Port is a MII. */
} ec_slave_port_desc_t;

/*****************************************************************************/

/** EtherCAT slave port information.
 */
typedef struct {
    uint8_t link_up; /**< Link detected. */
    uint8_t loop_closed; /**< Loop closed. */
    uint8_t signal_detected; /**< Detected signal on RX port. */
    uint8_t bypassed; /**< Packets are bypassing this port (eg. redundancy) */
} ec_slave_port_link_t;

/*****************************************************************************/

/** Slave information.
 *
 * This is used as an output parameter of ecrt_master_get_slave().
 *
 * \see ecrt_master_get_slave().
 */
typedef struct {
    uint16_t position; /**< Offset of the slave in the ring. */
    uint32_t vendor_id; /**< Vendor-ID stored on the slave. */
    uint32_t product_code; /**< Product-Code stored on the slave. */
    uint32_t revision_number; /**< Revision-Number stored on the slave. */
    uint32_t serial_number; /**< Serial-Number stored on the slave. */
    uint16_t alias; /**< The slaves alias if not equal to 0. */
    int16_t current_on_ebus; /**< Used current in mA. */
    struct {
        ec_slave_port_desc_t desc; /**< Physical port type. */
        ec_slave_port_link_t link; /**< Port link state. */
        uint32_t receive_time; /**< Receive time on DC transmission delay
                                 measurement. */
        uint16_t next_slave; /**< Ring position of next DC slave on that
                               port.  */
        uint32_t delay_to_next_dc; /**< Delay [ns] to next DC slave. */
    } ports[EC_MAX_PORTS]; /**< Port information. */
    uint8_t upstream_port; /**< Index of upstream (master facing) port */
    uint8_t al_state; /**< Current state of the slave. */
    uint8_t error_flag; /**< Error flag for that slave. */
    uint8_t scan_required; /**< The slave is being scanned. */
    uint8_t ready; /**< The slave is ready for external requests. */
    uint8_t sync_count; /**< Number of sync managers. */
    uint16_t sdo_count; /**< Number of SDOs. */
    char name[EC_MAX_STRING_LENGTH]; /**< Name of the slave. */
} ec_slave_info_t;

/*****************************************************************************/

/** Domain working counter interpretation.
 *
 * This is used in ec_domain_state_t.
 */
typedef enum {
    EC_WC_ZERO = 0,   /**< No registered process data were exchanged. */
    EC_WC_INCOMPLETE, /**< Some of the registered process data were
                        exchanged. */
    EC_WC_COMPLETE    /**< All registered process data were exchanged. */
} ec_wc_state_t;

/*****************************************************************************/

/** Domain state.
 *
 * This is used for the output parameter of ecrt_domain_state().
 */
typedef struct {
    unsigned int working_counter; /**< Value of the last working counter. */
    ec_wc_state_t wc_state; /**< Working counter interpretation. */
    unsigned int redundancy_active; /**< Redundant link is in use. */
} ec_domain_state_t;

/*****************************************************************************/

/** Direction type for PDO assignment functions.
 */
typedef enum {
    EC_DIR_INVALID, /**< Invalid direction. Do not use this value. */
    EC_DIR_OUTPUT, /**< Values written by the master. */
    EC_DIR_INPUT, /**< Values read by the master. */
    EC_DIR_BOTH, /**< Values read and written by the master. */
    EC_DIR_COUNT /**< Number of directions. For internal use only. */
} ec_direction_t;

/*****************************************************************************/

/** Watchdog mode for sync manager configuration.
 *
 * Used to specify, if a sync manager's watchdog is to be enabled.
 */
typedef enum {
    EC_WD_DEFAULT, /**< Use the default setting of the sync manager. */
    EC_WD_ENABLE, /**< Enable the watchdog. */
    EC_WD_DISABLE, /**< Disable the watchdog. */
} ec_watchdog_mode_t;

/*****************************************************************************/

/** PDO entry configuration information.
 *
 * This is the data type of the \a entries field in ec_pdo_info_t.
 *
 * \see ecrt_slave_config_pdos().
 */
typedef struct {
    uint16_t index; /**< PDO entry index. */
    uint8_t subindex; /**< PDO entry subindex. */
    uint8_t bit_length; /**< Size of the PDO entry in bit. */
} ec_pdo_entry_info_t;

/*****************************************************************************/

/** PDO configuration information.
 *
 * This is the data type of the \a pdos field in ec_sync_info_t.
 *
 * \see ecrt_slave_config_pdos().
 */
typedef struct {
    uint16_t index; /**< PDO index. */
    unsigned int n_entries; /**< Number of PDO entries in \a entries to map.
                              Zero means, that the default mapping shall be
                              used (this can only be done if the slave is
                              present at bus configuration time). */
    ec_pdo_entry_info_t *entries; /**< Array of PDO entries to map. Can either
                                    be \a NULL, or must contain at
                                    least \a n_entries values. */
} ec_pdo_info_t;

/*****************************************************************************/

/** Sync manager configuration information.
 *
 * This can be use to configure multiple sync managers including the PDO
 * assignment and PDO mapping. It is used as an input parameter type in
 * ecrt_slave_config_pdos().
 */
typedef struct {
    uint8_t index; /**< Sync manager index. Must be less
                     than #EC_MAX_SYNC_MANAGERS for a valid sync manager,
                     but can also be \a 0xff to mark the end of the list. */
    ec_direction_t dir; /**< Sync manager direction. */
    unsigned int n_pdos; /**< Number of PDOs in \a pdos. */
    ec_pdo_info_t *pdos; /**< Array with PDOs to assign. This must contain
                            at least \a n_pdos PDOs. */
    ec_watchdog_mode_t watchdog_mode; /**< Watchdog mode. */
} ec_sync_info_t;

/*****************************************************************************/

/** List record type for PDO entry mass-registration.
 *
 * This type is used for the array parameter of the
 * ecrt_domain_reg_pdo_entry_list()
 */
typedef struct {
    uint16_t alias; /**< Slave alias address. */
    uint16_t position; /**< Slave position. */
    uint32_t vendor_id; /**< Slave vendor ID. */
    uint32_t product_code; /**< Slave product code. */
    uint16_t index; /**< PDO entry index. */
    uint8_t subindex; /**< PDO entry subindex. */
    unsigned int *offset; /**< Pointer to a variable to store the PDO entry's
                       (byte-)offset in the process data. */
    unsigned int *bit_position; /**< Pointer to a variable to store a bit
                                  position (0-7) within the \a offset. Can be
                                  NULL, in which case an error is raised if the
                                  PDO entry does not byte-align. */
} ec_pdo_entry_reg_t;

/*****************************************************************************/

/** Request state.
 *
 * This is used as return type for ecrt_sdo_request_state() and
 * ecrt_voe_handler_state().
 */
typedef enum {
    EC_REQUEST_UNUSED, /**< Not requested. */
    EC_REQUEST_BUSY, /**< Request is being processed. */
    EC_REQUEST_SUCCESS, /**< Request was processed successfully. */
    EC_REQUEST_ERROR, /**< Request processing failed. */
} ec_request_state_t;

/*****************************************************************************/

/** FoE error enumeration type.
 */
typedef enum {
    FOE_BUSY               = 0, /**< Busy. */
    FOE_READY              = 1, /**< Ready. */
    FOE_IDLE               = 2, /**< Idle. */
    FOE_WC_ERROR           = 3, /**< Working counter error. */
    FOE_RECEIVE_ERROR      = 4, /**< Receive error. */
    FOE_PROT_ERROR         = 5, /**< Protocol error. */
    FOE_NODATA_ERROR       = 6, /**< No data error. */
    FOE_PACKETNO_ERROR     = 7, /**< Packet number error. */
    FOE_OPCODE_ERROR       = 8, /**< OpCode error. */
    FOE_TIMEOUT_ERROR      = 9, /**< Timeout error. */
    FOE_SEND_RX_DATA_ERROR = 10, /**< Error sending received data. */
    FOE_RX_DATA_ACK_ERROR  = 11, /**< Error acknowledging received data. */
    FOE_ACK_ERROR          = 12, /**< Acknowledge error. */
    FOE_MBOX_FETCH_ERROR   = 13, /**< Error fetching data from mailbox. */
    FOE_READ_NODATA_ERROR  = 14, /**< No data while reading. */
    FOE_MBOX_PROT_ERROR    = 15, /**< Mailbox protocol error. */
    FOE_READ_OVER_ERROR    = 16, /**< Read buffer overflow. */
} ec_foe_error_t;

/*****************************************************************************/

/** Application-layer state.
 */
typedef enum {
    EC_AL_STATE_INIT = 1, /**< Init. */
    EC_AL_STATE_PREOP = 2, /**< Pre-operational. */
    EC_AL_STATE_SAFEOP = 4, /**< Safe-operational. */
    EC_AL_STATE_OP = 8, /**< Operational. */
} ec_al_state_t;

/******************************************************************************
 * Global functions
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/** Returns the version magic of the realtime interface.
 *
 * \return Value of ECRT_VERSION_MAGIC() at EtherCAT master compile time.
 */
unsigned int ecrt_version_magic(void);

/** Requests an EtherCAT master for realtime operation.
 *
 * Before an application can access an EtherCAT master, it has to reserve one
 * for exclusive use.
 *
 * In userspace, this is a convenience function for ecrt_open_master() and
 * ecrt_master_reserve().
 *
 * This function has to be the first function an application has to call to
 * use EtherCAT. The function takes the index of the master as its argument.
 * The first master has index 0, the n-th master has index n - 1. The number
 * of masters has to be specified when loading the master module.
 *
 * \return Pointer to the reserved master, otherwise \a NULL.
 */
ec_master_t *ecrt_request_master(
        unsigned int master_index /**< Index of the master to request. */
        );

#ifndef __KERNEL__

/** Opens an EtherCAT master for userspace access.
 *
 * This function has to be the first function an application has to call to
 * use EtherCAT. The function takes the index of the master as its argument.
 * The first master has index 0, the n-th master has index n - 1. The number
 * of masters has to be specified when loading the master module.
 *
 * For convenience, the function ecrt_request_master() can be used.
 *
 * \return Pointer to the opened master, otherwise \a NULL.
 */
ec_master_t *ecrt_open_master(
        unsigned int master_index /**< Index of the master to request. */
        );

#endif // #ifndef __KERNEL__

/** Releases a requested EtherCAT master.
 *
 * After use, a master it has to be released to make it available for other
 * applications.
 *
 * This method frees all created data structures. It should not be called in
 * realtime context.
 *
 * If the master was activated, ecrt_master_deactivate() is called internally.
 */
void ecrt_release_master(
        ec_master_t *master /**< EtherCAT master */
        );

/******************************************************************************
 * Master methods
 *****************************************************************************/

#ifndef __KERNEL__

/** Reserves an EtherCAT master for realtime operation.
 *
 * Before an application can use PDO/domain registration functions or SDO
 * request functions on the master, it has to reserve one for exclusive use.
 *
 * \return 0 in case of success, else < 0
 */
int ecrt_master_reserve(
        ec_master_t *master /**< EtherCAT master */
        );

#endif // #ifndef __KERNEL__

#ifdef __KERNEL__

/** Sets the locking callbacks.
 *
 * For concurrent master access, i. e. if other instances than the application
 * want to send and receive datagrams on the bus, the application has to
 * provide a callback mechanism. This method takes two function pointers as
 * its parameters. Asynchronous master access (like EoE processing) is only
 * possible if the callbacks have been set.
 *
 * The task of the send callback (\a send_cb) is to decide, if the bus is
 * currently accessible and whether or not to call the ecrt_master_send_ext()
 * method.
 *
 * The task of the receive callback (\a receive_cb) is to decide, if a call to
 * ecrt_master_receive() is allowed and to execute it respectively.
 *
 * \attention This method has to be called before ecrt_master_activate().
 */
void ecrt_master_callbacks(
        ec_master_t *master, /**< EtherCAT master */
        void (*send_cb)(void *), /**< Datagram sending callback. */
        void (*receive_cb)(void *), /**< Receive callback. */
        void *cb_data /**< Arbitrary pointer passed to the callback functions.
                       */
        );

#endif /* __KERNEL__ */

/** Creates a new process data domain.
 *
 * For process data exchange, at least one process data domain is needed.
 * This method creates a new process data domain and returns a pointer to the
 * new domain object. This object can be used for registering PDOs and
 * exchanging them in cyclic operation.
 *
 * This method allocates memory and should be called in non-realtime context
 * before ecrt_master_activate().
 *
 * \return Pointer to the new domain on success, else NULL.
 */
ec_domain_t *ecrt_master_create_domain(
        ec_master_t *master /**< EtherCAT master. */
        );

/** setup the domain's process data memory.
 *
 * Call this after all PDO entries have been registered and before activating
 * the master.
 *
 * Call this if you need to access the domain memory before activating the
 * master
 *
 * \return 0 on success, else non-zero.
 */
int ecrt_master_setup_domain_memory(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Obtains a slave configuration.
 *
 * Creates a slave configuration object for the given \a alias and \a position
 * tuple and returns it. If a configuration with the same \a alias and \a
 * position already exists, it will be re-used. In the latter case, the given
 * vendor ID and product code are compared to the stored ones. On mismatch, an
 * error message is raised and the function returns \a NULL.
 *
 * Slaves are addressed with the \a alias and \a position parameters.
 * - If \a alias is zero, \a position is interpreted as the desired slave's
 *   ring position.
 * - If \a alias is non-zero, it matches a slave with the given alias. In this
 *   case, \a position is interpreted as ring offset, starting from the
 *   aliased slave, so a position of zero means the aliased slave itself and a
 *   positive value matches the n-th slave behind the aliased one.
 *
 * If the slave with the given address is found during the bus configuration,
 * its vendor ID and product code are matched against the given value. On
 * mismatch, the slave is not configured and an error message is raised.
 *
 * If different slave configurations are pointing to the same slave during bus
 * configuration, a warning is raised and only the first configuration is
 * applied.
 *
 * This method allocates memory and should be called in non-realtime context
 * before ecrt_master_activate().
 *
 * \retval >0 Pointer to the slave configuration structure.
 * \retval NULL in the error case.
 */
ec_slave_config_t *ecrt_master_slave_config(
        ec_master_t *master, /**< EtherCAT master */
        uint16_t alias, /**< Slave alias. */
        uint16_t position, /**< Slave position. */
        uint32_t vendor_id, /**< Expected vendor ID. */
        uint32_t product_code /**< Expected product code. */
        );

/** Selects the reference clock for distributed clocks.
 *
 * If this method is not called for a certain master, or if the slave
 * configuration pointer is NULL, then the first slave with DC functionality
 * will provide the reference clock.
 *
 * \return 0 on success, otherwise negative error code.
 */
int ecrt_master_select_reference_clock(
        ec_master_t *master, /**< EtherCAT master. */
        ec_slave_config_t *sc /**< Slave config of the slave to use as the
                               * reference slave (or NULL). */
        );

/** Obtains master information.
 *
 * No memory is allocated on the heap in
 * this function.
 *
 * \attention The pointer to this structure must point to a valid variable.
 *
 * \return 0 in case of success, else < 0
 */
int ecrt_master(
        ec_master_t *master, /**< EtherCAT master */
        ec_master_info_t *master_info /**< Structure that will output the
                                        information */
        );

/** Obtains slave information.
 *
 * Tries to find the slave with the given ring position. The obtained
 * information is stored in a structure. No memory is allocated on the heap in
 * this function.
 *
 * \attention The pointer to this structure must point to a valid variable.
 *
 * \return 0 in case of success, else < 0
 */
int ecrt_master_get_slave(
        ec_master_t *master, /**< EtherCAT master */
        uint16_t slave_position, /**< Slave position. */
        ec_slave_info_t *slave_info /**< Structure that will output the
                                      information */
        );

#ifndef __KERNEL__

/** Returns the proposed configuration of a slave's sync manager.
 *
 * Fills a given ec_sync_info_t structure with the attributes of a sync
 * manager. The \a pdos field of the return value is left empty. Use
 * ecrt_master_get_pdo() to get the PDO information.
 *
 * \return zero on success, else non-zero
 */
int ecrt_master_get_sync_manager(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint8_t sync_index, /**< Sync manager index. Must be less
                                than #EC_MAX_SYNC_MANAGERS. */
        ec_sync_info_t *sync /**< Pointer to output structure. */
        );

/** Returns information about a currently assigned PDO.
 *
 * Fills a given ec_pdo_info_t structure with the attributes of a currently
 * assigned PDO of the given sync manager. The \a entries field of the return
 * value is left empty. Use ecrt_master_get_pdo_entry() to get the PDO
 * entry information.
 *
 * \retval zero on success, else non-zero
 */
int ecrt_master_get_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint8_t sync_index, /**< Sync manager index. Must be less
                                 than #EC_MAX_SYNC_MANAGERS. */
        uint16_t pos, /**< Zero-based PDO position. */
        ec_pdo_info_t *pdo /**< Pointer to output structure. */
        );

/** Returns information about a currently mapped PDO entry.
 *
 * Fills a given ec_pdo_entry_info_t structure with the attributes of a
 * currently mapped PDO entry of the given PDO.
 *
 * \retval zero on success, else non-zero
 */
int ecrt_master_get_pdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint8_t sync_index, /**< Sync manager index. Must be less
                                 than #EC_MAX_SYNC_MANAGERS. */
        uint16_t pdo_pos, /**< Zero-based PDO position. */
        uint16_t entry_pos, /**< Zero-based PDO entry position. */
        ec_pdo_entry_info_t *entry /**< Pointer to output structure. */
        );

#endif /* #ifndef __KERNEL__ */

/** Executes an SDO download request to write data to a slave.
 *
 * This request is processed by the master state machine. This method blocks,
 * until the request has been processed and may not be called in realtime
 * context.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_master_sdo_download(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint16_t index, /**< Index of the SDO. */
        uint8_t subindex, /**< Subindex of the SDO. */
        const uint8_t *data, /**< Data buffer to download. */
        size_t data_size, /**< Size of the data buffer. */
        uint32_t *abort_code /**< Abort code of the SDO download. */
        );

/** Executes an SDO download request to write data to a slave via complete
 * access.
 *
 * This request is processed by the master state machine. This method blocks,
 * until the request has been processed and may not be called in realtime
 * context.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_master_sdo_download_complete(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint16_t index, /**< Index of the SDO. */
        const uint8_t *data, /**< Data buffer to download. */
        size_t data_size, /**< Size of the data buffer. */
        uint32_t *abort_code /**< Abort code of the SDO download. */
        );

/** Executes an SDO upload request to read data from a slave.
 *
 * This request is processed by the master state machine. This method blocks,
 * until the request has been processed and may not be called in realtime
 * context.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_master_sdo_upload(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint16_t index, /**< Index of the SDO. */
        uint8_t subindex, /**< Subindex of the SDO. */
        uint8_t *target, /**< Target buffer for the upload. */
        size_t target_size, /**< Size of the target buffer. */
        size_t *result_size, /**< Uploaded data size. */
        uint32_t *abort_code /**< Abort code of the SDO upload. */
        );

/** Executes an SDO upload request to read data from a slave via complete access.
 *
 * This request is processed by the master state machine. This method blocks,
 * until the request has been processed and may not be called in realtime
 * context.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_master_sdo_upload_complete(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint16_t index, /**< Index of the SDO. */
        uint8_t *target, /**< Target buffer for the upload. */
        size_t target_size, /**< Size of the target buffer. */
        size_t *result_size, /**< Uploaded data size. */
        uint32_t *abort_code /**< Abort code of the SDO upload. */
        );

/** Executes an SoE write request.
 *
 * Starts writing an IDN and blocks until the request was processed, or an
 * error occurred.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_master_write_idn(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint8_t drive_no, /**< Drive number. */
        uint16_t idn, /**< SoE IDN (see ecrt_slave_config_idn()). */
        uint8_t *data, /**< Pointer to data to write. */
        size_t data_size, /**< Size of data to write. */
        uint16_t *error_code /**< Pointer to variable, where an SoE error code
                               can be stored. */
        );

/** Executes an SoE read request.
 *
 * Starts reading an IDN and blocks until the request was processed, or an
 * error occurred.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_master_read_idn(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t slave_position, /**< Slave position. */
        uint8_t drive_no, /**< Drive number. */
        uint16_t idn, /**< SoE IDN (see ecrt_slave_config_idn()). */
        uint8_t *target, /**< Pointer to memory where the read data can be
                           stored. */
        size_t target_size, /**< Size of the memory \a target points to. */
        size_t *result_size, /**< Actual size of the received data. */
        uint16_t *error_code /**< Pointer to variable, where an SoE error code
                               can be stored. */
        );

/** Finishes the configuration phase and prepares for cyclic operation.
 *
 * This function tells the master that the configuration phase is finished and
 * the realtime operation will begin. The function allocates internal memory
 * for the domains and calculates the logical FMMU addresses for domain
 * members. It tells the master state machine that the bus configuration is
 * now to be applied.
 *
 * \attention After this function has been called, the realtime application is
 * in charge of cyclically calling ecrt_master_send() and
 * ecrt_master_receive() to ensure bus communication. Before calling this
 * function, the master thread is responsible for that, so these functions may
 * not be called! The method itself allocates memory and should not be called
 * in realtime context.
 *
 * \return 0 in case of success, else < 0
 */
int ecrt_master_activate(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Deactivates the slaves distributed clocks and sends the slaves into PREOP.
 *
 * This can be called prior to ecrt_master_deactivate to avoid the slaves
 * getting sync errors.
 *
 * This method should be called in realtime context.
 *
 * Note: EoE slaves will not be changed to PREOP.
 */
void ecrt_master_deactivate_slaves(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Deactivates the master.
 *
 * Removes the bus configuration. All objects created by
 * ecrt_master_create_domain(), ecrt_master_slave_config(), ecrt_domain_data()
 * ecrt_slave_config_create_sdo_request() and
 * ecrt_slave_config_create_voe_handler() are freed, so pointers to them
 * become invalid.
 *
 * This method should not be called in realtime context.
 */
void ecrt_master_deactivate(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Set interval between calls to ecrt_master_send().
 *
 * This information helps the master to decide, how much data can be appended
 * to a frame by the master state machine. When the master is configured with
 * --enable-hrtimers, this is used to calculate the scheduling of the master
 * thread.
 *
 * \retval 0 on success.
 * \retval <0 Error code.
 */
int ecrt_master_set_send_interval(
        ec_master_t *master, /**< EtherCAT master. */
        size_t send_interval /**< Send interval in us */
        );

/** Sends all datagrams in the queue.
 *
 * This method takes all datagrams, that have been queued for transmission,
 * puts them into frames, and passes them to the Ethernet device for sending.
 *
 * Has to be called cyclically by the application after ecrt_master_activate()
 * has returned.
 *
 * Returns the number of bytes sent.
 */
size_t ecrt_master_send(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Fetches received frames from the hardware and processes the datagrams.
 *
 * Queries the network device for received frames by calling the interrupt
 * service routine. Extracts received datagrams and dispatches the results to
 * the datagram objects in the queue. Received datagrams, and the ones that
 * timed out, will be marked, and dequeued.
 *
 * Has to be called cyclically by the realtime application after
 * ecrt_master_activate() has returned.
 */
void ecrt_master_receive(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Sends non-application datagrams.
 *
 * This method has to be called in the send callback function passed via
 * ecrt_master_callbacks() to allow the sending of non-application datagrams.
 *
 * Returns the number of bytes sent.
 */
size_t ecrt_master_send_ext(
        ec_master_t *master /**< EtherCAT master. */
        );

#if !defined(__KERNEL__) && defined(EC_RTDM) && (EC_EOE)

/** check if there are any open eoe handlers
 *
 * used by user space code to process EOE handlers
 *
 * \return 1 if any eoe handlers are open, zero if not,
 *   otherwise a negative error code.
 */
int ecrt_master_eoe_is_open(
        ec_master_t *master /**< EtherCAT master. */
        );

/** return flag from ecrt_master_eoe_process() to indicate there is
 * something to send.  if this flag is set call ecrt_master_send_ext()
 */
#define EOE_STH_TO_SEND 1

/** return flag from ecrt_master_eoe_process() to indicate there is
 * something still pending.  if this flag is set yield the process
 * before starting the cycle again quickly, else sleep for a short time
 * (e.g. 1ms)
 */

#define EOE_STH_PENDING 2

/** Check if any EOE handlers are open.
 *
 * used by user space code to process EOE handlers
 *
 * \return 1 if something to send +
 *   2 if an eoe handler has something still pending
 */
int ecrt_master_eoe_process(
        ec_master_t *master /**< EtherCAT master. */
        );
        
#endif /* !defined(__KERNEL__) && defined(EC_RTDM) && (EC_EOE) */

#ifdef EC_EOE

/** add an EOE network interface
 *
 * \return 0 on success else negative error code
 */
int ecrt_master_eoe_addif(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t alias, /**< slave alias. */
        uint16_t posn /**< slave position. */
        );
        
/** delete an EOE network interface
 *
 * \return 0 on success else negative error code
 */
int ecrt_master_eoe_delif(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t alias, /**< slave alias. */
        uint16_t posn /**< slave position. */
        );

#endif /* EC_EOE */

/** Reads the current master state.
 *
 * Stores the master state information in the given \a state structure.
 *
 * This method returns a global state. For the link-specific states in a
 * redundant bus topology, use the ecrt_master_link_state() method.
 */
void ecrt_master_state(
        const ec_master_t *master, /**< EtherCAT master. */
        ec_master_state_t *state /**< Structure to store the information. */
        );

/** Reads the current state of a redundant link.
 *
 * Stores the link state information in the given \a state structure.
 *
 * \return Zero on success, otherwise negative error code.
 */
int ecrt_master_link_state(
        const ec_master_t *master, /**< EtherCAT master. */
        unsigned int dev_idx, /**< Index of the device (0 = main device, 1 =
                                first backup device, ...). */
        ec_master_link_state_t *state /**< Structure to store the information.
                                       */
        );

/** Sets the application time.
 *
 * The master has to know the application's time when operating slaves with
 * distributed clocks. The time is not incremented by the master itself, so
 * this method has to be called cyclically.
 *
 * \attention The time passed to this method is used to calculate the phase of
 * the slaves' SYNC0/1 interrupts. It should be called constantly at the same
 * point of the realtime cycle. So it is recommended to call it at the start
 * of the calculations to avoid deviancies due to changing execution times.
 *
 * The time is used when setting the slaves' <tt>System Time Offset</tt> and
 * <tt>Cyclic Operation Start Time</tt> registers and when synchronizing the
 * DC reference clock to the application time via
 * ecrt_master_sync_reference_clock().
 *
 * The time is defined as nanoseconds from 2000-01-01 00:00. Converting an
 * epoch time can be done with the EC_TIMEVAL2NANO() macro, but is not
 * necessary, since the absolute value is not of any interest.
 */
void ecrt_master_application_time(
        ec_master_t *master, /**< EtherCAT master. */
        uint64_t app_time /**< Application time. */
        );

/** Queues the DC reference clock drift compensation datagram for sending.
 *
 * The reference clock will by synchronized to the application time provided
 * by the last call off ecrt_master_application_time().
 */
void ecrt_master_sync_reference_clock(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Queues the DC reference clock drift compensation datagram for sending.
 *
 * The reference clock will by synchronized to the time passed in the
 * sync_time parameter.
 */
void ecrt_master_sync_reference_clock_to(
        ec_master_t *master, /**< EtherCAT master. */
        uint64_t sync_time /**< Sync reference clock to this time. */
        );

/** Queues the DC clock drift compensation datagram for sending.
 *
 * All slave clocks synchronized to the reference clock.
 */
void ecrt_master_sync_slave_clocks(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Get the lower 32 bit of the reference clock system time.
 *
 * This method can be used to synchronize the master to the reference clock.
 *
 * The reference clock system time is queried via the
 * ecrt_master_sync_slave_clocks() method, that reads the system time of the
 * reference clock and writes it to the slave clocks (so be sure to call it
 * cyclically to get valid data).
 *
 * \attention The returned time is the system time of the reference clock
 * minus the transmission delay of the reference clock.
 *
 * \retval 0 success, system time was written into \a time.
 * \retval -ENXIO No reference clock found.
 * \retval -EIO Slave synchronization datagram was not received.
 */
int ecrt_master_reference_clock_time(
        ec_master_t *master, /**< EtherCAT master. */
        uint32_t *time /**< Pointer to store the queried system time. */
        );

/** Queues the 64bit dc reference slave clock time value datagram for sending.
 *
 * The datagram read the 64bit dc timestamp of the DC reference slave.
 * (register \a 0x0910:0x0917). The result can be checked with the 
 * ecrt_master_64bit_reference_clock_time() method.
 */
void ecrt_master_64bit_reference_clock_time_queue(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Get the 64bit dc reference slave clock time.
 * 
 * ecrt_master_64bit_reference_clock_time_queue() must be called in the cycle
 * prior to calling this method
 *
 * \attention The returned time is the system time of the reference clock
 * minus the transmission delay of the reference clock.
 *
 * \retval 0 success, system time was written into \a time.
 * \retval -ENXIO No reference clock found.
 * \retval -EIO Slave synchronization datagram was not received.
 */
int ecrt_master_64bit_reference_clock_time(
        ec_master_t *master, /**< EtherCAT master. */
        uint64_t *time /**< Pointer to store the queried time. */
        );

/** Queues the DC synchrony monitoring datagram for sending.
 *
 * The datagram broadcast-reads all "System time difference" registers (\a
 * 0x092c) to get an upper estimation of the DC synchrony. The result can be
 * checked with the ecrt_master_sync_monitor_process() method.
 */
void ecrt_master_sync_monitor_queue(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Processes the DC synchrony monitoring datagram.
 *
 * If the sync monitoring datagram was sent before with
 * ecrt_master_sync_monitor_queue(), the result can be queried with this
 * method.
 *
 * \return Upper estimation of the maximum time difference in ns.
 */
uint32_t ecrt_master_sync_monitor_process(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Selects whether to process slave requests by the application or the master
 *
 * if rt_slave_requests \a True, slave requests are to be handled by calls to 
 * ecrt_master_exec_requests() from the applications realtime context,
 * otherwise the master will handle them from its operation thread
 *
 * \return 0 on success, otherwise negative error code.
 */
int ecrt_master_rt_slave_requests(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned int rt_slave_requests /**< if \a True, slave requests are
                                       to be handled by calls to 
                                      ecrt_master_exec_requests() from
                                      the applications realtime context. */
        );

/** Explicit call to process slave requests.
 *
 * This needs to be called on a cyclical period by the applications
 * realtime context if ecrt_master_rt_slave_requests() has been called
 * with rt_slave_requests set to true.  If rt_slave_requests is \a False
 * (the default) slave requests will be processed within the master and
 * this call will be ignored.
 */
void ecrt_master_exec_slave_requests(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Retry configuring slaves.
 *
 * Via this method, the application can tell the master to bring all slaves to
 * OP state. In general, this is not necessary, because it is automatically
 * done by the master. But with special slaves, that can be reconfigured by
 * the vendor during runtime, it can be useful.
 */
void ecrt_master_reset(
        ec_master_t *master /**< EtherCAT master. */
        );

/******************************************************************************
 * Slave configuration methods
 *****************************************************************************/

/** Configure a sync manager.
 *
 * Sets the direction of a sync manager. This overrides the direction bits
 * from the default control register from SII.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_sync_manager(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t sync_index, /**< Sync manager index. Must be less
                              than #EC_MAX_SYNC_MANAGERS. */
        ec_direction_t direction, /**< Input/Output. */
        ec_watchdog_mode_t watchdog_mode /** Watchdog mode. */
        );

/** Configure a slave's watchdog times.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 */
void ecrt_slave_config_watchdog(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t watchdog_divider, /**< Number of 40 ns intervals. Used as a
                                     base unit for all slave watchdogs. If set
                                     to zero, the value is not written, so the
                                     default is used. */
        uint16_t watchdog_intervals /**< Number of base intervals for process
                                      data watchdog. If set to zero, the value
                                      is not written, so the default is used.
                                     */
        );

/** Configure whether a slave allows overlapping PDOs.
 *
 * Overlapping PDOs allows inputs to use the same space as outputs on the frame.
 * This reduces the frame length.
 */
void ecrt_slave_config_overlapping_pdos(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t allow_overlapping_pdos /**< Allow overlapping PDOs */
        );


/** Add a PDO to a sync manager's PDO assignment.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_pdos()
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdo_assign_add(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t sync_index, /**< Sync manager index. Must be less
                              than #EC_MAX_SYNC_MANAGERS. */
        uint16_t index /**< Index of the PDO to assign. */
        );

/** Clear a sync manager's PDO assignment.
 *
 * This can be called before assigning PDOs via
 * ecrt_slave_config_pdo_assign_add(), to clear the default assignment of a
 * sync manager.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_pdos()
 */
void ecrt_slave_config_pdo_assign_clear(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t sync_index /**< Sync manager index. Must be less
                              than #EC_MAX_SYNC_MANAGERS. */
        );

/** Add a PDO entry to the given PDO's mapping.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_pdos()
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdo_mapping_add(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t pdo_index, /**< Index of the PDO. */
        uint16_t entry_index, /**< Index of the PDO entry to add to the PDO's
                                mapping. */
        uint8_t entry_subindex, /**< Subindex of the PDO entry to add to the
                                  PDO's mapping. */
        uint8_t entry_bit_length /**< Size of the PDO entry in bit. */
        );

/** Clear the mapping of a given PDO.
 *
 * This can be called before mapping PDO entries via
 * ecrt_slave_config_pdo_mapping_add(), to clear the default mapping.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_pdos()
 */
void ecrt_slave_config_pdo_mapping_clear(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t pdo_index /**< Index of the PDO. */
        );

/** Specify a complete PDO configuration.
 *
 * This function is a convenience wrapper for the functions
 * ecrt_slave_config_sync_manager(), ecrt_slave_config_pdo_assign_clear(),
 * ecrt_slave_config_pdo_assign_add(), ecrt_slave_config_pdo_mapping_clear()
 * and ecrt_slave_config_pdo_mapping_add(), that are better suitable for
 * automatic code generation.
 *
 * The following example shows, how to specify a complete configuration,
 * including the PDO mappings. With this information, the master is able to
 * reserve the complete process data, even if the slave is not present at
 * configuration time:
 *
 * \code
 * ec_pdo_entry_info_t el3162_channel1[] = {
 *     {0x3101, 1,  8}, // status
 *     {0x3101, 2, 16}  // value
 * };
 *
 * ec_pdo_entry_info_t el3162_channel2[] = {
 *     {0x3102, 1,  8}, // status
 *     {0x3102, 2, 16}  // value
 * };
 *
 * ec_pdo_info_t el3162_pdos[] = {
 *     {0x1A00, 2, el3162_channel1},
 *     {0x1A01, 2, el3162_channel2}
 * };
 *
 * ec_sync_info_t el3162_syncs[] = {
 *     {2, EC_DIR_OUTPUT},
 *     {3, EC_DIR_INPUT, 2, el3162_pdos},
 *     {0xff}
 * };
 *
 * if (ecrt_slave_config_pdos(sc_ana_in, EC_END, el3162_syncs)) {
 *     // handle error
 * }
 * \endcode
 *
 * The next example shows, how to configure the PDO assignment only. The
 * entries for each assigned PDO are taken from the PDO's default mapping.
 * Please note, that PDO entry registration will fail, if the PDO
 * configuration is left empty and the slave is offline.
 *
 * \code
 * ec_pdo_info_t pdos[] = {
 *     {0x1600}, // Channel 1
 *     {0x1601}  // Channel 2
 * };
 *
 * ec_sync_info_t syncs[] = {
 *     {3, EC_DIR_INPUT, 2, pdos},
 * };
 *
 * if (ecrt_slave_config_pdos(slave_config_ana_in, 1, syncs)) {
 *     // handle error
 * }
 * \endcode
 *
 * Processing of \a syncs will stop, if
 * - the number of processed items reaches \a n_syncs, or
 * - the \a index member of an ec_sync_info_t item is 0xff. In this case,
 *   \a n_syncs should set to a number greater than the number of list items;
 *   using EC_END is recommended.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdos(
        ec_slave_config_t *sc, /**< Slave configuration. */
        unsigned int n_syncs, /**< Number of sync manager configurations in
                                \a syncs. */
        const ec_sync_info_t syncs[] /**< Array of sync manager
                                       configurations. */
        );

/** Registers a PDO entry for process data exchange in a domain.
 *
 * Searches the assigned PDOs for the given PDO entry. An error is raised, if
 * the given entry is not mapped. Otherwise, the corresponding sync manager
 * and FMMU configurations are provided for slave configuration and the
 * respective sync manager's assigned PDOs are appended to the given domain,
 * if not already done. The offset of the requested PDO entry's data inside
 * the domain's process data is returned. Optionally, the PDO entry bit
 * position (0-7) can be retrieved via the \a bit_position output parameter.
 * This pointer may be \a NULL, in this case an error is raised if the PDO
 * entry does not byte-align.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \retval >=0 Success: Offset of the PDO entry's process data.
 * \retval  <0 Error code.
 */
int ecrt_slave_config_reg_pdo_entry(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t entry_index, /**< Index of the PDO entry to register. */
        uint8_t entry_subindex, /**< Subindex of the PDO entry to register. */
        ec_domain_t *domain, /**< Domain. */
        unsigned int *bit_position /**< Optional address if bit addressing
                                 is desired */
        );

/** Registers a PDO entry using its position.
 *
 * Similar to ecrt_slave_config_reg_pdo_entry(), but not using PDO indices but
 * offsets in the PDO mapping, because PDO entry indices may not be unique
 * inside a slave's PDO mapping. An error is raised, if
 * one of the given positions is out of range.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \retval >=0 Success: Offset of the PDO entry's process data.
 * \retval  <0 Error code.
 */
int ecrt_slave_config_reg_pdo_entry_pos(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t sync_index, /**< Sync manager index. */
        unsigned int pdo_pos, /**< Position of the PDO inside the SM. */
        unsigned int entry_pos, /**< Position of the entry inside the PDO. */
        ec_domain_t *domain, /**< Domain. */
        unsigned int *bit_position /**< Optional address if bit addressing
                                 is desired */
        );

/** Configure distributed clocks.
 *
 * Sets the AssignActivate word and the cycle and shift times for the sync
 * signals.
 *
 * The AssignActivate word is vendor-specific and can be taken from the XML
 * device description file (Device -> Dc -> AssignActivate). Set this to zero,
 * if the slave shall be operated without distributed clocks (default).
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \attention The \a sync1_shift time is ignored.
 */
void ecrt_slave_config_dc(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t assign_activate, /**< AssignActivate word. */
        uint32_t sync0_cycle, /**< SYNC0 cycle time [ns]. */
        int32_t sync0_shift, /**< SYNC0 shift time [ns]. */
        uint32_t sync1_cycle, /**< SYNC1 cycle time [ns]. */
        int32_t sync1_shift /**< SYNC1 shift time [ns]. */
        );

/** Add an SDO configuration.
 *
 * An SDO configuration is stored in the slave configuration object and is
 * downloaded to the slave whenever the slave is being configured by the
 * master. This usually happens once on master activation, but can be repeated
 * subsequently, for example after the slave's power supply failed.
 *
 * \attention The SDOs for PDO assignment (\p 0x1C10 - \p 0x1C2F) and PDO
 * mapping (\p 0x1600 - \p 0x17FF and \p 0x1A00 - \p 0x1BFF) should not be
 * configured with this function, because they are part of the slave
 * configuration done by the master. Please use ecrt_slave_config_pdos() and
 * friends instead.
 *
 * This is the generic function for adding an SDO configuration. Please note
 * that the this function does not do any endianness correction. If
 * datatype-specific functions are needed (that automatically correct the
 * endianness), have a look at ecrt_slave_config_sdo8(),
 * ecrt_slave_config_sdo16() and ecrt_slave_config_sdo32().
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_slave_config_sdo(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t index, /**< Index of the SDO to configure. */
        uint8_t subindex, /**< Subindex of the SDO to configure. */
        const uint8_t *data, /**< Pointer to the data. */
        size_t size /**< Size of the \a data. */
        );

/** Add a configuration value for an 8-bit SDO.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_sdo().
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_slave_config_sdo8(
        ec_slave_config_t *sc, /**< Slave configuration */
        uint16_t sdo_index, /**< Index of the SDO to configure. */
        uint8_t sdo_subindex, /**< Subindex of the SDO to configure. */
        uint8_t value /**< Value to set. */
        );

/** Add a configuration value for a 16-bit SDO.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_sdo().
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_slave_config_sdo16(
        ec_slave_config_t *sc, /**< Slave configuration */
        uint16_t sdo_index, /**< Index of the SDO to configure. */
        uint8_t sdo_subindex, /**< Subindex of the SDO to configure. */
        uint16_t value /**< Value to set. */
        );

/** Add a configuration value for a 32-bit SDO.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_sdo().
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_slave_config_sdo32(
        ec_slave_config_t *sc, /**< Slave configuration */
        uint16_t sdo_index, /**< Index of the SDO to configure. */
        uint8_t sdo_subindex, /**< Subindex of the SDO to configure. */
        uint32_t value /**< Value to set. */
        );

/** Add configuration data for a complete SDO.
 *
 * The SDO data are transferred via CompleteAccess. Data for the first
 * subindex (0) have to be included.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_sdo().
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_slave_config_complete_sdo(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t index, /**< Index of the SDO to configure. */
        const uint8_t *data, /**< Pointer to the data. */
        size_t size /**< Size of the \a data. */
        );

/** Set the size of the CoE emergency ring buffer.
 *
 * The initial size is zero, so all messages will be dropped. This method can
 * be called even after master activation, but it will clear the ring buffer!
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return 0 on success, or negative error code.
 */
int ecrt_slave_config_emerg_size(
        ec_slave_config_t *sc, /**< Slave configuration. */
        size_t elements /**< Number of records of the CoE emergency ring. */
        );

/** Read and remove one record from the CoE emergency ring buffer.
 *
 * A record consists of 8 bytes:
 *
 * Byte 0-1: Error code (little endian)
 * Byte   2: Error register
 * Byte 3-7: Data
 *
 * \return 0 on success (record popped), or negative error code (i. e.
 * -ENOENT, if ring is empty).
 */
int ecrt_slave_config_emerg_pop(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t *target /**< Pointer to target memory (at least
                          EC_COE_EMERGENCY_MSG_SIZE bytes). */
        );

/** Clears CoE emergency ring buffer and the overrun counter.
 *
 * \return 0 on success, or negative error code.
 */
int ecrt_slave_config_emerg_clear(
        ec_slave_config_t *sc /**< Slave configuration. */
        );

/** Read the number of CoE emergency overruns.
 *
 * The overrun counter will be incremented when a CoE emergency message could
 * not be stored in the ring buffer and had to be dropped. Call
 * ecrt_slave_config_emerg_clear() to reset the counter.
 *
 * \return Number of overruns since last clear, or negative error code.
 */
int ecrt_slave_config_emerg_overruns(
        ec_slave_config_t *sc /**< Slave configuration. */
        );

/** Create an SDO request to exchange SDOs during realtime operation.
 *
 * The created SDO request object is freed automatically when the master is
 * released.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return New SDO request, or NULL on error.
 */
ec_sdo_request_t *ecrt_slave_config_create_sdo_request(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t index, /**< SDO index. */
        uint8_t subindex, /**< SDO subindex. */
        size_t size /**< Data size to reserve. */
        );

/** Create an SDO request to exchange SDOs during realtime operation
 *  using complete access.
 *
 * The created SDO request object is freed automatically when the master is
 * released.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return New SDO request, or NULL on error.
 */
ec_sdo_request_t *ecrt_slave_config_create_sdo_request_complete(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t index, /**< SDO index. */
        size_t size /**< Data size to reserve. */
        );

/** Create an FoE request to exchange files during realtime operation.
 *
 * The created FoE request object is freed automatically when the master is
 * released.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return New FoE request, or NULL on error.
 */
ec_foe_request_t *ecrt_slave_config_create_foe_request(
        ec_slave_config_t *sc, /**< Slave configuration. */
        size_t size /**< Data size to reserve. */
        );

/** Create an VoE handler to exchange vendor-specific data during realtime
 * operation.
 *
 * The number of VoE handlers per slave configuration is not limited, but
 * usually it is enough to create one for sending and one for receiving, if
 * both can be done simultaneously.
 *
 * The created VoE handler object is freed automatically when the master is
 * released.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return New VoE handler, or NULL on error.
 */
ec_voe_handler_t *ecrt_slave_config_create_voe_handler(
        ec_slave_config_t *sc, /**< Slave configuration. */
        size_t size /**< Data size to reserve. */
        );

/** Create a register request to exchange EtherCAT register contents during
 * realtime operation.
 *
 * This interface should not be used to take over master functionality,
 * instead it is intended for debugging and monitoring reasons.
 *
 * The created register request object is freed automatically when the master
 * is released.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \return New register request, or NULL on error.
 */
ec_reg_request_t *ecrt_slave_config_create_reg_request(
        ec_slave_config_t *sc, /**< Slave configuration. */
        size_t size /**< Data size to reserve. */
        );

/** Outputs the state of the slave configuration.
 *
 * Stores the state information in the given \a state structure. The state
 * information is updated by the master state machine, so it may take a few
 * cycles, until it changes.
 *
 * \attention If the state of process data exchange shall be monitored in
 * realtime, ecrt_domain_state() should be used.
 */
void ecrt_slave_config_state(
        const ec_slave_config_t *sc, /**< Slave configuration */
        ec_slave_config_state_t *state /**< State object to write to. */
        );

/** Add an SoE IDN configuration.
 *
 * A configuration for a Sercos-over-EtherCAT IDN is stored in the slave
 * configuration object and is written to the slave whenever the slave is
 * being configured by the master. This usually happens once on master
 * activation, but can be repeated subsequently, for example after the slave's
 * power supply failed.
 *
 * The \a idn parameter can be separated into several sections:
 *  - Bit 15: Standard data (0) or Product data (1)
 *  - Bit 14 - 12: Parameter set (0 - 7)
 *  - Bit 11 - 0: Data block number (0 - 4095)
 *
 * Please note that the this function does not do any endianness correction.
 * Multi-byte data have to be passed in EtherCAT endianness (little-endian).
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ecrt_slave_config_idn(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t drive_no, /**< Drive number. */
        uint16_t idn, /**< SoE IDN. */
        ec_al_state_t state, /**< AL state in which to write the IDN (PREOP or
                               SAFEOP). */
        const uint8_t *data, /**< Pointer to the data. */
        size_t size /**< Size of the \a data. */
        );

/******************************************************************************
 * Domain methods
 *****************************************************************************/

/** Registers a bunch of PDO entries for a domain.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 * \see ecrt_slave_config_reg_pdo_entry()
 *
 * \attention The registration array has to be terminated with an empty
 *            structure, or one with the \a index field set to zero!
 * \return 0 on success, else non-zero.
 */
int ecrt_domain_reg_pdo_entry_list(
        ec_domain_t *domain, /**< Domain. */
        const ec_pdo_entry_reg_t *pdo_entry_regs /**< Array of PDO
                                                   registrations. */
        );

/** Returns the current size of the domain's process data.
 *
 * \return Size of the process data image, or a negative error code.
 */
size_t ecrt_domain_size(
        const ec_domain_t *domain /**< Domain. */
        );

#ifdef __KERNEL__

/** Provide external memory to store the domain's process data.
 *
 * Call this after all PDO entries have been registered and before activating
 * the master.
 *
 * The size of the allocated memory must be at least ecrt_domain_size(), after
 * all PDO entries have been registered.
 *
 * This method has to be called in non-realtime context before
 * ecrt_master_activate().
 *
 */
void ecrt_domain_external_memory(
        ec_domain_t *domain, /**< Domain. */
        uint8_t *memory /**< Address of the memory to store the process
                          data in. */
        );

#endif /* __KERNEL__ */

/** Returns the domain's process data.
 *
 * - In kernel context: If external memory was provided with
 * ecrt_domain_external_memory(), the returned pointer will contain the
 * address of that memory. Otherwise it will point to the internally allocated
 * memory. In the latter case, this method may not be called before
 * ecrt_master_activate().
 *
 * - In userspace context: This method has to be called after
 * ecrt_master_activate() to get the mapped domain process data memory.
 *
 * \return Pointer to the process data memory.
 */
uint8_t *ecrt_domain_data(
        ec_domain_t *domain /**< Domain. */
        );

/** Determines the states of the domain's datagrams.
 *
 * Evaluates the working counters of the received datagrams and outputs
 * statistics, if necessary. This must be called after ecrt_master_receive()
 * is expected to receive the domain datagrams in order to make
 * ecrt_domain_state() return the result of the last process data exchange.
 */
void ecrt_domain_process(
        ec_domain_t *domain /**< Domain. */
        );

/** (Re-)queues all domain datagrams in the master's datagram queue.
 *
 * Call this function to mark the domain's datagrams for exchanging at the
 * next call of ecrt_master_send().
 */
void ecrt_domain_queue(
        ec_domain_t *domain /**< Domain. */
        );

/** Reads the state of a domain.
 *
 * Stores the domain state in the given \a state structure.
 *
 * Using this method, the process data exchange can be monitored in realtime.
 */
void ecrt_domain_state(
        const ec_domain_t *domain, /**< Domain. */
        ec_domain_state_t *state /**< Pointer to a state object to store the
                                   information. */
        );

/*****************************************************************************
 * SDO request methods.
 ****************************************************************************/

/** Set the SDO index and subindex and prepare for non-complete-access.
 *
 * This is valid even if the request was created for complete-access.
 *
 * \attention If the SDO index and/or subindex is changed while
 * ecrt_sdo_request_state() returns EC_REQUEST_BUSY, this may lead to
 * unexpected results.
 */
void ecrt_sdo_request_index(
        ec_sdo_request_t *req, /**< SDO request. */
        uint16_t index, /**< SDO index. */
        uint8_t subindex /**< SDO subindex. */
        );

/** Set the SDO index and prepare for complete-access.
 *
 * This is valid even if the request was not created for complete-access.
 *
 * \attention If the SDO index is changed while ecrt_sdo_request_state()
 * returns EC_REQUEST_BUSY, this may lead to unexpected results.
 */
void ecrt_sdo_request_index_complete(
        ec_sdo_request_t *req, /**< SDO request. */
        uint16_t index /**< SDO index. */
        );

/** Set the timeout for an SDO request.
 *
 * If the request cannot be processed in the specified time, if will be marked
 * as failed.
 *
 * The timeout is permanently stored in the request object and is valid until
 * the next call of this method.
 */
void ecrt_sdo_request_timeout(
        ec_sdo_request_t *req, /**< SDO request. */
        uint32_t timeout /**< Timeout in milliseconds. Zero means no
                           timeout. */
        );

/** Access to the SDO request's data.
 *
 * This function returns a pointer to the request's internal SDO data memory.
 *
 * - After a read operation was successful, integer data can be evaluated using
 *   the EC_READ_*() macros as usual. Example:
 *   \code
 *   uint16_t value = EC_READ_U16(ecrt_sdo_request_data(sdo)));
 *   \endcode
 * - If a write operation shall be triggered, the data have to be written to
 *   the internal memory. Use the EC_WRITE_*() macros, if you are writing
 *   integer data. Be sure, that the data fit into the memory. The memory size
 *   is a parameter of ecrt_slave_config_create_sdo_request().
 *   \code
 *   EC_WRITE_U16(ecrt_sdo_request_data(sdo), 0xFFFF);
 *   \endcode
 *
 * \attention The return value can be invalid during a read operation, because
 * the internal SDO data memory could be re-allocated if the read SDO data do
 * not fit inside.
 *
 * \return Pointer to the internal SDO data memory.
 */
uint8_t *ecrt_sdo_request_data(
        ec_sdo_request_t *req /**< SDO request. */
        );

/** Returns the current SDO data size.
 *
 * When the SDO request is created, the data size is set to the size of the
 * reserved memory. After a read operation the size is set to the size of the
 * read data. The size is not modified in any other situation.
 *
 * \return SDO data size in bytes.
 */
size_t ecrt_sdo_request_data_size(
        const ec_sdo_request_t *req /**< SDO request. */
        );

/** Get the current state of the SDO request.
 *
 * \return Request state.
 */
#ifdef __KERNEL__
ec_request_state_t ecrt_sdo_request_state(
        const ec_sdo_request_t *req /**< SDO request. */
    );
#else
ec_request_state_t ecrt_sdo_request_state(
        ec_sdo_request_t *req /**< SDO request. */
    );
#endif

/** Schedule an SDO write operation.
 *
 * \attention This method may not be called while ecrt_sdo_request_state()
 * returns EC_REQUEST_BUSY.
 */
void ecrt_sdo_request_write(
        ec_sdo_request_t *req /**< SDO request. */
        );

/** Schedule an SDO write operation.
 *
 * \attention This method may not be called while ecrt_sdo_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \attention The size must be less than or equal to the size specified
 * when the request was created.
 */
void ecrt_sdo_request_write_with_size(
        ec_sdo_request_t *req, /**< SDO request. */
        size_t size /**< Size of data to write. */
        );

/** Schedule an SDO read operation.
 *
 * \attention This method may not be called while ecrt_sdo_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \attention After calling this function, the return value of
 * ecrt_sdo_request_data() must be considered as invalid while
 * ecrt_sdo_request_state() returns EC_REQUEST_BUSY.
 */
void ecrt_sdo_request_read(
        ec_sdo_request_t *req /**< SDO request. */
        );

/*****************************************************************************
 * FoE request methods.
 ****************************************************************************/

/** Select the filename to use for the next FoE operation.
 */
void ecrt_foe_request_file(
        ec_foe_request_t *req, /**< FoE request. */
        const char *file_name, /**< File name. */
        uint32_t password /**< Password. */
        );

/** Set the timeout for an FoE request.
 *
 * If the request cannot be processed in the specified time, if will be marked
 * as failed.
 *
 * The timeout is permanently stored in the request object and is valid until
 * the next call of this method.
 */
void ecrt_foe_request_timeout(
        ec_foe_request_t *req, /**< FoE request. */
        uint32_t timeout /**< Timeout in milliseconds. Zero means no
                           timeout. */
        );

/** Access to the FoE request's data.
 *
 * This function returns a pointer to the request's internal data memory.
 *
 * - After a read operation was successful, the data can be read from this
 *   buffer up to the ecrt_foe_request_data_size().
 * - If a write operation shall be triggered, the data has to be written to
 *   the internal memory. Be sure that the data fit into the memory. The
 *   memory size is a parameter of ecrt_slave_config_create_foe_request().
 *
 * \attention The return value can be invalid during a read operation, because
 * the internal data memory could be re-allocated if the read data does not
 * fit inside.
 *
 * \return Pointer to the internal file data memory.
 */
uint8_t *ecrt_foe_request_data(
        ec_foe_request_t *req /**< FoE request. */
        );

/** Returns the current FoE data size.
 *
 * When the FoE request is created, the data size is set to the size of the
 * reserved memory. After a read operation completes the size is set to the
 * size of the read data. After a write operation starts the size is set to
 * the size of the data to write.
 *
 * \return FoE data size in bytes.
 */
size_t ecrt_foe_request_data_size(
        const ec_foe_request_t *req /**< FoE request. */
        );

/** Get the current state of the FoE request.
 *
 * \return Request state.
 */
#ifdef __KERNEL__
ec_request_state_t ecrt_foe_request_state(
        const ec_foe_request_t *req /**< FoE request. */
    );
#else
ec_request_state_t ecrt_foe_request_state(
        ec_foe_request_t *req /**< FoE request. */
    );
#endif

/** Get the result of the FoE request.
 *
 * \attention This method may not be called while ecrt_foe_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \return FoE transfer result.
 */
ec_foe_error_t ecrt_foe_request_result(
        const ec_foe_request_t *req /**< FoE request. */
    );

/** Get the FoE error code from the FoE request.
 *
 * \attention This value is only valid when ecrt_foe_request_result()
 * returns FOE_OPCODE_ERROR.
 *
 * \return FoE error code.  If the returned value is zero, then the error
 * is that an unexpected opcode was received; if it is non-zero then the
 * value is the code reported by the slave in the FoE ERROR opcode.
 */
uint32_t ecrt_foe_request_error_code(
        const ec_foe_request_t *req /**< FoE request. */
    );

/** Returns the progress of the current @EC_REQUEST_BUSY transfer.
 *
 * \attention Must be called after ecrt_foe_request_state().
 *
 * \return Progress in bytes.
 */
size_t ecrt_foe_request_progress(
        const ec_foe_request_t *req /**< FoE request. */
        );

/** Schedule an FoE write operation.
 *
 * \attention This method may not be called while ecrt_foe_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \attention The size must be less than or equal to the size specified
 * when the request was created.
 */
void ecrt_foe_request_write(
        ec_foe_request_t *req, /**< FoE request. */
        size_t size /**< Size of data to write. */
        );

/** Schedule an FoE read operation.
 *
 * \attention This method may not be called while ecrt_foe_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \attention After calling this function, the return value of
 * ecrt_foe_request_data() must be considered as invalid while
 * ecrt_foe_request_state() returns EC_REQUEST_BUSY.
 */
void ecrt_foe_request_read(
        ec_foe_request_t *req /**< FoE request. */
        );

/*****************************************************************************
 * VoE handler methods.
 ****************************************************************************/

/** Sets the VoE header for future send operations.
 *
 * A VoE message shall contain a 4-byte vendor ID, followed by a 2-byte vendor
 * type at as header. These numbers can be set with this function. The values
 * are valid and will be used for future send operations until the next call
 * of this method.
 */
void ecrt_voe_handler_send_header(
        ec_voe_handler_t *voe, /**< VoE handler. */
        uint32_t vendor_id, /**< Vendor ID. */
        uint16_t vendor_type /**< Vendor-specific type. */
        );

/** Reads the header data of a received VoE message.
 *
 * This method can be used to get the received VoE header information after a
 * read operation has succeeded.
 *
 * The header information is stored at the memory given by the pointer
 * parameters.
 */
void ecrt_voe_handler_received_header(
        const ec_voe_handler_t *voe, /**< VoE handler. */
        uint32_t *vendor_id, /**< Vendor ID. */
        uint16_t *vendor_type /**< Vendor-specific type. */
        );

/** Access to the VoE handler's data.
 *
 * This function returns a pointer to the VoE handler's internal memory, that
 * points to the actual VoE data right after the VoE header (see
 * ecrt_voe_handler_send_header()).
 *
 * - After a read operation was successful, the memory contains the received
 *   data. The size of the received data can be determined via
 *   ecrt_voe_handler_data_size().
 * - Before a write operation is triggered, the data have to be written to the
 *   internal memory. Be sure, that the data fit into the memory. The reserved
 *   memory size is a parameter of ecrt_slave_config_create_voe_handler().
 *
 * \attention The returned pointer is not necessarily persistent: After a read
 * operation, the internal memory may have been reallocated. This can be
 * avoided by reserving enough memory via the \a size parameter of
 * ecrt_slave_config_create_voe_handler().
 *
 * \return Pointer to the internal memory.
 */
uint8_t *ecrt_voe_handler_data(
        ec_voe_handler_t *voe /**< VoE handler. */
        );

/** Returns the current data size.
 *
 * The data size is the size of the VoE data without the header (see
 * ecrt_voe_handler_send_header()).
 *
 * When the VoE handler is created, the data size is set to the size of the
 * reserved memory. At a write operation, the data size is set to the number
 * of bytes to write. After a read operation the size is set to the size of
 * the read data. The size is not modified in any other situation.
 *
 * \return Data size in bytes.
 */
size_t ecrt_voe_handler_data_size(
        const ec_voe_handler_t *voe /**< VoE handler. */
        );

/** Start a VoE write operation.
 *
 * After this function has been called, the ecrt_voe_handler_execute() method
 * must be called in every bus cycle as long as it returns EC_REQUEST_BUSY. No
 * other operation may be started while the handler is busy.
 */
void ecrt_voe_handler_write(
        ec_voe_handler_t *voe, /**< VoE handler. */
        size_t size /**< Number of bytes to write (without the VoE header). */
        );

/** Start a VoE read operation.
 *
 * After this function has been called, the ecrt_voe_handler_execute() method
 * must be called in every bus cycle as long as it returns EC_REQUEST_BUSY. No
 * other operation may be started while the handler is busy.
 *
 * The state machine queries the slave's send mailbox for new data to be send
 * to the master. If no data appear within the EC_VOE_RESPONSE_TIMEOUT
 * (defined in master/voe_handler.c), the operation fails.
 *
 * On success, the size of the read data can be determined via
 * ecrt_voe_handler_data_size(), while the VoE header of the received data
 * can be retrieved with ecrt_voe_handler_received_header().
 */
void ecrt_voe_handler_read(
        ec_voe_handler_t *voe /**< VoE handler. */
        );

/** Start a VoE read operation without querying the sync manager status.
 *
 * After this function has been called, the ecrt_voe_handler_execute() method
 * must be called in every bus cycle as long as it returns EC_REQUEST_BUSY. No
 * other operation may be started while the handler is busy.
 *
 * The state machine queries the slave by sending an empty mailbox. The slave
 * fills its data to the master in this mailbox. If no data appear within the
 * EC_VOE_RESPONSE_TIMEOUT (defined in master/voe_handler.c), the operation
 * fails.
 *
 * On success, the size of the read data can be determined via
 * ecrt_voe_handler_data_size(), while the VoE header of the received data
 * can be retrieved with ecrt_voe_handler_received_header().
 */
void ecrt_voe_handler_read_nosync(
        ec_voe_handler_t *voe /**< VoE handler. */
        );

/** Execute the handler.
 *
 * This method executes the VoE handler. It has to be called in every bus
 * cycle as long as it returns EC_REQUEST_BUSY.
 *
 * \return Handler state.
 */
ec_request_state_t ecrt_voe_handler_execute(
    ec_voe_handler_t *voe /**< VoE handler. */
    );

/*****************************************************************************
 * Register request methods.
 ****************************************************************************/

/** Access to the register request's data.
 *
 * This function returns a pointer to the request's internal memory.
 *
 * - After a read operation was successful, integer data can be evaluated
 *   using the EC_READ_*() macros as usual. Example:
 *   \code
 *   uint16_t value = EC_READ_U16(ecrt_reg_request_data(reg_request)));
 *   \endcode
 * - If a write operation shall be triggered, the data have to be written to
 *   the internal memory. Use the EC_WRITE_*() macros, if you are writing
 *   integer data. Be sure, that the data fit into the memory. The memory size
 *   is a parameter of ecrt_slave_config_create_reg_request().
 *   \code
 *   EC_WRITE_U16(ecrt_reg_request_data(reg_request), 0xFFFF);
 *   \endcode
 *
 * \return Pointer to the internal memory.
 */
uint8_t *ecrt_reg_request_data(
        ec_reg_request_t *req /**< Register request. */
        );

/** Get the current state of the register request.
 *
 * \return Request state.
 */
#ifdef __KERNEL__
ec_request_state_t ecrt_reg_request_state(
        const ec_reg_request_t *req /**< Register request. */
    );
#else
ec_request_state_t ecrt_reg_request_state(
        ec_reg_request_t *req /**< Register request. */
    );
#endif

/** Schedule an register write operation.
 *
 * \attention This method may not be called while ecrt_reg_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \attention The \a size parameter is truncated to the size given at request
 * creation.
 */
void ecrt_reg_request_write(
        ec_reg_request_t *req, /**< Register request. */
        uint16_t address, /**< Register address. */
        size_t size /**< Size to write. */
        );

/** Schedule a register read operation.
 *
 * \attention This method may not be called while ecrt_reg_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \attention The \a size parameter is truncated to the size given at request
 * creation.
 */
void ecrt_reg_request_read(
        ec_reg_request_t *req, /**< Register request. */
        uint16_t address, /**< Register address. */
        size_t size /**< Size to write. */
        );

/******************************************************************************
 * Bitwise read/write macros
 *****************************************************************************/

/** Read a certain bit of an EtherCAT data byte.
 *
 * \param DATA EtherCAT data pointer
 * \param POS bit position
 */
#define EC_READ_BIT(DATA, POS) ((*((uint8_t *) (DATA)) >> (POS)) & 0x01)

/** Write a certain bit of an EtherCAT data byte.
 *
 * \param DATA EtherCAT data pointer
 * \param POS bit position
 * \param VAL new bit value
 */
#define EC_WRITE_BIT(DATA, POS, VAL) \
    do { \
        if (VAL) *((uint8_t *) (DATA)) |=  (1 << (POS)); \
        else     *((uint8_t *) (DATA)) &= ~(1 << (POS)); \
    } while (0)

/******************************************************************************
 * Byte-swapping functions for user space
 *****************************************************************************/

#ifndef __KERNEL__

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define le16_to_cpu(x) x
#define le32_to_cpu(x) x
#define le64_to_cpu(x) x

#define cpu_to_le16(x) x
#define cpu_to_le32(x) x
#define cpu_to_le64(x) x

#elif __BYTE_ORDER == __BIG_ENDIAN

#define swap16(x) \
        ((uint16_t)( \
        (((uint16_t)(x) & 0x00ffU) << 8) | \
        (((uint16_t)(x) & 0xff00U) >> 8) ))
#define swap32(x) \
        ((uint32_t)( \
        (((uint32_t)(x) & 0x000000ffUL) << 24) | \
        (((uint32_t)(x) & 0x0000ff00UL) <<  8) | \
        (((uint32_t)(x) & 0x00ff0000UL) >>  8) | \
        (((uint32_t)(x) & 0xff000000UL) >> 24) ))
#define swap64(x) \
        ((uint64_t)( \
        (((uint64_t)(x) & 0x00000000000000ffULL) << 56) | \
        (((uint64_t)(x) & 0x000000000000ff00ULL) << 40) | \
        (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24) | \
        (((uint64_t)(x) & 0x00000000ff000000ULL) <<  8) | \
        (((uint64_t)(x) & 0x000000ff00000000ULL) >>  8) | \
        (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24) | \
        (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40) | \
        (((uint64_t)(x) & 0xff00000000000000ULL) >> 56) ))

#define le16_to_cpu(x) swap16(x)
#define le32_to_cpu(x) swap32(x)
#define le64_to_cpu(x) swap64(x)

#define cpu_to_le16(x) swap16(x)
#define cpu_to_le32(x) swap32(x)
#define cpu_to_le64(x) swap64(x)

#endif

#define le16_to_cpup(x) le16_to_cpu(*((uint16_t *)(x)))
#define le32_to_cpup(x) le32_to_cpu(*((uint32_t *)(x)))
#define le64_to_cpup(x) le64_to_cpu(*((uint64_t *)(x)))

#endif /* ifndef __KERNEL__ */

/******************************************************************************
 * Read macros
 *****************************************************************************/

/** Read an 8-bit unsigned value from EtherCAT data.
 *
 * \return EtherCAT data value
 */
#define EC_READ_U8(DATA) \
    ((uint8_t) *((uint8_t *) (DATA)))

/** Read an 8-bit signed value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S8(DATA) \
     ((int8_t) *((uint8_t *) (DATA)))

/** Read a 16-bit unsigned value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_U16(DATA) \
     ((uint16_t) le16_to_cpup((void *) (DATA)))

/** Read a 16-bit signed value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S16(DATA) \
     ((int16_t) le16_to_cpup((void *) (DATA)))

/** Read a 32-bit unsigned value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_U32(DATA) \
     ((uint32_t) le32_to_cpup((void *) (DATA)))

/** Read a 32-bit signed value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S32(DATA) \
     ((int32_t) le32_to_cpup((void *) (DATA)))

/** Read a 64-bit unsigned value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_U64(DATA) \
     ((uint64_t) le64_to_cpup((void *) (DATA)))

/** Read a 64-bit signed value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S64(DATA) \
     ((int64_t) le64_to_cpup((void *) (DATA)))

/******************************************************************************
 * Floating-point read functions and macros (userspace only)
 *****************************************************************************/

#ifndef __KERNEL__

/** Read a 32-bit floating-point value from EtherCAT data.
 *
 * \param data EtherCAT data pointer
 * \return EtherCAT data value
 */
float ecrt_read_real(const void *data);

/** Read a 32-bit floating-point value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_REAL(DATA) ecrt_read_real(DATA)

/** Read a 64-bit floating-point value from EtherCAT data.
 *
 * \param data EtherCAT data pointer
 * \return EtherCAT data value
 */
double ecrt_read_lreal(const void *data);

/** Read a 64-bit floating-point value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_LREAL(DATA) ecrt_read_lreal(DATA)

#endif // ifndef __KERNEL__

/******************************************************************************
 * Write macros
 *****************************************************************************/

/** Write an 8-bit unsigned value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U8(DATA, VAL) \
    do { \
        *((uint8_t *)(DATA)) = ((uint8_t) (VAL)); \
    } while (0)

/** Write an 8-bit signed value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S8(DATA, VAL) EC_WRITE_U8(DATA, VAL)

/** Write a 16-bit unsigned value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U16(DATA, VAL) \
    do { \
        *((uint16_t *) (DATA)) = cpu_to_le16((uint16_t) (VAL)); \
    } while (0)

/** Write a 16-bit signed value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S16(DATA, VAL) EC_WRITE_U16(DATA, VAL)

/** Write a 32-bit unsigned value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U32(DATA, VAL) \
    do { \
        *((uint32_t *) (DATA)) = cpu_to_le32((uint32_t) (VAL)); \
    } while (0)

/** Write a 32-bit signed value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S32(DATA, VAL) EC_WRITE_U32(DATA, VAL)

/** Write a 64-bit unsigned value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U64(DATA, VAL) \
    do { \
        *((uint64_t *) (DATA)) = cpu_to_le64((uint64_t) (VAL)); \
    } while (0)

/** Write a 64-bit signed value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S64(DATA, VAL) EC_WRITE_U64(DATA, VAL)

/******************************************************************************
 * Floating-point write functions and macros (userspace only)
 *****************************************************************************/

#ifndef __KERNEL__

/** Write a 32-bit floating-point value to EtherCAT data.
 *
 * \param data EtherCAT data pointer
 * \param value new value
 */
void ecrt_write_real(void *data, float value);

/** Write a 32-bit floating-point value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_REAL(DATA, VAL) ecrt_write_real(DATA, VAL)

/** Write a 64-bit floating-point value to EtherCAT data.
 *
 * \param data EtherCAT data pointer
 * \param value new value
 */
void ecrt_write_lreal(void *data, double value);

/** Write a 64-bit floating-point value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_LREAL(DATA, VAL) ecrt_write_lreal(DATA, VAL)

#endif // ifndef __KERNEL__

/** Schedule a register read-write operation.
 *
 * \attention This method may not be called while ecrt_reg_request_state()
 * returns EC_REQUEST_BUSY.
 *
 * \attention The \a size parameter is truncated to the size given at request
 * creation.
 */
void ecrt_reg_request_readwrite(
        ec_reg_request_t *req, /**< Register request. */
        uint16_t address, /**< Register address. */
        size_t size /**< Size to read-write. */
        );

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

/*****************************************************************************/

/** @} */

#endif
