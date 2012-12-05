unsigned int rev_code = 1;


int rtapi_module_master_shared_memory_init(rtapi_data_t **rtapi_data) {
    /* get master shared memory block from OS and save its address */
    rtapi_data* = &(rtai_kmalloc(RTAPI_KEY, sizeof(rtapi_data_t)));
    if (rtapi_data* == NULL) {
	rtapi_print_msg(RTAPI_MSG_ERR,
	    "RTAPI: ERROR: could not open shared memory\n");
	return -ENOMEM;
    }

    return 0;
}
