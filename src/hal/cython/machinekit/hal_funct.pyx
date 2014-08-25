

def addf(char *func, char *thread, int position=-1):
    hal_required()
    r = hal_add_funct_to_thread(func, thread, position)
    if r:
        raise RuntimeError("hal_add_funct_to_thread failed: %d  %s" % (r, hal_lasterror()))


def delf(const char *funct_name, const char *thread_name):
    hal_required()
    r = hal_del_funct_from_thread(funct_name,thread_name)
    if r:
        raise RuntimeError("hal_del_funct_from_thread failed: %d %s" % (r, hal_lasterror()))
