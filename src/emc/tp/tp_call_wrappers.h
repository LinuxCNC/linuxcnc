#ifndef TP_CALL_WRAPPERS_H
#define TP_CALL_WRAPPERS_H

#include "tp_enums.h"

#define CH_ERR(expect__, mycall__) \
do {\
    int expt = expect__; \
    int res = mycall__; \
    if (expt != res) { \
        tp_debug_print("%s failed with %d (expect %d) at %s:%d\n", #mycall__, res, expt, __FUNCTION__, __LINE__); \
        return res; \
    } \
} while (0)

#define CHP(mycall__) CH_ERR(TP_ERR_OK, mycall__)


#endif // TP_CALL_WRAPPERS_H
