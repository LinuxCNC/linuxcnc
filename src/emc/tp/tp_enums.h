#ifndef TP_ENUMS_H
#define TP_ENUMS_H

/**
 * TP return codes.
 * This enum is a catch-all for useful return statuses from TP
 * internal functions. This may be replaced with a better system in
 * the future.
 */
typedef enum {
    TP_ERR_UNRECOVERABLE = -10,
    TP_ERR_INVALID = -9,
    TP_ERR_INPUT_TYPE = -8,
    TP_ERR_TOLERANCE = -7,
    TP_ERR_RADIUS_TOO_SMALL = -6,
    TP_ERR_GEOM = -5,
    TP_ERR_RANGE = -4,
    TP_ERR_MISSING_OUTPUT = -3,
    TP_ERR_MISSING_INPUT = -2,
    TP_ERR_FAIL = -1,
    TP_ERR_OK = 0,
    TP_ERR_NO_ACTION,
    TP_ERR_SLOWING,
    TP_ERR_STOPPED,
    TP_ERR_WAITING,
    TP_ERR_ZERO_LENGTH,
    TP_ERR_REVERSE_EMPTY,
    TP_ERR_LAST
} tp_err_t;

typedef enum {
    TC_INTERSECT_INCOMPATIBLE=-1,
    TC_INTERSECT_TANGENT,
    TC_INTERSECT_NONTANGENT,
} TCIntersectType;

typedef enum {
    UPDATE_NORMAL,
    UPDATE_PARABOLIC_BLEND,
    UPDATE_SPLIT
} UpdateCycleMode;

/**
 * Describes blend modes used in the trajectory planner.
 * @note these values are used as array indices, so make sure valid options
 * start at 0 and increase by one.
 */
typedef enum {
    NO_BLEND = -1,
    PARABOLIC_BLEND,
    TANGENT_SEGMENTS_BLEND,
    ARC_BLEND
} tc_blend_type_t;



#endif // TP_ENUMS_H
