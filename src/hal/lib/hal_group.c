
#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"
#include "hal_group.h"		/* HAL group decls */

#if defined(ULAPI)
#include <stdlib.h>		/* malloc()/free() */
#include <assert.h>
#endif

int halg_group_new(const int use_hal_mutex,const char *name, int arg1, int arg2)
{
    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_LOCK(HAL_LOCK_LOAD);

    HALDBG("creating group '%s' arg1=%d arg2=%d/0x%x",
	   name, arg1, arg2, arg2);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_group_t *group = halpr_find_group_by_name(name);
	if (group != 0) {
	    HALFAIL_RC(EINVAL, "group '%s' already defined", name);
	}

	if ((group = halg_create_objectf(0, sizeof(hal_group_t),
					 HAL_GROUP, 0, name)) == NULL)
	    return _halerrno;

	group->userarg1 = arg1;
	group->userarg2 = arg2;

	halg_add_object(false, (hal_object_ptr)group);
	return 0;
    }
}

int halg_group_delete(const int use_hal_mutex,const char *name)
{
    CHECK_HALDATA();
    CHECK_STR(name);
    CHECK_LOCK(HAL_LOCK_CONFIG);

    HALDBG("deleting group '%s'", name);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_group_t *group = halpr_find_group_by_name(name);
	if (group == NULL) {
	    HALFAIL_RC(ENOENT,"group '%s' not found", name);
	}
	if (ho_referenced(group)) {
	    HALFAIL_RC(EBUSY,"cannot delete group '%s' (still used: %d)",
		   name, ho_refcnt(group));
	}
	// NB: freeing any members is done in free_group_struct
	free_group_struct(group);
    }
    return 0;
}

int hal_ref_group(const char *name)
{
    WITH_HAL_MUTEX();
    hal_group_t *group  = halpr_find_group_by_name(name);
    if (group == NULL)
	HALFAIL_RC(ENOENT,"group '%s' not found", name);
    ho_incref(group);
    return 0;
}

int hal_unref_group(const char *name)
{
    WITH_HAL_MUTEX();
    hal_group_t *group = halpr_find_group_by_name(name);
    if (group == NULL)
	HALFAIL_RC(ENOENT,"group '%s' not found", name);
    ho_decref(group);
    return 0;
}

int halg_member_new(const int use_hal_mutex,const char *group, const char *member,
		   int arg1, int eps_index)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);
    CHECK_STRLEN(group, HAL_NAME_LEN);
    CHECK_STRLEN(member, HAL_NAME_LEN);

    HALDBG("creating member '%s' arg1=%d epsilon[%d]=%f",
	   member, arg1, eps_index, hal_data->epsilon[eps_index]);
    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_member_t *new;
	hal_sig_t *sig;

	hal_group_t *grp = halg_find_object_by_name(0, HAL_GROUP, group).group;
	if (!grp) {
	    HALFAIL_RC(ENOENT, "no such group '%s'", group);
	}

	// fail if group referenced
	if (ho_referenced(grp)) {
	    HALFAIL_RC(EBUSY, "cannot change referenced group '%s', refcount=%d",
		   group, ho_refcnt(grp));
	}

	if ((sig = halg_find_object_by_name(0, HAL_SIGNAL, member).sig) == NULL) {
	    HALFAIL_RC(ENOENT, "no such signal '%s'", member);
	}

	// detect duplicate insertion
	new = halg_find_object_by_name(0, HAL_MEMBER, member).member;
	if (new != NULL) {
	    HALFAIL_RC(EBUSY, "group '%s' already has signal '%s' as member", group, member);
	}

	HALDBG("adding signal '%s' to group '%s'",  member, group);
	if ((new = halg_create_objectf(0, sizeof(hal_member_t),
				       HAL_MEMBER, ho_id(grp), member)) == NULL)
	    return _halerrno;

	ho_incref(sig); // prevent deletion

	new->sig_ptr = SHMOFF(sig);
	new->userarg1 = arg1;
	new->eps_index = eps_index;

	halg_add_object(false, (hal_object_ptr)new);
	return 0;
    }
}

int halg_member_delete(const int use_hal_mutex,const char *group, const char *member)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);
    CHECK_STRLEN(group, HAL_NAME_LEN);
    CHECK_STRLEN(member, HAL_NAME_LEN);

    {
	WITH_HAL_MUTEX_IF(use_hal_mutex);

	hal_group_t *grp;
	hal_member_t  *mptr;
	hal_sig_t *sig;

	grp = halpr_find_group_by_name(group);
	if (!grp) {
	    HALFAIL_RC(EINVAL, "no such group '%s'", group);
	}
	// fail if group referenced
	if (ho_referenced(grp)) {

	    HALFAIL_RC(EBUSY, "cannot change referenced group '%s', refcount=%d",
		   group,  ho_refcnt(grp));
	}
	mptr = halg_find_object_by_name(0, HAL_MEMBER, member).member;
	if (!mptr) {
	    HALFAIL_RC(ENOENT, "no such member '%s'", member);
	}

	if ((sig = halpr_find_sig_by_name(member)) == NULL) {
	    HALFAIL_RC(ENOENT," BUG: no such signal '%s' ??", member);
	} else {
	    // drop refcnt on signal
	    ho_decref(sig); // permit deletion if 0
	}

	HALDBG("deleting member '%s' from group '%s'",  member, group);
	halg_free_object(false, (hal_object_ptr) mptr);
    }
    return 0;
}

#ifdef ULAPI

static int cgroup_init_members_cb(hal_object_ptr o, foreach_args_t *args)
{
    hal_member_t *member = o.member;
    hal_compiled_group_t *tc = args->user_ptr1;
    hal_group_t *group  = args->user_ptr2;

    tc->member[tc->mbr_index] = member;
    tc->mbr_index++;
    if ((member->userarg1 & MEMBER_MONITOR_CHANGE) ||
	(group->userarg2 & GROUP_MONITOR_ALL_MEMBERS)) {
	tc->mon_index++;
    }
    return 0;
}

static int cgroup_size_cb(hal_object_ptr o, foreach_args_t *args)
{
    hal_member_t *member = o.member;
    hal_compiled_group_t *tc = args->user_ptr1;
    hal_group_t *group  = args->user_ptr2;

    tc->n_members++;
    if ((member->userarg1 & MEMBER_MONITOR_CHANGE) ||
	(group->userarg2 & GROUP_MONITOR_ALL_MEMBERS))
	tc->n_monitored++;
    return 0;
}

// group generic change detection & reporting support
int halpr_group_compile(const char *name, hal_compiled_group_t **cgroup)
{
    hal_compiled_group_t *tc;
    hal_group_t *grp;

    CHECK_STR(name);

    if ((grp = halpr_find_group_by_name(name)) == NULL) {
	HALFAIL_RC(EINVAL, "no such group '%s'", name);
    }

    // a compiled group is a userland memory object
    if ((tc = calloc(sizeof(hal_compiled_group_t), 1)) == NULL)
	NOMEM("hal_compiled_group");

    // first pass: determine sizes
    // this fills sets the n_members and n_monitored fields
    foreach_args_t args =  {
	.type = HAL_MEMBER,
	.owner_id = ho_id(grp),
	.user_ptr1 = tc,
	.user_ptr2 = grp,
    };
    halg_foreach(0, &args, cgroup_size_cb);

    HALDBG("hal_group_compile(%s): %d signals %d monitored",
	   name, tc->n_members, tc->n_monitored );

    if ((tc->member =
	 malloc(sizeof(hal_member_t  *) * tc->n_members )) == NULL)
	NOMEM("%d hal_members",  tc->n_members);

    tc->mbr_index = 0;
    tc->mon_index = 0;

    // second pass: fill in references (same args)
    halg_foreach(0, &args, cgroup_init_members_cb);

    assert(tc->n_monitored == tc->mon_index);
    assert(tc->n_members == tc->mbr_index);

    // this attribute combination does not make sense - such a group
    // definition will never trigger a report:
    if ((grp->userarg2 & (GROUP_REPORT_ON_CHANGE|GROUP_REPORT_CHANGED_MEMBERS)) &&
	(tc->n_monitored  == 0)) {
	HALFAIL_RC(EINVAL, "changed-monitored group '%s' with no members to check",
	       name);
    }

    // set up value tracking if either the whole group is to be monitored for changes
    // to cause a report, or only changed members should be included in a periodic report
    if ((grp->userarg2 & (GROUP_REPORT_ON_CHANGE|GROUP_REPORT_CHANGED_MEMBERS)) ||
	(tc->n_monitored  > 0)) {
	if ((tc->tracking =
	     malloc(sizeof(hal_data_u) * tc->n_monitored )) == NULL)
	    NOMEM("allocating tracking values");
	memset(tc->tracking, 0, sizeof(hal_data_u) * tc->n_monitored);
	if ((tc->changed =
	     malloc(RTAPI_BITMAP_BYTES(tc->n_members))) == NULL)
	    NOMEM("allocating change bitmap");
	RTAPI_ZERO_BITMAP(tc->changed, tc->n_members);
    } else {
	// nothing to track
	tc->n_monitored = 0;
	tc->tracking = NULL;
	tc->changed = NULL;
    }

    tc->magic = CGROUP_MAGIC;
    tc->group = grp;
    ho_incref(grp);
    tc->user_data = NULL;
    tc->user_flags = 0;
    *cgroup = tc;

    return 0;
}

int hal_cgroup_match(hal_compiled_group_t *cg)
{
    int i, monitor, nchanged = 0, m = 0;
    hal_object_ptr ho;
    //    hal_sig_t *sig;
    hal_bit_t halbit;
    hal_s32_t hals32;
    hal_u32_t halu32;
    hal_float_t halfloat,delta;

    HAL_ASSERT(cg->magic == CGROUP_MAGIC);

    // scan for changes if needed
    monitor = (cg->group->userarg2 & (GROUP_REPORT_ON_CHANGE|GROUP_REPORT_CHANGED_MEMBERS))
	|| (cg->n_monitored > 0);

    // walk the group if either the whole group is to be monitored for changes
    // to cause a report, or only changed members should be included in a periodic
    // report.
    if (monitor) {
	RTAPI_ZERO_BITMAP(cg->changed, cg->n_members);
	for (i = 0; i < cg->n_members; i++) {
	    if (!((cg->member[i]->userarg1 &  MEMBER_MONITOR_CHANGE) ||
		  (cg->group->userarg2 &  GROUP_MONITOR_ALL_MEMBERS)))
		continue;
	    ho.any = SHMPTR(cg->member[i]->sig_ptr);
	    switch (sig_type(ho.sig)) {
	    case HAL_BIT:
		halbit = _get_bit_sig(ho.sig);
		if (get_bit_value(&cg->tracking[m]) != halbit) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    set_bit_value(&cg->tracking[m], halbit);
		}
		break;
	    case HAL_FLOAT:
		halfloat = _get_float_sig(ho.sig);
		delta = HAL_FABS(halfloat - get_float_value(&cg->tracking[m]));
		if (delta > hal_data->epsilon[cg->member[i]->eps_index]) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    set_float_value(&cg->tracking[m], halfloat);
		}
		break;
	    case HAL_S32:
		hals32 = _get_s32_sig(ho.sig);
		if (get_s32_value(&cg->tracking[m]) != hals32) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    set_s32_value(&cg->tracking[m], hals32);
		}
		break;
	    case HAL_U32:
		halu32 = _get_u32_sig(ho.sig);
		if (get_u32_value(&cg->tracking[m]) != halu32) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    set_u32_value(&cg->tracking[m], halu32);
		}
		break;
	    default:
		HALFAIL_RC(EINVAL, "BUG: detect_changes(%s): invalid type for signal %s: %d",
		       ho_name(cg->group), ho_name(ho.sig), sig_type(ho.sig));
	    }
	    m++;
	}
	return nchanged;
    } else
	  return 1; // by default match
}


int hal_cgroup_report(hal_compiled_group_t *cg,
		      group_report_callback_t report_cb,
		      void *cb_data,
		      int force_all)
{
    int retval, i, reportall;

    if (!report_cb)
	return 0;
    if ((retval = report_cb(REPORT_BEGIN, cg, NULL,  cb_data)) < 0)
	return retval;

    // report all members if forced, there are no members with
    // change detect in the group,
    // or the group doesnt have the 'report changed members only' bit set
    reportall = force_all || (cg->n_monitored == 0) ||
	!(cg->group->userarg2 & GROUP_REPORT_CHANGED_MEMBERS);

    for (i = 0; i < cg->n_members; i++) {
	if (reportall || RTAPI_BIT_TEST(cg->changed, i))
	    if ((retval = report_cb(REPORT_SIGNAL, cg,
				    SHMPTR(cg->member[i]->sig_ptr),
				    cb_data)) < 0)
		return retval;
    }
    return report_cb(REPORT_END, cg, NULL,  cb_data);
}

int hal_cgroup_free(hal_compiled_group_t *cgroup)
{
    if (cgroup == NULL)
	HALFAIL_RC(ENOENT, "null cgroup");
    if (cgroup->tracking)
	free(cgroup->tracking);
    if (cgroup->changed)
	free(cgroup->changed);
    if (cgroup->member)
	free(cgroup->member);
    free(cgroup);
    return 0;
}
#endif // ULAPI

void free_group_struct(hal_group_t * group)
{
    // delete all owned members first
    foreach_args_t args =  {
	.type = HAL_MEMBER,
	.owner_id = ho_id(group),
    };
    halg_foreach(0, &args, yield_free);
    halg_free_object(false, (hal_object_ptr)group);
}

