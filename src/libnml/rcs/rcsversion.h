#ifndef LIBVERSION_H
#define LIBVERSION_H

#define LIB_VERSION "5.1"
#define LIB_MAJOR_VERSION (5)
#define LIB_MINOR_VERSION (1)

const int lib_major_version = LIB_MAJOR_VERSION;
const int lib_minor_version = LIB_MINOR_VERSION;

const static char __attribute__ ((unused)) * rcs_version_info_string =
    "@(#)" " $Info: NML Library version " LIB_VERSION " Compiled on  "
    __DATE__ " at " __TIME__ " for Linux. $ \n";

#endif
