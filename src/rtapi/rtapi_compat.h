#ifndef RTAPI_COMPAT_H
#define RTAPI_COMPAT_H

// basic features of thread flavors. Needed to init the flavor
// descriptor in rtapi_compat.h and rtapi_switch->flavor_flags

#define POSIX_FLAVOR_FLAGS                 0

#define RTPREEMPT_FLAVOR_FLAGS             (FLAVOR_DOES_IO)

#define RTAI_KERNEL_FLAVOR_FLAGS           (FLAVOR_DOES_IO| \
					    FLAVOR_KERNEL_BUILD|\
					    FLAVOR_RTAPI_DATA_IN_SHM)

#define XENOMAI_KERNEL_FLAVOR_FLAGS        (FLAVOR_DOES_IO|\
					    FLAVOR_KERNEL_BUILD|\
					    FLAVOR_RTAPI_DATA_IN_SHM)

#define XENOMAI_FLAVOR_FLAGS               (FLAVOR_DOES_IO)

#endif // RTAPI_COMPAT_H
