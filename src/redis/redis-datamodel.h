#ifndef REDIS_DATAMODEL_H
#define REDIS_DATAMODEL_H

// data model for LinuxCNC using Redis
// try to stick to "object-type:id:field" notation for key syntax

#define REDIS_BEGIN_TRANSACTION()   redis_cmd("MULTI")
#define REDIS_COMMIT_TRANSACTION()  redis_cmd("EXEC")

// setting a numbered interpreter parameter
#define REDIS_SET_NUMBERED_PARAM(number, value) redis_cmd("SET param:%d:value %f", number, value)

// define a "tool table entry" (has nothing to do with current tool)
#define REDIS_SET_TOOL(toolno, x, y, z, a, b, c, u, v, w,		\
		       diameter,frontangle,backangle,orientation)	\
    REDIS_BEGIN_TRANSACTION();						\
    redis_cmd("HSET tool:%d x %f",toolno, x);				\
    redis_cmd("HSET tool:%d y %f",toolno, y);				\
    redis_cmd("HSET tool:%d z %f",toolno, z);				\
    redis_cmd("HSET tool:%d a %f",toolno, a);				\
    redis_cmd("HSET tool:%d b %f",toolno, b);				\
    redis_cmd("HSET tool:%d c %f",toolno, c);				\
    redis_cmd("HSET tool:%d u %f",toolno, u);				\
    redis_cmd("HSET tool:%d v %f",toolno, v);				\
    redis_cmd("HSET tool:%d w %f",toolno, w);				\
    redis_cmd("HSET tool:%d diameter %f",toolno, diameter);		\
    redis_cmd("HSET tool:%d frontangle %f",toolno, frontangle);		\
    redis_cmd("HSET tool:%d backangle %f",toolno, backangle);		\
    redis_cmd("HSET tool:%d orientation %d",toolno, orientation);	\
    REDIS_COMMIT_TRANSACTION()


#endif
