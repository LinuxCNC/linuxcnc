allowed_float_error = 1.0e-6

def fnear(f1,f2):
    global allowed_float_error
    return abs(f1 - f2) <= allowed_float_error
