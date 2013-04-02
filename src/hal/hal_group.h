#ifndef HAL_GROUP_H
#define HAL_GROUP_H

#include <rtapi.h>
RTAPI_BEGIN_DECLS

// visible - locks then hal mutex
extern int halpr_group_new(const char *group, int id, int arg1, int arg2);
extern int halpr_group_delete(const char *group);

extern int halpr_member_new(const char *group, const char *member, int arg1, double epsilon);
extern int halpr_member_delete(const char *group, const char *member);

// group iterator
// for each defined group, call the callback function iff:
// - a NULL group argument is give: this will cause all groups to be visited.
// - a non-null group argument will visit exactly that group with an exact name match (no prefix matching).
// - if the group was found, 1 is returned, else 0.
// cb_data can be used in any fashion and it is not inspected.

// callback return values:
//    0:   this signals 'continue iterating'
//    >0:  this signals 'stop iteration and return count'
//    <0:  this signals an error. Stop iterating and return the error code.
// if iteration runs to completion and the callback has never returned a
// non-zero value, halpr_foreach_group returns the number of groups visited.

typedef int(*hal_group_callback_t)(hal_group_t *group,  void *cb_data);
extern int halpr_foreach_group(const char *group, hal_group_callback_t callback, void *cb_data);

// member iterator
// for each defined member, call the callback function iff its group name matches
// AND its member name matches (exact string match).
// - a NULL group name argument is given: this will cause all groups to be visited.
// - a NULL member name argument is given: this will cause all members to be visited,
//   depending on the previous group match.
//
// giving NULL as group name and 'name' as member name will cause the callback to executed
// for each group which has this signal as member.
//
// cb_data can be used in any fashion and it is not inspected.

// callback return values:
//    0:   this signals 'continue iterating'
//    >0:  this signals 'stop iteration and return count'
//    <0:  this signals an error. Stop iterating and return the error code.
// if iteration runs to completion and the callback has never returned a
// non-zero value, halpr_foreach_member returns the number of members visited.
//
// NB: both halpr_foreach_group and halpr_foreach_member lock the global HAL mutex, so it is
// not a good idea to say, call halpr_foreach_member from a halpr_foreach_group callback.
typedef int(*hal_member_callback_t)(hal_group_t *group, hal_member_t *member, void *cb_data);
extern int halpr_foreach_member(const char *group, const char *member,
				hal_member_callback_t callback, void *cb_data);



RTAPI_END_DECLS
#endif // HAL_GROUP_H
