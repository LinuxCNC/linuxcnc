#ifndef HAL_GROUP_H
#define HAL_GROUP_H

#include <rtapi.h>
#include <hal_priv.h>

RTAPI_BEGIN_DECLS

typedef struct {
    int next_ptr;		/* next member in linked list */
    int sig_member_ptr;          /* offset of hal_signal_t  */
    int group_member_ptr;        /* offset of hal_group_t (nested) */
    int userarg1;                /* interpreted by using layer */
    int handle;                 // unique ID
    __u8 eps_index;             // index into haldata->epsilon[]; default 0
} hal_member_t;

typedef struct {
    int next_ptr;		/* next group in free list */
    int refcount;               /* advisory by using code */
    int userarg1;	        /* interpreted by using layer */
    int userarg2;	        /* interpreted by using layer */
    int handle;                 /* immutable ID */
    char name[HAL_NAME_LEN + 1];	/* group name */
    int member_ptr;             /* list of group members */
} hal_group_t;


#define CGROUP_MAGIC  0xbeef7411
typedef struct {
    int magic;
    hal_group_t *group;
    int n_members;
    int mbr_index;               // iterator state
    int mon_index;               // iterator state
    hal_member_t  **member;      // all members (nesting resolved)
    unsigned long *changed;      // bitmap
    int n_monitored;             // count of pins to monitor for change
    hal_data_u    *tracking;     // tracking values of monitored pins
    unsigned long user_flags;    // uninterpreted by HAL code
    void *user_data;             // uninterpreted by HAL code
} hal_compiled_group_t;

typedef int (*group_report_callback_t)(int,  hal_compiled_group_t *,
				      hal_sig_t *sig, void *cb_data);

typedef int (*hal_group_callback_t)(hal_group_t *group,  void *cb_data);

// the following functions lock the hal mutex:
extern int hal_group_new(const char *group, int arg1, int arg2);
extern int hal_group_delete(const char *group);

extern int hal_member_new(const char *group, const char *member, int arg1, int eps_index);
extern int hal_member_delete(const char *group, const char *member);

extern int hal_cgroup_report(hal_compiled_group_t *cgroup,
			     group_report_callback_t report_cb,
			     void *cb_data, int force_all);
extern int hal_cgroup_free(hal_compiled_group_t *cgroup);

// using code is supposed to hal_ref_group() when starting to use it
// this prevents group change or deletion during use
extern int hal_ref_group(const char *group);
// when done, a group should be unreferenced:
extern int hal_unref_group(const char *group);


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
//
// NB: halpr_foreach_group() does not lock hal_data.
extern int halpr_foreach_group(const char *groupname,
			       hal_group_callback_t callback, void *cb_data);


// group level operations:
// typedef struct {} .. compiled_group_t;
// typedef struct {} .. group_status_t;
//
// compiled_group_t *cgroup;
//
// int halpr_group_compile("name", &cgroup)
// NB: halpr_group_compile() does not lock hal_data.

// hal_group_execute(cgroup)  // not sure if needed
// hal_group_report(cgroup, group_cb, member_cb, flags)
// flags: 0.. changed; 1..all-unconditional
//
// hal_group_free(cgroup);
//
// outlines:
// hal_group_compile("name", &cgroup)
// - expanding nested groups:
// - determine cardinality - group size, # of change detects
// - allocate compiled_group_t including shadow signals and
//   pointers to group members
// - fill in bitmaps as needed
// - set changed bit for all monitored signals to cause initial report
// - fill in header for fast check if change-detect or report
// - lock by reference_group
// exec a compiled group and see if report needed and if so, how:
// hal_group_match(cgroup)
static inline int hal_cgroup_timer(hal_compiled_group_t *cgroup)
{
    if (!cgroup || !cgroup->group)
	return -EINVAL;
    return cgroup->group->userarg1;
}
extern int halpr_group_compile(const char *name, hal_compiled_group_t **cgroup);
extern int hal_cgroup_match(hal_compiled_group_t *cgroup);

// given a cgroup which returned a non-zero value from hal_cgroup_match(),
// generate a report.
// the report callback is called for the following phases:
typedef enum report_phase {
    REPORT_BEGIN,
    REPORT_SIGNAL, // for cgroups only
    REPORT_PIN,    // for ccomp's only
    REPORT_END
} report_phase_t;

#if 0
// a sample report callback would have the following structure:
int demo_report(int phase, hal_compiled_group_t *cgroup, hal_sig_t *sig,
		void *cb_data)
{
    switch (phase) {
    case REPORT_BEGIN:
	// any report initialisation
	break;
    case REPORT_SIGNAL:
	// per-reported-signal action
	break;
    case REPORT_END:
	// finalise & send it off
	break;
    }
    return 0;
}
#endif


// member iterator
// visit the group named 'group', or all groups if group was passed as NULL
//
// for each member in matching groups, call the callback function
// if the member happens is a nested group:
//   if flags & RESOLVE_NESTED_GROUPS is nonzero:
//     all nested groups are resolved recursivel and
//     the callback happens at the leaf (signal level)
//   else
//     the callback happens at the top level without nested group resolution
//     in this case using code must be prepared to deal with a group or signal member
//     in the callback function
//
//
// cb_data can be used in any fashion and it is passed to the callback
// uninspected and unmodified.
//
// callback return values are interpreted as follows:
//    0:   this signals 'continue iterating'
//    >0:  this signals 'stop iteration and return count'
//    <0:  this signals a user-defined error situation;
//         stop iterating and return the error code.
//
// if iteration runs to completion and the callback has never returned a
// non-zero value, halpr_foreach_member returns the number of members visited.
//
// NB: halpr_foreach_member() does not lock hal_data.
#define RESOLVE_NESTED_GROUPS 1
#define MAX_NESTED_GROUPS 10

typedef int (*hal_member_callback_t)(int level, hal_group_t **groups,
				     hal_member_t *member, void *cb_data);
extern int halpr_foreach_member(const char *group,
				hal_member_callback_t callback, void *cb_data, int flags);

// group execution argument conventions:
//
// group.arg1 = timer (mS)
// group.arg2 = action disposition bit mask
//              bit 0=0..hal_cgroup_match() always returns true
//              bit 0=1..report if change detected
#define GROUP_REPORT_ON_CHANGE 1
// halcmd keyword: onchange always

// change detection disposition:
//              bit 1=0..monitor members with userarg MEMBER_MONITOR_CHANGE bit set
//              bit 1=1..monitor all members for change regardless of member
//                     ..MEMBER_MONITOR_CHANGES bit
#define GROUP_MONITOR_ALL_MEMBERS 2
// halcmd keyword: monitorall

//              bit 2=0..report all members
//              bit 2=1..report only changed members
#define GROUP_REPORT_CHANGED_MEMBERS 4
// halcmd keyword: reportchanged reportall

// the following combination of attributes makes no sense and will
// cause an error message by hal_compile_group():
// - the GROUP_REPORT_ON_CHANGE bit is set in group.arg2
// - there are no members in the group which have the
//   MEMBER_MONITOR_CHANGES attribute set in member.userarg
//

// sensible combinations for arg2 are:
// 0...report whole group unconditionally
// 1...report whole group if any member marked for change detect changed value
// 3...report only changed members
//
// nonsense combinations:
// 1 or 3 and no member marked for change detect
//
// timer (group.arg1) == 0: no periodic checks
// this would be used if there is some other non-periodic/non-change
// mechanism to notify the reporting enitiy to cause a report
//
// member arg, epsilon:
// member.arg = action disposition bit mask
//              bit 0=0..no change detection
//              bit 0=1..monitor for changes

#define MEMBER_MONITOR_CHANGE  1
// halcmd keyword: monitor

// member.eps_index: consider as changed iff:
//                 (type(member) == HAL_FLOAT) &&
//                 (abs(value - previous-value) > hal_data->epsilon[eps_index]



// the following functions do NOT lock the hal mutex:
extern hal_group_t *halpr_find_group_by_name(const char *name);
extern hal_group_t *halpr_find_group_of_member(const char *member);



RTAPI_END_DECLS
#endif // HAL_GROUP_H
