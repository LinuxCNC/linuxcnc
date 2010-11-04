/********************************************************************
* Description: rcs_print.hh
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
********************************************************************/
#ifndef RCS_PRNT_HH
#define RCS_PRNT_HH


#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>		/* va_list */

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
class LinkedList;

extern LinkedList *get_rcs_print_list();
  /* Returns the address of the linked list where messages may have been
     stored. */
#endif

#ifdef __cplusplus
extern "C" {
#endif

    extern void clean_print_list(void);
    /* Deletes the linked list where messages may have been stored. */

    extern void output_print_list(int output_func(const char *));
    extern int count_characters_in_print_list(void);
    extern int count_lines_in_print_list(void);
    extern void convert_print_list_to_lines(void);
    extern void update_lines_table(void);
    extern int rcs_vprint(const char *_fmt, va_list va_args, int save_string);
    /* Prints a message using the _fmt format string and the _va_args using
       the vprintf conventions. */

    extern int rcs_print(const char *_fmt, ...) __attribute__((format(printf,1,2)));
    /* 
       Prints a message using the _fmt format string and optional additional
       arguments using the printf conventions. */

    extern int rcs_print_debug(long, const char *_fmt, ...) __attribute__((format(printf,2,3)));
    /* 
       Prints a message using the _fmt format string and optional additional
       arguments using the printf conventions if the corresponding flag to
       _flag_to_check is set. (See set_rcs_print_flag().) */
#ifdef DO_NOT_USE_RCS_PRINT_ERROR_NEW
    extern int rcs_print_error(const char *_fmt, ...) __attribute__((format(printf,1,2)));
#else
    extern int set_print_rcs_error_info(const char *file, int line);
    extern int print_rcs_error_new(const char *_fmt, ...) __attribute__((format(printf,1,2)));
#define rcs_print_error set_print_rcs_error_info( __FILE__, __LINE__); print_rcs_error_new
#endif

    extern void set_rcs_print_flag(long flags_to_set);
    /* An internal 32 bit integer contains a set of flags that are checked
       whenever an rcs_print_debug or rcs_print_error occurs to determine if
       the message should be printed or not. Programmers can define their own 
       flags in the most significant byte or turn on or off several NODE or
       CMS/NML debug messages.

       See the Print MODE flags defined below for messages that can be turned 
       on or off. */

    extern void clear_rcs_print_flag(long flags_to_set);
    /* Clears a flag set with set_rcs_print_flag. */

    extern char *strip_control_characters(char *_dest, char *_src);
    /* Removes new lines, carriage returns and tabs from the _src string and
       stores the result in the _dest string if the _dest pointer does not
       equal NULL. If the _dest pointer equals NULL the new string is stored
       in an internal array.

       Returns the dest pointer or the address of the internal array where
       the new string was stored. */

    extern int separate_words(char **_dest, int _max, char *_src);
    extern int rcs_puts(const char *);
    /* Prints the string _str and adds a new line character at the end
       following the puts convention. */

    extern int rcs_fputs(const char *);
    extern char **get_rcs_lines_table(void);
    extern int get_rcs_print_list_size(void);
    typedef void (*RCS_PRINT_NOTIFY_FUNC_PTR) (void);
    extern void set_rcs_print_notify(RCS_PRINT_NOTIFY_FUNC_PTR);
    extern int set_rcs_print_file(const char *_file_name);
    extern void close_rcs_printing(void);

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
    /* Changes where the output of the rcs_print functions is directed. The
       following choices are available:

       RCS_PRINT_TO_STDOUT Print to stdout.

       RCS_PRINT_TO_LOGGER Currently prints to stdout, except under VxWorks
       where it uses the logMsg function, which is non-blocking.

       RCS_PRINT_TO_STDERR Print to stderr

       RCS_PRINT_TO_NULL Make all rcs_print functions return without doing
       anything.

       RCS_PRINT_TO_LIST Store all rcs_print messages in a linked list, so
       that later they can be displayed in a separate window, or used in some 
       other way. The current list sizing mode defaults to a maximum size of
       256 with excess nodes being deleted from the head. */

    extern RCS_PRINT_DESTINATION_TYPE get_rcs_print_destination(void);
    extern int rcs_print_sys_error(int error_source, const char *_fmt, ...) __attribute__((format(printf,2,3)));

#ifdef __cplusplus
    enum RCS_PRINT_ERROR_SOURCE_TYPE {
#else
    typedef enum {
#endif
	ERRNO_ERROR_SOURCE = 1,
	GETLASTERROR_ERROR_SOURCE,
	WSAGETLASTERROR_ERROR_SOURCE
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
