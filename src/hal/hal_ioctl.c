
#ifndef RTAPI
#error This is a realtime component only!
#endif

#include <linux/ctype.h>	/* isspace() */
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include <math.h>

#include "hal_ioctl.h"

#ifdef MODULE
/* module information */
MODULE_AUTHOR("Jonathan Stark");
MODULE_DESCRIPTION("ioctl interface to the new hal");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* MODULE */



#include <linux/fs.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>

void kernel_to_user(void *kernelmem, void *usermem, int count)
{
int c;
    for (c=0; c<count; c++)
        put_user(*(( (char *)kernelmem)+c), ((char *)usermem)+c);
}

void user_to_kernel(void *usermem, void *kernelmem, int count)
{
int c;
    for (c=0; c<count; c++)
        get_user(*((char *)kernelmem+c), (char *)usermem+c);
}

static int device_close(struct inode *inode, 
                       struct file *file)
{
  rtapi_print_msg(RTAPI_MSG_DBG,
  	"HAL: /dev/hal device_close(%p)\n", file);

//  if (open)
//    return -EBUSY;

  MOD_DEC_USE_COUNT;

  return 0;
}


static int device_open(struct inode *inode, 
                       struct file *file)
{
  rtapi_print_msg(RTAPI_MSG_DBG,
  	"HAL: /dev/hal device_open(%p)\n", file);

//  if (open)
//    return -EBUSY;

  MOD_INC_USE_COUNT;

  return 0;
}

void copy_name_id_to_user(HAL_IOCTL_NAME_ITEM *useritem,
	int kernelid, char *kernelname)
{
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: Sending %d/%s to user at %p!\n", kernelid, kernelname, useritem);
	kernel_to_user(kernelname, useritem->name, HAL_NAME_LEN);
	kernel_to_user(&kernelid, &useritem->id, sizeof(int));
}


void copy_pin_to_user(hal_pin_t *kernelpin, hal_pin_t *userpin)
{
//    rtapi_print_msg(RTAPI_MSG_DBG,
//	"HAL: Sending pin %p to user %p [%d] - %s!\n", 
//		kernelpin, userpin, sizeof(hal_pin_t), kernelpin->name);
	kernel_to_user(kernelpin, userpin, sizeof(hal_pin_t));
/* TODO .. fix shared memory links in pin... */

}

int send_modules(HAL_IOCTL_NAME_LIST *list)
{
HAL_IOCTL_NAME_LIST kernellist;
int count;
int start;
hal_module_list_t *module;

    user_to_kernel(list, &kernellist, sizeof(HAL_IOCTL_NAME_LIST));
    start=kernellist.start;

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: Requested to send modules from %d, max %d!\n", start,
	kernellist.count);
    module=global_module_list;
       
    while (module && start)
	{
	module=module->next;
	start--;
	} 

    for (count=0; (count<kernellist.count) && module; count++)
	{
	copy_name_id_to_user(&kernellist.items[count],
		module->module_id, module->module_info->name);
	module=module->next;
	}
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: Setting user count to %d!\n", count);
    kernel_to_user(&count, &list->count, sizeof(int));
return HAL_SUCCESS;
}

int send_pins(HAL_IOCTL_GET_PINS_LIST *list)
{
HAL_IOCTL_GET_PINS_LIST kernellist;
int count;
int start;
hal_part_t **handle;
hal_part_t *part;
hal_pin_t *pin;
hal_pin_t *userpin;

    user_to_kernel(list, &kernellist, sizeof(HAL_IOCTL_GET_PINS_LIST));
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: Requested to send pins from %d, max %d on part %d!\n", 
	kernellist.start, kernellist.count, kernellist.part_id);

    start=kernellist.start;

    handle=find_part_by_id(kernellist.part_id);
    part=*handle;
    if (!part)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: get_pins: Requested part %d does not exist!\n", kernellist.part_id);
	return (HAL_INVAL);
	}

    pin=part->pins;
    
    while (start && pin)
	{
	pin=pin->next;
	start--;
	} 

    userpin=kernellist.pins;
    for (count=0; (count<kernellist.count) && pin; count++)
	{
        copy_pin_to_user(pin, &kernellist.pins[count]);
//sizeof(hal_pin_t));
	pin=pin->next;
	}
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: Setting user count to %d!\n", count);
    kernel_to_user(&count, &list->count, sizeof(int));
return HAL_SUCCESS;
}

int send_parts(HAL_IOCTL_NAME_LIST *list)
{
HAL_IOCTL_NAME_LIST kernellist;
int count;
int start;
hal_part_t *part;

    user_to_kernel(list, &kernellist, sizeof(HAL_IOCTL_NAME_LIST));
    start=kernellist.start;

    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: Requested to send parts from %d, max %d!\n", start,
	kernellist.count);
    part=global_part_list;
       
    while (part && start)
	{
	part=part->next;
	start--;
	} 

    for (count=0; (count<kernellist.count) && part; count++)
	{
	copy_name_id_to_user(&kernellist.items[count],
		part->part_id, part->name);
	part=part->next;
	}
    rtapi_print_msg(RTAPI_MSG_DBG,
	"HAL: Setting user count to %d!\n", count);
    kernel_to_user(&count, &list->count, sizeof(int));
return HAL_SUCCESS;
}


int device_ioctl(
    struct inode *inode,
    struct file *file,
    unsigned int ioctl,/* The number of the ioctl */
    unsigned long p) /* The parameter to it */
{
  NEW_SIGNAL_PARAM 	signal_pb;
  NEW_PART_PARAM 	part_pb;
  LINK_PART_PARAM	link_pb;
  UNLINK_PART_PARAM	unlink_pb;
  int result=HAL_INVAL;
  int id;
  void *param=(void *)p;
//  hal_pin_t *newpin;
//  hal_part_t *newpart;

  long version=65816;

	rtapi_print_msg(RTAPI_MSG_DBG,
	    "HAL: handling ioctl %d!\n", ioctl);

  /* Switch according to the ioctl called */
  switch (ioctl) 
	{
        case HAL_IOCTL_GET_VERSION:        // Get HAL Version
	    kernel_to_user(&version, param, sizeof(long));
	    result=HAL_SUCCESS;
	    break;
        case HAL_IOCTL_NEW_SIGNAL:           // Create a new signal
	    user_to_kernel(param, &signal_pb, sizeof(NEW_SIGNAL_PARAM));
 	    result=hal_signal_new(signal_pb.name, signal_pb.type);
	    break;
        case HAL_IOCTL_DEL_SIGNAL:           // Delete a signal
	    user_to_kernel(param, signal_pb.name, HAL_NAME_LEN);
	    signal_pb.name[HAL_NAME_LEN-1]=0;
	    result=hal_signal_delete(signal_pb.name);
	    break;
        case HAL_IOCTL_NEW_PART:             // Create a new part
	    user_to_kernel(param, &part_pb, sizeof(NEW_PART_PARAM));
	    result=hal_create_part_by_names(part_pb.module,
		part_pb.part_type, part_pb.name);
	    break;
        case HAL_IOCTL_DEL_PART:             // Delete a part
	    id=p;
	    result=hal_remove_part(id);
	    break;
        case HAL_IOCTL_LINK_PIN_SIGNAL:      // Link a pin to a signal
	    user_to_kernel(param, &link_pb, sizeof(LINK_PART_PARAM));
	    link_pb.pin_name[HAL_NAME_LEN-1]=0;
	    link_pb.signal_name[HAL_NAME_LEN-1]=0;
	    result=link_pin_by_name(link_pb.part_id,
		link_pb.pin_name, link_pb.signal_name); 
	    break;
        case HAL_IOCTL_UNLINK_PIN:           // Unlink a pin from a signal
	    user_to_kernel(param, &unlink_pb, sizeof(UNLINK_PART_PARAM));
	    unlink_pb.pin_name[HAL_NAME_LEN-1]=0;
	    result=unlink_pin_by_name(unlink_pb.part_id,
		unlink_pb.pin_name);
	    break;
        case HAL_IOCTL_ADD_FUNCTION:         // Add a function to a thread
        case HAL_IOCTL_DEL_FUNCTION:         // Remove a function from a thread
	    break;
        case HAL_IOCTL_START:                // Start execution of realtime threads
	    result=hal_start_threads();
	    break;
        case HAL_IOCTL_STOP:                 // Stop execution of realtime threads
	    result=hal_stop_threads();
	    break;

/* IOCTL's required for usermode part operation ... */
        case HAL_IOCTL_CREATE_USER_PART:     // Register creation of a user part
//	    newpart=hal_create_part()
//	    kernel_to_user(newpart, param, sizeof(hal_part_t));
	    break;
        case HAL_IOCTL_CREATE_PIN:           // Create a new pin for the user
//	    newpin=alloc_new_pin();
//	    kernel_to_user(newpin, param, sizeof(hal_pin_t));
	    break;
        case HAL_IOCTL_CREATE_PARAM:         // Create a new param for the user
	    break;
        case HAL_IOCTL_CREATE_THREAD:        // Create a new thread for the user
	    break;
        case HAL_IOCTL_CREATE_FUNCTION:      // Create a new function for the user
	    break;

	case HAL_IOCTL_LIST_MODULES:         // Return a lits of loaded modules
	    result=send_modules(param);
	    break;
	case HAL_IOCTL_LIST_PARTS:           // Return a list of loaded parts
	    result=send_parts((HAL_IOCTL_NAME_LIST *)param);
	    break;
	case HAL_IOCTL_LIST_TYPES:		// Return part type list
	    break;
	case HAL_IOCTL_LIST_SIGNALS:		// Return a list of signals
	    break;
	case HAL_IOCTL_GET_PINS:		// Return a list of pins
	    result=send_pins(param);
	    break;

	default:
		return EINVAL;
	}

  return result;
}

struct file_operations Fops = {
  llseek: NULL,	  /* llseek */
  read: NULL,	  /* read */
  write: NULL,   /* write */
  readdir: NULL,   /* readdir */
  ioctl: device_ioctl,   /* ioctl */
  mmap: NULL,   /* mmap */
  open: device_open,  /* open */
  flush: NULL,  /* flush */
  release: device_close /*close */
};


int rtapi_app_main(void)
{
int result;
  /* Register the character device (atleast try) */
  result = register_chrdev(MAJORNUM, 
                                 DEVICE_FILE_NAME,
                                 &Fops);

  /* Negative values signify an error */
  if (result < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "HAL: unable to register character device %d!\n", MAJORNUM);
	}
return 0;
} 


void rtapi_app_exit(void)
{
int result;

    result=unregister_chrdev(MAJORNUM, DEVICE_FILE_NAME);

}


