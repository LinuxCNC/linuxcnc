
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

int hal_init_procfs()
{
	hal_procfs_root=proc_mkdir("hal", NULL);

	hal_procfs_components=create_proc_entry("show_components", S_IRUSR, hal_procfs_root);
	hal_procfs_components->read_proc = hal_procfs_read_components;

return 0;
}

void hal_shutdown_procfs()
{
	remove_proc_entry("components", hal_procfs_root);
	remove_proc_entry("hal", NULL);
}

#endif // PROCFS
#endif // RTAI
