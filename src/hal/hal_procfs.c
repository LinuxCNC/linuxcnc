
#ifdef RTAI

#include "hal_procfs.h"

#ifdef PROCFS
#include <linux/proc_fs.h>

struct proc_dir_entry *hal_procfs_root;
struct proc_dir_entry *hal_procfs_components;

int hal_procfs_read_components(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
int len=0;
hal_module_list_t **module_handle;

//	hal_data->comp_list_ptr;

/* Walk the linked list of module_list_t's until we reach the end... */
    for (module_handle=&global_module_list; 
	(*module_handle && (count-len >0));
        module_handle=&( (*module_handle)->next ) )
	{
	len+=snprintf(page+off, count-len, "%s\n", 
		(*module_handle)->module.module_name);
	}
return(len);
}

int hal_procfs_read_blocks(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
int len=0;
hal_block_list_t **block_handle;

    for (block_handle=&global_block_list; 
	(*block_handle && (count-len >0));
        block_handle=&( (*block_handle)->next ) )
	{
	len+=snprintf(page+off, count-len, "%d: %s %s\n", 
		(*block_handle)->block_id,
		(*block_handle)->module->module.module_name,
		(*block_handle)->block_type->block.type_name);
	}
return(len);
}

#define MAX_COMMAND_LENGTH 100

int hal_procfs_write_create(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
int len=count;
int result;

static char command[MAX_COMMAND_LENGTH];
char *type=NULL;
int offset;

    if (len>MAX_COMMAND_LENGTH)
	len=MAX_COMMAND_LENGTH;

    strncpy(command, buffer, len);

    for (offset=0; offset<len; offset++)
	{
	if (command[offset]==' ')
		{
		command[offset]=0;
		type=command+offset+1;
		break;
		}
	}

    for (offset=0; offset<len; offset++)
	if (command[offset]=='\n')
	    command[offset]=0;

    if (!type)
	{
        rtapi_print_msg(RTAPI_MSG_ERR,
         	"hal_procfs_create: No type specifier found in proc create write\n");
	return -EINVAL;
	}

    command[MAX_COMMAND_LENGTH-1]=0;

    result=hal_create_block_by_names(command, type);
    if (HAL_SUCCESS != result)
	return -EINVAL;

return(len);
}


int hal_init_procfs()
{
	hal_procfs_root=proc_mkdir("hal", NULL);

	hal_procfs_components=create_proc_entry("show_components", S_IRUSR, hal_procfs_root);
	hal_procfs_components->read_proc = hal_procfs_read_components;


	hal_procfs_components=create_proc_entry("show_blocks", S_IRUSR, hal_procfs_root);
	hal_procfs_components->read_proc = hal_procfs_read_blocks;


	hal_procfs_components=create_proc_entry("create", S_IWUSR, hal_procfs_root);
	hal_procfs_components->write_proc = hal_procfs_write_create;
return 0;
}

void hal_shutdown_procfs()
{
	remove_proc_entry("show_components", hal_procfs_root);
	remove_proc_entry("show_blocks", hal_procfs_root);
	remove_proc_entry("hal", NULL);
}

#endif // PROCFS
#endif // RTAI
