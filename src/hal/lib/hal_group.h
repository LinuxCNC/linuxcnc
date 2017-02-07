#ifndef HAL_GROUP_H
#define HAL_GROUP_H

#include <rtapi.h>
#include <hal_priv.h>

RTAPI_BEGIN_DECLS


// groups are top-level objects and have no owner,
// hence hdr.owner_id == 0
typedef struct hal_group {
    halhdr_t hdr;		// common HAL object header
    int userarg1;	        /* interpreted by using layer */
    int userarg2;	        /* interpreted by using layer */
} hal_group_t;

// members are subordinate to a group identified by the group_id
// in the hdr.
// a member refers to a signal of the same name,
// through sig_ptr (for performance reasons).
typedef struct hal_member {
    halhdr_t hdr;		// common HAL object header
    int sig_ptr;                // refers to a signal descriptor
    int userarg1;               // interpreted by using layer
    __u8 eps_index;             // index into haldata->epsilon[]; default 0
} hal_member_t;

#define CGROUP_MAGIC  0xbeef7411
typedef struct hal_compiled_group {
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

extern int halg_group_new(const int use_hal_mutex, const char *group, int arg1, int arg2);
extern int halg_group_delete(const int use_hal_mutex, const char *group);

extern int halg_member_new(const int use_hal_mutex, const char *group, const char *member, int arg1, int eps_index);
extern int halg_member_delete(const int use_hal_mutex, const char *group, const char *member);

extern int hal_cgroup_report(hal_compiled_group_t *cgroup,
			     group_report_callback_t report_cb,
			     void *cb_data, int force_all);
extern int hal_cgroup_free(hal_compiled_group_t *cgroup);

// using code is supposed to hal_ref_group() when starting to use it
// this prevents group change or deletion during use
extern int hal_ref_group(const char *group);
// when done, a group should be unreferenced:
extern int hal_unref_group(const char *group);


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
	HALFAIL_RC(EINVAL, "invalid cgroup");
    return cgroup->group->userarg1;
}
extern int halpr_group_compile(const char *name, hal_compiled_group_t **cgroup);
extern int hal_cgroup_match(hal_compiled_group_t *cgroup);

// given a cgroup which returned a non-zero value from hal_cgroup_match(),
// generate a report.
// the report callback is called for the following phases:
typedef enum {
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


static inline hal_group_t *halpr_find_group_by_name(const char *name){
    return halg_find_object_by_name(0, HAL_GROUP, name).group;
}


RTAPI_END_DECLS
#endif // HAL_GROUP_H
