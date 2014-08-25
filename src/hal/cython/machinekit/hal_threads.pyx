

def start_threads():
    hal_required()
    r = hal_start_threads()
    if r:
        raise RuntimeError("hal_start_threads failed: %d %s" % (r, hal_lasterror()))


def stop_threads():
    hal_required()
    r = hal_stop_threads()
    if r:
        raise RuntimeError("hal_stop_threads failed: %d %s" % (r, hal_lasterror()))
