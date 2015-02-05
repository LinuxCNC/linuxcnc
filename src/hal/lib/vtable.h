#ifndef _VTABLE_H
#define _VTABLE_H


// whenever a new type of/new layout of a vtable is exported,
// add a unique signature here
// this signature is used to match up exporting and referencing code

typedef enum {
    VTKINEMATICS_VERSION1 = 1000,

    VTTP_VERSION1 = 2000,
} vtable_t;

#endif // _VTABLE_H
