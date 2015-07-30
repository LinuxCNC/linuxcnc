
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

static hal_group_t *alloc_group_struct(void);
static void free_group_struct(hal_group_t * group);

static hal_member_t *alloc_member_struct(void);
static void free_member_struct(hal_member_t * member);

int hal_group_new(const char *name, int arg1, int arg2)
{
    CHECK_HALDATA();
    CHECK_STRLEN(name, HAL_NAME_LEN);
    CHECK_LOCK(HAL_LOCK_LOAD);

    HALDBG("creating group '%s' arg1=%d arg2=%d/0x%x",
	   name, arg1, arg2, arg2);

    {
	hal_group_t *new, *chan;
	hal_group_t *ptr   __attribute__((cleanup(halpr_autorelease_mutex)));
	int *prev, next, cmp;

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* validate group name */
	chan = halpr_find_group_by_name(name);
	if (chan != 0) {
	    HALERR("group '%s' already defined", name);
	    return -EINVAL;
	}
	/* allocate a new group structure */
	new = alloc_group_struct();
	if (new == 0)
	    NOMEM("group '%s'", name);

	/* initialize the structure */
	new->userarg1 = arg1;
	new->userarg2 = arg2;
	new->handle = rtapi_next_handle();
	rtapi_snprintf(new->name, sizeof(new->name), "%s", name);
	/* search list for 'name' and insert new structure */
	prev = &(hal_data->group_list_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		/* reached end of list, insert here */
		new->next_ptr = next;
		*prev = SHMOFF(new);
		return 0;
	    }
	    ptr = SHMPTR(next);
	    cmp = strcmp(ptr->name, new->name);
	    if (cmp > 0) {
		/* found the right place for it, insert here */
		new->next_ptr = next;
		*prev = SHMOFF(new);
		return 0;
	    }
	    /* didn't find it yet, look at next one */
	    prev = &(ptr->next_ptr);
	    next = *prev;
	}
	return 0;
    }
}

int hal_group_delete(const char *name)
{
    CHECK_HALDATA();
    CHECK_STR(name);
    CHECK_LOCK(HAL_LOCK_CONFIG);

    HALDBG("deleting group '%s'", name);

    // this block is protected by hal_data->mutex with automatic
    // onlock on scope exit
    {
	hal_group_t *group __attribute__((cleanup(halpr_autorelease_mutex)));
	int next,*prev;

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));
	/* search for the group */
	prev = &(hal_data->group_list_ptr);
	next = *prev;
	while (next != 0) {
	    group = SHMPTR(next);
	    if (strcmp(group->name, name) == 0) {
		/* this is the right group */
		// verify it is unreferenced, and fail if not:
		if (group->refcount) {
		    HALERR("cannot delete group '%s' (still used: %d)",
				       name, group->refcount);
		    return -EBUSY;
		}
		/* unlink from list */
		*prev = group->next_ptr;
		/* and delete it, linking it on the free list */
		//NB: freeing member list is done in free_group_struct
		free_group_struct(group);
		/* done */
		return 0;
	    }
	    /* no match, try the next one */
	    prev = &(group->next_ptr);
	    next = *prev;
	}
	HALERR("group_delete: no such group '%s'", name);
	return -EINVAL;
    }
}

int hal_ref_group(const char *name)
{
    hal_group_t *group __attribute__((cleanup(halpr_autorelease_mutex)));

    rtapi_mutex_get(&(hal_data->mutex));

    group = halpr_find_group_by_name(name);
    if (group == NULL)
	return -ENOENT;
    group->refcount += 1;
    return 0;
}

int hal_unref_group(const char *name)
{
    hal_group_t *group __attribute__((cleanup(halpr_autorelease_mutex)));

    rtapi_mutex_get(&(hal_data->mutex));

    group = halpr_find_group_by_name(name);
    if (group == NULL)
	return -ENOENT;
    group->refcount -= 1;
    return 0;
}

// does NOT lock hal_data!
int halpr_foreach_group(const char *groupname,
			hal_group_callback_t callback, void *cb_data)
{
    hal_group_t *group;
    int next;
    int nvisited = 0;
    int result;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);

    /* search for the group */
    next = hal_data->group_list_ptr;
    while (next != 0) {
	group = SHMPTR(next);
	if (!groupname || (strcmp(group->name, groupname)) == 0) {
	    nvisited++;
	    /* this is the right group */
	    if (callback) {
		result = callback(group, cb_data);
		if (result < 0) {
		    // callback signaled an error, pass that back up.
		    return result;
		} else if (result > 0) {
		    // callback signaled 'stop iterating'.
		    // pass back the number of visited groups.
		    return nvisited;
		} else {
		    // callback signaled 'OK to continue'
		    // fall through
		}
	    } else {
		// null callback passed in,
		// just count groups
		// nvisited already bumped above.
	    }
	}
	/* no match, try the next one */
	next = group->next_ptr;
    }
    /* if we get here, we ran through all the groups, so return count */
    return nvisited;
}

// run through member list, recursively expanding nested groups depth-first
static int resolve_members( int *nvisited, int level, hal_group_t **groups,
			   hal_member_callback_t callback, void *cb_data,
			   int flags)
{
    hal_member_t *member;
    hal_group_t *nestedgroup;
    int result = 0, mptr = groups[level]->member_ptr;

    while (mptr) {
	member = SHMPTR(mptr);
	if (member->sig_member_ptr) {
	    (*nvisited)++;
	    if (callback)
		result = callback(level, groups, member, cb_data);
	} else if (member->group_member_ptr) {
	    if (flags & RESOLVE_NESTED_GROUPS) {
		if (level >= MAX_NESTED_GROUPS) {
		    // dump stack & bail
		    HALERR("maximum group nesting exceeded for '%s'",
			   groups[0]->name);
		    while (level) {
			HALERR("\t%d: %s", level, groups[level]->name);
			level--;
		    }
		    return -EINVAL;
		}
		nestedgroup = SHMPTR(member->group_member_ptr);
		groups[level+1] = nestedgroup;
		result = resolve_members(nvisited, level+1, groups,
					 callback, cb_data, flags);
	    } else {
		if (callback)
		    result = callback(level, groups, member, cb_data);
		(*nvisited)++;
	    }
	}
	if (result < 0) {
	    // callback signaled an error, pass that back up.
	    return result;
	} else if (result > 0) {
	    // callback signaled 'stop iterating'.
	    // pass back the number of visited members.
	    return *nvisited;
	} else {
	    // callback signaled 'OK to continue'
	    //fall through
	}
	mptr = member->next_ptr;
    }
    return result;
}

// does NOT lock hal_data!
int halpr_foreach_member(const char *groupname,
			 hal_member_callback_t callback, void *cb_data, int flags)
{
    int nvisited = 0;
    int level = 0;
    int result;
    int next;
    hal_group_t *groupstack[MAX_NESTED_GROUPS+1], *grp;

    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_CONFIG);
    CHECK_STR(groupname);

    /* search for the group */
    next = hal_data->group_list_ptr;
    while (next != 0) {
	grp = SHMPTR(next);
	groupstack[0] = grp;

	if (!groupname ||
	    (strcmp(grp->name, groupname)) == 0) {
	    // go through members
	    result = resolve_members(&nvisited, level, groupstack,
				     callback,  cb_data, flags);
	    if (result < 0) {
		// callback signaled an error, pass that back up.
		return result;
	    } else if (result > 0) {
		// callback signaled 'stop iterating'.
		// pass back the number of visited members.
		return nvisited;
	    } else {
		// callback signaled 'OK to continue'
		// fall through
	    }
	} // group match
	/* no match, try the next one */
	next = grp->next_ptr;
    } // forall groups
    /* if we get here, we ran through all the groups, so return count */
    return nvisited;
}

hal_group_t *halpr_find_group_by_name(const char *name)
{
    int next;
    hal_group_t *group;

    /* search group list for 'name' */
    next = hal_data->group_list_ptr;
    while (next != 0) {
	group = SHMPTR(next);
	if (strcmp(group->name, name) == 0) {
	    /* found a match */
	    return group;
	}
	/* didn't find it yet, look at next one */
	next = group->next_ptr;
    }
    /* if loop terminates, we reached end of list with no match */
    return 0;
}

hal_group_t *halpr_find_group_of_member(const char *name)
{
    int nextg, nextm;
    hal_group_t *group, *tgrp;
    hal_member_t *member;
    hal_sig_t *sig;

    nextg = hal_data->group_list_ptr;
    while (nextg != 0) {
	group = SHMPTR(nextg);
	nextm = group->member_ptr;
	while (nextm != 0) {
	    member = SHMPTR(nextm);
	    if (member->sig_member_ptr) { // a signal member

		sig = SHMPTR(member->sig_member_ptr);
		if (strcmp(name, sig->name) == 0) {
		    HALDBG("find_group_of_member(%s): found signal in group '%s'",
			   name, group->name);
		    return group;
		}
		nextm = member->next_ptr;
	    }
	    if (member->group_member_ptr) { // a nested group
		tgrp = SHMPTR(member->group_member_ptr);
		if (strcmp(name, tgrp->name) == 0) {
		    HALDBG("find_group_of_member(%s): found group in group '%s'",
			   name, group->name);
		    return group;
		}
		nextm = member->next_ptr;
	    }
	}
	nextg = group->next_ptr;
    }
    return NULL;
}

int hal_member_new(const char *group, const char *member,
		   int arg1, int eps_index)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);
    CHECK_STRLEN(group, HAL_NAME_LEN);
    CHECK_STRLEN(member, HAL_NAME_LEN);

    if (!strcmp(member, group)) {
	HALERR("member_new(): cannot nest group '%s' as member '%s'",
	       group, member);
	return -EINVAL;
    }

    HALDBG("creating member '%s' arg1=%d epsilon[%d]=%f",
	   member, arg1, eps_index, hal_data->epsilon[eps_index]);

    {
	hal_group_t *grp __attribute__((cleanup(halpr_autorelease_mutex)));
	hal_member_t *new, *ptr;
	hal_group_t *mgrp = NULL;
	hal_sig_t *sig = NULL;
	int *prev, next;

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* validate group name */
	grp = halpr_find_group_by_name(group);
	if (!grp) {
	    HALERR("undefined group '%s'", group);
	    return -EINVAL;
	}

	// TBD: handle remote signal case
	if ((sig = halpr_find_sig_by_name(member)) != NULL) {
	    HALDBG("adding signal '%s' to group '%s'",  member, group);
	    goto found;
	}
	if ((mgrp = halpr_find_group_by_name(member)) != NULL) {
	    HALDBG("adding nested group '%s' to group '%s'", member, group);
	    goto found;
	}
	HALERR("undefined member '%s'", member);
	return -EINVAL;

    found:
	/* allocate a new member structure */
	new = alloc_member_struct();
	if (new == 0) {
	    NOMEM("member '%s'", member);
	    return -ENOMEM;
	}
	/* initialize the structure */
	new->userarg1 = arg1;
	new->eps_index = eps_index;
	new->handle = rtapi_next_handle();
	if (sig) {
	    new->sig_member_ptr =  SHMOFF(sig);
	    new->group_member_ptr = 0;
	} else if (mgrp) {
	    new->group_member_ptr =  SHMOFF(mgrp);
	    new->sig_member_ptr = 0;
	}
	/* insert new structure */
	/* NB: ordering is by insertion sequence */
	prev = &(grp->member_ptr);
	next = *prev;
	while (1) {
	    if (next == 0) {
		/* reached end of list, insert here */
		new->next_ptr = next;
		*prev = SHMOFF(new);
		return 0;
	    }
	    ptr = SHMPTR(next);

	    if (ptr->sig_member_ptr) {
		sig = SHMPTR(ptr->sig_member_ptr);
		if (strcmp(member, sig->name) == 0) {
		    HALERR("group '%s' already has signal member '%s'",
			   group, sig->name);
		    return -EINVAL;
		}
	    }
	    if (ptr->group_member_ptr) {
		mgrp = SHMPTR(ptr->group_member_ptr);
		if (strcmp(member, mgrp->name) == 0) {
		    HALERR("group '%s' already has group member '%s'",
			   group, mgrp->name);
		    return -EINVAL;
		}
	    }
	    /* didn't find it yet, look at next one */
	    prev = &(ptr->next_ptr);
	    next = *prev;
	}
	return 0;
    }
}

int hal_member_delete(const char *group, const char *member)
{
    CHECK_HALDATA();
    CHECK_LOCK(HAL_LOCK_LOAD);
    CHECK_STRLEN(group, HAL_NAME_LEN);
    CHECK_STRLEN(member, HAL_NAME_LEN);

    {
	hal_group_t *grp __attribute__((cleanup(halpr_autorelease_mutex)));
	hal_member_t  *mptr;
	hal_sig_t *sig;
	int *prev, next;

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* validate group name */
	grp = halpr_find_group_by_name(group);
	if (!grp) {
	    HALERR("undefined group '%s'", group);
	    return -EINVAL;
	}

	sig = halpr_find_sig_by_name(member);
	if (!sig) {
	    HALERR("undefined member '%s'", member);
	    return -EINVAL;
	}
	HALDBG("deleting signal '%s' from group '%s'",  member, group);

	/* delete member structure */
	prev = &(grp->member_ptr);
	/* search for the member */
	next = *prev;
	while (next != 0) {
	    mptr = SHMPTR(next);
	    if (strcmp(member, sig->name) == 0) {
		/* this is the right member, unlink from list */
		*prev = mptr->next_ptr;
		/* and delete it */
		free_member_struct(mptr);
		/* done */
		return 0;
	    }
	    /* no match, try the next one */
	    prev = &(mptr->next_ptr);
	    next = *prev;
	}
	// pin or signal did exist but was not a group member */
	HALERR("signal '%s' exists but not member of '%s'",
	       member, group);
	return 0;
    }
}

#ifdef ULAPI

static int cgroup_size_cb(int level, hal_group_t **groups, hal_member_t *member,
			  void *cb_data)
{
    hal_compiled_group_t *tc = cb_data;
    tc->n_members++;
    if ((member->userarg1 & MEMBER_MONITOR_CHANGE) ||
	// the toplevel group flags are relevant, no the nested ones:
	(groups[0]->userarg2 & GROUP_MONITOR_ALL_MEMBERS))
	tc->n_monitored++;
    return 0;
}

static int cgroup_init_members_cb(int level, hal_group_t **groups, hal_member_t *member,
				  void *cb_data)
{
    hal_compiled_group_t *tc = cb_data;

    tc->member[tc->mbr_index] = member;
    tc->mbr_index++;
    if ((member->userarg1 & MEMBER_MONITOR_CHANGE) ||
	(groups[0]->userarg2 & GROUP_MONITOR_ALL_MEMBERS)) {
	tc->mon_index++;
    }
    return 0;
}

// group generic change detection & reporting support
// must be called with hal_data lock aquired by caller
int halpr_group_compile(const char *name, hal_compiled_group_t **cgroup)
{
    int result;
    hal_compiled_group_t *tc;
    hal_group_t *grp;

    CHECK_STR(name);

    if ((grp = halpr_find_group_by_name(name)) == NULL) {
	HALERR("no such group '%s'", name);
	return -EINVAL;
    }

    // a compiled group is a userland memory object
    if ((tc = malloc(sizeof(hal_compiled_group_t))) == NULL)
	NOMEM("hal_compiled_group");

    memset(tc, 0, sizeof(hal_compiled_group_t));

    // first pass: determine sizes
    // this fills sets the n_members and n_monitored fields
    result = halpr_foreach_member(name, cgroup_size_cb,
				  tc, RESOLVE_NESTED_GROUPS);
    HALDBG("hal_group_compile(%s): %d signals %d monitored",
	   name, tc->n_members, tc->n_monitored );
    if ((tc->member =
	 malloc(sizeof(hal_member_t  *) * tc->n_members )) == NULL)
	NOMEM("%d hal_members",  tc->n_members);

    tc->mbr_index = 0;
    tc->mon_index = 0;

    // second pass: fill in references
    result = halpr_foreach_member(name, cgroup_init_members_cb,
				  tc, RESOLVE_NESTED_GROUPS);
    assert(tc->n_monitored == tc->mon_index);
    assert(tc->n_members == tc->mbr_index);

    // this attribute combination does not make sense - such a group
    // definition will never trigger a report:
    if ((grp->userarg2 & (GROUP_REPORT_ON_CHANGE|GROUP_REPORT_CHANGED_MEMBERS)) &&
	(tc->n_monitored  == 0)) {
	HALERR("changed-monitored group '%s' with no members to check",
	       name);
	return -EINVAL;
    }

    // set up value tracking if either the whole group is to be monitored for changes
    // to cause a report, or only changed members should be included in a periodic report
    if ((grp->userarg2 & (GROUP_REPORT_ON_CHANGE|GROUP_REPORT_CHANGED_MEMBERS)) ||
	(tc->n_monitored  > 0)) {
	if ((tc->tracking =
	     malloc(sizeof(hal_data_u) * tc->n_monitored )) == NULL)
	    return -ENOMEM;
	memset(tc->tracking, 0, sizeof(hal_data_u) * tc->n_monitored);
	if ((tc->changed =
	     malloc(RTAPI_BITMAP_BYTES(tc->n_members))) == NULL)
	    return -ENOMEM;
	RTAPI_ZERO_BITMAP(tc->changed, tc->n_members);
    } else {
	// nothing to track
	tc->n_monitored = 0;
	tc->tracking = NULL;
	tc->changed = NULL;
    }

    tc->magic = CGROUP_MAGIC;
    tc->group = grp;
    grp->refcount++;
    tc->user_data = NULL;
    tc->user_flags = 0;
    *cgroup = tc;

    return 0;
}

int hal_cgroup_apply(hal_compiled_group_t *cg, int handle, hal_type_t type, hal_data_u value)
{
    hal_sig_t *sig;
    hal_data_u *dp;
    hal_member_t *member;

    HAL_ASSERT(cg->magic ==  CGROUP_MAGIC);

    // handles (value references) run from 0..cc->n_pin-1
    if ((handle < 0) || (handle > cg->n_members-1))
	return -ERANGE;

    member = cg->member[handle];
    sig = SHMPTR(member->sig_member_ptr);
    dp = SHMPTR(sig->data_ptr);

    if (sig->writers > 0)
	HALWARN("group '%s': member signal '%s' already has updater",
		cg->group->name, sig->name);

    switch (type) {
    case HAL_TYPE_UNSPECIFIED:
	HAL_ASSERT(type != HAL_TYPE_UNSPECIFIED);
	return -EINVAL;
    case HAL_BIT:
	dp->b = value.b;
	break;
    case HAL_FLOAT:
	dp->f = value.f;
	break;
    case HAL_S32:
	dp->s = value.s;
	break;
    case HAL_U32:
	dp->u = value.u;
	break;
    }
    return 0;
}


int hal_cgroup_match(hal_compiled_group_t *cg)
{
    int i, monitor, nchanged = 0, m = 0;
    hal_sig_t *sig;
    hal_bit_t halbit;
    hal_s32_t hals32;
    hal_u32_t halu32;
    hal_float_t halfloat,delta;

    HAL_ASSERT(cg->magic ==  CGROUP_MAGIC);

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
	    sig = SHMPTR(cg->member[i]->sig_member_ptr);
	    switch (sig->type) {
	    case HAL_BIT:
		halbit = *((hal_bit_t *) SHMPTR(sig->data_ptr));
		if (cg->tracking[m].b != halbit) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    cg->tracking[m].b = halbit;
		}
		break;
	    case HAL_FLOAT:
		halfloat = *((hal_float_t *) SHMPTR(sig->data_ptr));
		delta = HAL_FABS(halfloat - cg->tracking[m].f);
		if (delta > hal_data->epsilon[cg->member[i]->eps_index]) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    cg->tracking[m].f = halfloat;
		}
		break;
	    case HAL_S32:
		hals32 =  *((hal_s32_t *) SHMPTR(sig->data_ptr));
		if (cg->tracking[m].s != hals32) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    cg->tracking[m].s = hals32;
		}
		break;
	    case HAL_U32:
		halu32 =  *((hal_u32_t *) SHMPTR(sig->data_ptr));
		if (cg->tracking[m].u != halu32) {
		    nchanged++;
		    RTAPI_BIT_SET(cg->changed, i);
		    cg->tracking[m].u = halu32;
		}
		break;
	    default:
		HALERR("BUG: detect_changes(%s): invalid type for signal %s: %d",
		       cg->group->name, sig->name, sig->type);
		return -EINVAL;
	    }
	    m++;
	}
	return nchanged;
    } else
	  return 1; // by default match
}


int hal_cgroup_report(hal_compiled_group_t *cg, group_report_callback_t report_cb,
		      void *cb_data, int force_all)
{
    int retval, i, reportall;

    if (!report_cb)
	return 0;
    if ((retval = report_cb(REPORT_BEGIN, cg, NULL,  cb_data)) < 0)
	return retval;

    // report all members if forced, there are no members with change detect in the group,
    // or the group doesnt have the 'report changed members only' bit set
    reportall = force_all || (cg->n_monitored == 0) ||
	!(cg->group->userarg2 & GROUP_REPORT_CHANGED_MEMBERS);

    for (i = 0; i < cg->n_members; i++) {
	if (reportall || RTAPI_BIT_TEST(cg->changed, i))
	    if ((retval = report_cb(REPORT_SIGNAL, cg,
				    SHMPTR(cg->member[i]->sig_member_ptr), cb_data)) < 0)
		return retval;
    }
    return report_cb(REPORT_END, cg, NULL,  cb_data);
}

int hal_cgroup_free(hal_compiled_group_t *cgroup)
{
    if (cgroup == NULL)
	return -ENOENT;
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

// internal support routines

static hal_group_t *alloc_group_struct(void)
{
    hal_group_t *p;

    /* check the free list */
    if (hal_data->group_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->group_free_ptr);
	/* unlink it from the free list */
	hal_data->group_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_group_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->refcount = 0;
	p->userarg1 = 0;
	p->userarg2 = 0;
	p->member_ptr = 0;
	p->name[0] = '\0';
    }
    return p;
}


static hal_member_t *alloc_member_struct(void)
{
    hal_member_t *p;

    /* check the free list */
    if (hal_data->member_free_ptr != 0) {
	/* found a free structure, point to it */
	p = SHMPTR(hal_data->member_free_ptr);
	/* unlink it from the free list */
	hal_data->member_free_ptr = p->next_ptr;
	p->next_ptr = 0;
    } else {
	/* nothing on free list, allocate a brand new one */
	p = shmalloc_dn(sizeof(hal_member_t));
    }
    if (p) {
	/* make sure it's empty */
	p->next_ptr = 0;
	p->sig_member_ptr = 0;
	p->group_member_ptr = 0;
	p->userarg1 = 0;
	p->eps_index = 0;
    }
    return p;
}
static void free_member_struct(hal_member_t * member)
{
    /* clear contents of struct */
    member->group_member_ptr = 0;
    member->sig_member_ptr = 0;
    member->userarg1 = 0;
    member->handle = -1;

    /* add it to free list */
    member->next_ptr = hal_data->member_free_ptr;
    hal_data->member_free_ptr = SHMOFF(member);
}

static void free_group_struct(hal_group_t * group)
{
    int nextm;
    hal_member_t * member;

    /* clear contents of struct */
    group->next_ptr = 0;
    group->refcount = 0;
    group->userarg1 = 0;
    group->userarg2 = 0;
    group->handle = -1;
    group->name[0] = '\0';
    nextm = group->member_ptr;
    // free all linked member structs
    while (nextm != 0) {
	member = SHMPTR(nextm);
	nextm = member->next_ptr;
	free_member_struct(member);
    }
    group->member_ptr = 0;

    /* add it to free list */
    group->next_ptr = hal_data->group_free_ptr;
    hal_data->group_free_ptr = SHMOFF(group);
}

#ifdef RTAPI
EXPORT_SYMBOL(halpr_find_group_of_member);
#endif
