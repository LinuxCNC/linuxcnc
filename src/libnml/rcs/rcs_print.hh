#ifndef RCS_PRNT_HH
#define RCS_PRNT_HH

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>		/* va_list */

#ifdef __cplusplus
}
#include "linklist.hh"		/* LinkedList */
extern LinkedList *get_rcs_print_list();

extern "C" {
#endif

    extern int rcs_vprint(char *_fmt, va_list va_args, int save_string);
    extern int rcs_print(char *_fmt, ...);
    extern int rcs_print_debug(long, char *_fmt, ...);
    extern int rcs_print_error(char *_fmt, ...);
    extern void set_rcs_print_flag(unsigned long flags_to_set);

/* Tokenizes the string _src up to a maximum of _max.
   Returns the total number of tokens and pointers to each one in _dest[i]
   or -1 if the string is NULL or greater than 255 chars.
*/
    extern int separate_words(char **_dest, int _max, char *_src);
    extern int rcs_puts(char *);
    extern int rcs_fputs(char *);

#ifdef __cplusplus
}
#endif
/* Print MODE flags. */
#define PRINT_RCS_ERRORS                0x00000001	/* 1 */
#define PRINT_NODE_CONSTRUCTORS         0x00000002	/* 2 */
#define PRINT_NODE_DESTRUCTORS          0x00000004	/* 4 */
#define PRINT_CMS_CONSTRUCTORS          0x00000008	/* 8 */
#define PRINT_CMS_DESTRUCTORS           0x00000010	/* 16 */
#define PRINT_NML_CONSTRUCTORS          0x00000020	/* 32 */
#define PRINT_NML_DESTRUCTORS           0x00000040	/* 64 */
#define PRINT_COMMANDS_RECIEVED         0x00000100	/* 256 */
#define PRINT_COMMANDS_SENT             0x00000200	/* 512 */
#define PRINT_STATUS_RECIEVED           0x00000400	/* 1024 */
#define PRINT_STATUS_SENT               0x00000800	/* 2048 */
#define PRINT_NODE_CYCLES               0x00001000	/* 4096 */
#define PRINT_NODE_MISSED_CYCLES        0x00002000	/* 8192 */
#define PRINT_NODE_CYCLE_TIMES          0x00004000	/* 16384 */
#define PRINT_NODE_PROCESS_TIMES        0x00008000	/* 32768 */
#define PRINT_NEW_WM                    0x00010000	/* 65536 */
#define PRINT_NODE_ABORT                0x00020000	/* 131072 */
#define PRINT_CMS_CONFIG_INFO           0x00040000	/* 262144 */
#define PRINT_SOCKET_READ_SIZE          0x00080000	/* 524288 */
#define PRINT_SOCKET_WRITE_SIZE         0x00100000	/* 1048576 */
#define PRINT_INTERFACE_LOADING         0x00200000	/* 2097152 */
#define PRINT_RPC_SERVER_CALL           0x00400000	/* 4194304 */
#define PRINT_SEMAPHORE_ACTIVITY        0x00800000	/* 8388608 */
#define PRINT_SOCKET_CONNECT            0x01000000	/* 16777216 */
#define PRINT_SERVER_THREAD_ACTIVITY    0x02000000	// 33554432
#define PRINT_SERVER_SUBSCRIPTION_ACTIVITY    0x04000000	// 67108864
#define PRINT_SHARED_MEMORY_ACTIVITY    0x08000000
#define PRINT_ALL_SOCKET_REQUESTS       0x10000000
#define PRINT_UPDATER_ACTIVITY          0x20000000
#define PRINT_MISC                      0x40000000
#define PRINT_EVERYTHING                0xFFFFFFFF	/* 4294967295 */
#ifdef __cplusplus
enum RCS_PRINT_DESTINATION_TYPE {
#else
typedef enum {
#endif
    RCS_PRINT_TO_STDOUT,
    RCS_PRINT_TO_STDERR,
    RCS_PRINT_TO_NULL,
    RCS_PRINT_TO_LIST,
    RCS_PRINT_TO_FILE,
    RCS_PRINT_TO_MESSAGE_BOX,	/* Only available for Windows */
    RCS_PRINT_TO_LOGGER		/* Only available for VXWORKS */
#ifdef __cplusplus
};
#else
} RCS_PRINT_DESTINATION_TYPE;
#endif

#ifdef __cplusplus
extern "C" {
#endif

    extern void set_rcs_print_destination(RCS_PRINT_DESTINATION_TYPE);
    extern RCS_PRINT_DESTINATION_TYPE get_rcs_print_destination(void);

    extern int rcs_print_sys_error(int error_source, char *_fmt, ...);

#ifdef __cplusplus
    enum RCS_PRINT_ERROR_SOURCE_TYPE {
#else
    typedef enum {
#endif
	ERRNO_ERROR_SOURCE = 1,
	GETLASTERROR_ERROR_SOURCE
#ifdef __cplusplus
    };
#else
    } RCS_PRINT_ERROR_SOURCE_TYPE;
#endif

#ifdef __cplusplus
};
#endif

extern int max_rcs_errors_to_print;
extern int rcs_errors_printed;

extern char last_error_bufs[4][100];
extern int last_error_buf_filled;

#endif
