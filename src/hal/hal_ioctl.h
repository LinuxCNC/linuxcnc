
#ifndef HAL_IOCTL_H 
#define HAL_IOCTL_H 

#include <linux/ioctl.h>

#include "hal_refactor.h"

#define MAJORNUM 251	// 0xfc

enum {
/* HALCMD type IOCTL's... */
	HAL_IOCTL_GET_VERSION=1,	// Get HAL Version
	HAL_IOCTL_NEW_SIGNAL,		// Create a new signal
	HAL_IOCTL_DEL_SIGNAL,		// Delete a signal
	HAL_IOCTL_NEW_PART,		// Create a new part
	HAL_IOCTL_DEL_PART,		// Delete a part
	HAL_IOCTL_LINK_PIN_SIGNAL,	// Link a pin to a signal
	HAL_IOCTL_UNLINK_PIN,		// Unlink a pin from a signal
///	HAL_IOCTL_SET_PARAM,		// Set the value of a paramter
//	HAL_IOCTL_GET_PARAM,		// Get the value of a paramter
	HAL_IOCTL_ADD_FUNCTION,		// Add a function to a thread
	HAL_IOCTL_DEL_FUNCTION,		// Remove a function from a thread
	HAL_IOCTL_START,		// Start execution of realtime threads
	HAL_IOCTL_STOP,			// Stop execution of realtime threads

/* IOCTL's required for usermode part operation ... */
	HAL_IOCTL_CREATE_USER_PART,	// Register creation of a user part
	HAL_IOCTL_CREATE_PIN,		// Create a new pin for the user
	HAL_IOCTL_CREATE_PARAM,		// Create a new param for the user
	HAL_IOCTL_CREATE_THREAD,	// Create a new thread for the user
	HAL_IOCTL_CREATE_FUNCTION,	// Create a new function for the user
/* IOCTL's for inspection/management */
	HAL_IOCTL_LIST_MODULES,		// Return a lits of loaded modules
	HAL_IOCTL_LIST_PARTS,		// Return a list of loaded parts
	HAL_IOCTL_GET_PINS,		// Return a list of pins
	HAL_IOCTL_LIST_TYPES,		// Return part type list
	HAL_IOCTL_LIST_SIGNALS	// Return a list of signals
};

/* Set the message of the device driver */


//    _IO    an ioctl with no parameters
//    _IOW   an ioctl with write parameters (copy_from_user)
//    _IOR   an ioctl with read parameters  (copy_to_user)
//    _IOWR  an ioctl with both write and read parameters.

#define IOCTL_GET_VERSION _IOR(MAJORNUM, HAL_IOCTL_GET_VERSION, long *)
#define IOCTL_NEW_SIGNAL _IOW(MAJORNUM, HAL_IOCTL_NEW_SIGNAL, long *)
#define IOCTL_DEL_SIGNAL _IOW(MAJORNUM, HAL_IOCTL_DEL_SIGNAL, long *)
#define IOCTL_NEW_PART _IOW(MAJORNUM, HAL_IOCTL_NEW_PART, long *)
#define IOCTL_DEL_PART _IOW(MAJORNUM, HAL_IOCTL_DEL_PART, long *)
#define IOCTL_LINK_PIN_SIGNAL _IOW(MAJORNUM, HAL_IOCTL_LINK_PIN_SIGNAL, long *)
#define IOCTL_UNLINK_PIN _IOW(MAJORNUM, HAL_IOCTL_UNLINK_PIN, long *)
#define IOCTL_ADD_FUNCTION _IOW(MAJORNUM, HAL_IOCTL_ADD_FUNCTION, long *)
#define IOCTL_DEL_FUNCTION _IOW(MAJORNUM, HAL_IOCTL_DEL_FUNCTION, long *)
#define IOCTL_START _IO(MAJORNUM, HAL_IOCTL_START, long *)
#define IOCTL_STOP _IO(MAJORNUM, HAL_IOCTL_STOP, long *)
#define IOCTL_CREATE_USER_PART _IOW(MAJORNUM, HAL_IOCTL_CREATE_USER_PART, long *)
#define IOCTL_CREATE_PIN _IOW(MAJORNUM, HAL_IOCTL_,CREATE_PIN long *)
#define IOCTL_CREATE_PARAM _IWR(MAJORNUM, HAL_IOCTL_CREATE_PARAM, long *)
#define IOCTL_CREATE_THREAD _IOW(MAJORNUM, HAL_IOCTL_CREATE_THREAD, long *)
#define IOCTL_CREATE_FUNCTION _IOW(MAJORNUM, HAL_IOCTL_CREATE_FUNCTION, long *)
#define	IOCTL_LIST_MODULES _IORW(MAJORNUM, HAL_IOCTL_LIST_MODULES, long *)
#define	IOCTL_LIST_PARTS _IORW(MAJORNUM, HAL_IOCTL_LIST_PARTS, long *)
#define	IOCTL_LIST_TYPES _IORW(MAJORNUM, HAL_IOCTL_LIST_TYPES, long *)
#define	IOCTL_LIST_SIGNALS _IORW(MAJORNUM, HAL_IOCTL_LIST_SIGNALS, long *)
#define	IOCTL_GET_PINS _IORW(MAJORNUM, HAL_IOCTL_GET_PINS, long *)


/* The name of the device file */
#define DEVICE_FILE_NAME "hal_dev"

//	HAL_IOCTL_GET_VERSION,		// Get HAL Version
// param =  *long to the storage location where the version will be placed.


//	HAL_IOCTL_NEW_SIGNAL,		// Create a new signal
// param = 

typedef struct HAL_IOCTL_NEW_SIGNAL_PARAM {
	char name[HAL_NAME_LEN];	// from user to kernel
	hal_type_t type;		// from user to kernel
	} NEW_SIGNAL_PARAM;

//	HAL_IOCTL_DEL_SIGNAL,		// Delete a signal
// param = char name[HAL_NAME_LEN]


//	HAL_IOCTL_NEW_PART,		// Create a new part
typedef struct HAL_IOCTL_NEW_PART {
	char module[HAL_NAME_LEN];
	char part_type[HAL_NAME_LEN];
	char name[HAL_NAME_LEN];
} NEW_PART_PARAM;

//	HAL_IOCTL_DEL_PART,		// Delete a part
// param = the id of the part to delete

//	HAL_IOCTL_LINK_PIN_SIGNAL,	// Link a pin to a signal
// param =
typedef struct HAL_IOCTL_LINK_PART {
	int part_id;
	char pin_name[HAL_NAME_LEN];
	char signal_name[HAL_NAME_LEN];
} LINK_PART_PARAM;

//	HAL_IOCTL_UNLINK_PIN,		// Unlink a pin from a signal
// param =
typedef struct HAL_IOCTL_UNLINK_PIN {
	int part_id;
	char pin_name[HAL_NAME_LEN];
} UNLINK_PART_PARAM;

///	HAL_IOCTL_SET_PARAM,		// Set the value of a paramter
//	HAL_IOCTL_GET_PARAM,		// Get the value of a paramter
//	HAL_IOCTL_ADD_FUNCTION,		// Add a function to a thread
//	HAL_IOCTL_DEL_FUNCTION,		// Remove a function from a thread

//	HAL_IOCTL_START,		// Start execution of realtime threads
// param = None

//	HAL_IOCTL_STOP,			// Stop execution of realtime threads
// param = None

/* IOCTL's required for usermode part operation ... */
//	HAL_IOCTL_CREATE_USER_PART,	// Register creation of a user part
// param = *hal_part_t


//	HAL_IOCTL_CREATE_PIN,		// Create a new pin for the user
//	HAL_IOCTL_CREATE_PARAM,		// Create a new param for the user
//	HAL_IOCTL_CREATE_THREAD,	// Create a new thread for the user
//	HAL_IOCTL_CREATE_FUNCTION,	// Create a new function for the user

//	HAL_IOCTL_LIST_MODULES,         // Return a lits of loaded modules
// param =

typedef struct HAL_IOCTL_NAME_ITEM {
	int id;
	char name[HAL_NAME_LEN];
} HAL_IOCTL_NAME_ITEM;

typedef struct HAL_IOCTL_NAME_LIST {
	int count;
	int start;
	HAL_IOCTL_NAME_ITEM *items;
} HAL_IOCTL_NAME_LIST;

//	HAL_IOCTL_GET_PINS,		// Return a list of pins
// param =

typedef struct HAL_IOCTL_GET_PINS_LIST {
	int count;		// number of pins to return
	int start;		// start pin to return
	int part_id;		// id of the part to inspect.
	hal_pin_t *pins;	// location to put the data...
} HAL_IOCTL_GET_PINS_LIST;

	// on entry, count is set to the sie of the items array.
	// on return, count contains the number of items copied into the array
	// start contains the in-order offset of the item you would like
	// to start with

///	HAL_IOCTL_SET_PARAM,		// Set the value of a paramter

//	HAL_IOCTL_LIST_PARTS,           // Return a list of loaded parts
//	HAL_IOCTL_LIST_TYPES,           // Return part type list
//	HAL_IOCTL_LIST_SIGNALS.    // Return a list of signals
// param=
typedef struct HAL_IOCTL_SIGNAL_LIST {
	int count;
	int start;
	hal_signal_t *items;
} HAL_IOCTL_SIGNAL_LIST;



#endif
