#ifndef HAL_PRIV_H
#define HAL_PRIV_H

#define PROCFS

/*
  hal_module_info

  Info required to register a new module with the hal subsystem

  module_name - the name of the module (ie, 'motmod')
  author - the name of the person responsible (duck) for the module
  short_description - what this module does
  info_link - a URL or email address with where to get more information/help
*/

/** These status codes are returned by many HAL functions. */

#define HAL_SUCCESS       0     /* call successfull */
#define HAL_UNSUP        -1     /* function not supported */
#define HAL_BADVAR       -2     /* duplicate or not-found variable name */
#define HAL_INVAL        -3     /* invalid argument */
#define HAL_NOMEM        -4     /* not enough memory */
#define HAL_LIMIT        -5     /* resource limit reached */
#define HAL_PERM         -6     /* permission denied */
#define HAL_BUSY         -7     /* resource is busy or locked */
#define HAL_NOTFND       -8     /* object not found */
#define HAL_FAIL         -9     /* operation failed */


#define HAL_NAME_LEN	31
#define HAL_SIZE  65500              /* so it fits inside 64K */

#define HAL_KEY   0x48414C21 /* key used to open HAL shared memory */

#define HAL_STACKSIZE 16384  /* realtime task stacksize */

/* Use these for x86 machines, and anything else that can write to
   individual bytes in a machine word. */
typedef volatile unsigned char 	hal_bit_t;
typedef volatile unsigned char 	hal_u8_t;
typedef volatile signed char 	hal_s8_t;
typedef volatile unsigned short hal_u16_t;
typedef volatile signed short 	hal_s16_t;
typedef volatile unsigned long 	hal_u32_t;
typedef volatile signed long 	hal_s32_t;
typedef volatile float 		hal_float_t;


typedef enum {
    HAL_BIT = 1,
    HAL_FLOAT = 2,
    HAL_S8 = 3,
    HAL_U8 = 4,
    HAL_S16 = 5,
    HAL_U16 = 6,
    HAL_S32 = 7,
    HAL_U32 = 8
} hal_type_t;

typedef enum {
    HAL_RD = 16,
    HAL_WR = 32,
    HAL_RD_WR = (HAL_RD | HAL_WR),
} hal_dir_t;


typedef struct hal_shm_signal_t {
    long signal;		/* the actual real signal data */
} hal_shm_signal_t;


typedef struct hal_signal_t {
    struct hal_signal_t	*next;		/* next signal in linked list */
    hal_shm_signal_t	*shm_data; 	/* pointer to signal value */
    hal_type_t 		type;		/* data type */
    char 		name[HAL_NAME_LEN + 1];	/* signal name */
    int 		readers;	/* number of read pins linked */
    int 		writers;	/* number of write pins linked */
} hal_signal_t;


typedef struct hal_parameter_t{
    struct hal_parameter_t *next;		/* next paramter in list */
    hal_shm_signal_t	*shm_data; 	/* pointer to parameter value */
    hal_type_t 		type;		/* data type */
    char 		name[HAL_NAME_LEN + 1];	/* signal name */
    hal_dir_t		dir;		/* direction */
} hal_parameter_t;



typedef struct hal_pin_t {
    struct hal_pin_t	*next; 		/* next pin in linked list */

					/* pointer to the pointer to the data*/
					/* TODO... this data must be shmem*/
    hal_shm_signal_t	**pin_handle;	
    hal_shm_signal_t	dummysignal;	/* dummy signal when not linked */
    hal_signal_t	*signal;	/* signal to which pin is linked */
    hal_type_t 		type;		/* data type */
    hal_dir_t 		dir;		/* pin direction */
    char name[HAL_NAME_LEN + 1];	/* pin name */
} hal_pin_t;


typedef struct hal_function_t {
    struct hal_function_t	*next;	/* linked list to the next function */
    int 		uses_fp;	/* floating point flag */
    int 		reentrant;	/* non-zero if function is re-entrant */
    int users;				/* number of threads using function */
    void *arg;				/* argument for function */
    void (*function) (void *, long);	/* ptr to function code */
    hal_s32_t runtime;			/* duration of last run, in nsec */
    hal_s32_t maxtime;			/* duration of longest run, in nsec */
    char name[HAL_NAME_LEN + 1];	/* function name */
} hal_function_t;



typedef struct hal_part_type_info
{
	int 		type_id;	// A unique (to this module) block id
	const char 	*name;		// Name of the part type
	const char 	*description;	// Description of what the part does
	int (*create)(int new_part_id, int part_type_id);
} hal_part_type_info;


 
typedef struct hal_part_type_list_t {
    hal_part_type_info 	*part;		/* Data from above */
    struct 		hal_part_type_list_t	
			*next;		/* linked list to next item in list */
} hal_part_type_list_t; 


typedef struct hal_module_info
  {
  	char 		*name;		// Name of the module
  	char 		*author;	// Author who created this module
  	char 		*description;	// Description of what the module does
  	char 		*info_link;	// email and/or web reference info
  } hal_module_info;

typedef struct hal_module_list_t {
    struct hal_module_list_t	*next;	/* linked list to next item in list */
    hal_module_info	*module_info;
    hal_part_type_list_t *part_types;	/* Types this module can create*/
    int			module_id;	/* module identifier */
} hal_module_list_t;


typedef struct hal_part_t {
    int part_id;			/* ID for tracking memory and linking */
    hal_module_list_t	*module;	/* module for memory tracking	*/
    hal_part_type_list_t *type; 	/* pointer to the type of this part */
    struct hal_part_t	*next;		/* next part in the list */
    char name[HAL_NAME_LEN + 1];	/* component name */
    hal_pin_t		*pins;		/* linked list of pins on this part */
    hal_function_t	*functions;	/* functions on this part */
    hal_parameter_t	*parameters;	/* parameters on this part */
} hal_part_t;


typedef struct hal_function_entry_t {
    hal_function_t	*function;	/* pointer to the object */
    struct hal_function_entry_t	
			*next;		/* pointer to next item */
} hal_function_entry_t;


typedef struct hal_thread_t{
    struct hal_thread_t	*next;		/* linked list to next item in list */
    int uses_fp;			/* floating point flag */
    long int period;			/* period of the thread, in nsec */
    int priority;			/* priority of the thread */
    int task_id;			/* ID of the task that runs this thread */
    hal_s32_t runtime;			/* duration of last run, in nsec */
    hal_s32_t maxtime;			/* duration of longest run, in nsec */
    hal_function_entry_t *function_list;	/* list of functions to run */
    char name[HAL_NAME_LEN + 1];	/* thread name */
} hal_thread_t;


typedef struct {
    unsigned long mutex;        /* protection for linked lists, etc. */
    hal_s32_t shmem_avail;      /* amount of shmem left free */
    void *shmem_bot;              /* bottom of free shmem (first free byte) */
    void *shmem_top;              /* top of free shmem (1 past last free) */

    long base_period;           /* timer period for realtime tasks */
    int threads_running;        /* non-zero if threads are started */

} hal_data_t;


typedef struct hal_memory_t {
 	void *start;
	void *allocated;	// address we give the client...
	int size;
	struct hal_memory_t *next;
} hal_memory_t;


/* FUNCTIONS */

int hal_pin_new(char *name, hal_type_t type, hal_dir_t dir,
    void **data_ptr_addr, int part_id);


#ifdef PROCFS


#endif




int hal_init_procfs(void);
void hal_shutdown_procfs(void);


int hal_register_part_type(int module_id, hal_part_type_info *part_info);

int hal_remove_part(int part_id);



int hal_param_new(char *name, hal_type_t type, hal_dir_t dir, void *data_addr,
    int part_id);

int hal_param_set(char *name, hal_type_t type, void *value_addr);

int hal_signal_new(char *name, hal_type_t type);
int unlink_pin_by_name(int part_id, char *pin_name);
int link_pin_by_name(int part_id, const char *pin_name, const char *signal_name)
;
int hal_create_part_by_names(char *module_name, char *part_type_name, const char *part_name);
int hal_signal_delete(char *name);

int hal_start_threads(void);
int hal_stop_threads(void);

// static void init_hal_data(void);

extern hal_module_list_t       *global_module_list;
extern hal_part_t              *global_part_list;
extern hal_signal_t            *global_signal_list;
extern hal_thread_t            *global_thread_list;
extern hal_function_t          *global_function_list;
extern hal_part_t **find_part_by_id(int part_id);

#endif

