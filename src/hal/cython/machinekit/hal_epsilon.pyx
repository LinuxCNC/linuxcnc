# Epsilon pseudo array

cdef class Epsilon:

    def __setitem__(self, int n, double value):
        hal_required()
        if (n < 0) or (n > MAX_EPSILON-1):
            raise RuntimeError("epsilon index out of range: 0..%d" % (MAX_EPSILON-1))
        hal_data.epsilon[n] = value

    def __getitem__(self, int n):
        hal_required()
        if (n < 0) or (n > MAX_EPSILON-1):
            raise RuntimeError("epsilon index out of range: 0..%d" % (MAX_EPSILON-1))
        return hal_data.epsilon[n]

    def __len__(self):
        return  MAX_EPSILON

    def __call__(self):
        hal_required()
        values = []
        for i in range(MAX_EPSILON):
            values.append(hal_data.epsilon[i])
        return values

epsilon = Epsilon()
