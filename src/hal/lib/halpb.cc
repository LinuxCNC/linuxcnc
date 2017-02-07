#include "halpb.hh"

int halpr_describe_pin(hal_pin_t *pin, machinetalk::Pin *pbpin)
{
    pbpin->set_type((machinetalk::ValueType) pin->type);
    pbpin->set_dir((machinetalk::HalPinDirection) pin->dir);
    pbpin->set_handle(ho_id(pin));
    pbpin->set_name(ho_name(pin));
    pbpin->set_linked(pin_is_linked(pin));
    assert(hal_pin2pb(pin, pbpin) == 0);
    pbpin->set_flags(pin->flags);
    if (pin->type == HAL_FLOAT)
	pbpin->set_epsilon(hal_data->epsilon[pin->eps_index]);
    return 0;
}

int halpr_describe_param(hal_param_t *param, machinetalk::Param *pbparam)
{
    pbparam->set_name(ho_name(param));
    pbparam->set_handle(ho_id(param));
    pbparam->set_type((machinetalk::ValueType) param->type);
    pbparam->set_dir((machinetalk::HalParamDirection) param->dir);
    assert(hal_param2pb(param, pbparam) == 0);
    return 0;
}

int pbadd_owned(hal_object_ptr o, foreach_args_t *args)
{
    int type = hh_get_object_type(o.hdr);
    machinetalk::Component *pbcomp = (machinetalk::Component *)args->user_ptr1;
    switch (type) {
    case HAL_PARAM:
	{
	    machinetalk::Param *pbparam = pbcomp->add_param();
	    halpr_describe_param(o.param, pbparam);
	}
	break;
    case HAL_PIN:
	{
	    machinetalk::Pin *pbpin = pbcomp->add_pin();
	    halpr_describe_pin(o.pin, pbpin);
	}
	break;
    default: ;
    }
    return 0;
}

// transfrom a HAL component into a Component protobuf.
// does not aquire the HAL mutex.
int
halpr_describe_component(hal_comp_t *comp, machinetalk::Component *pbcomp)
{
    pbcomp->set_name(ho_name(comp));
    pbcomp->set_comp_id(ho_id(comp));
    pbcomp->set_type(comp->type);
    pbcomp->set_state(comp->state);
    pbcomp->set_last_update(comp->last_update);
    pbcomp->set_last_bound(comp->last_bound);
    pbcomp->set_last_unbound(comp->last_unbound);
    pbcomp->set_pid(comp->pid);
    if (comp->insmod_args)
	pbcomp->set_args((const char *)SHMPTR(comp->insmod_args));
    pbcomp->set_userarg1(comp->userarg1);
    pbcomp->set_userarg2(comp->userarg2);

    foreach_args_t args = {};
    args.owning_comp = ho_id(comp);
    args.user_ptr1 = (void *)pbcomp;
    halg_foreach(0, &args, pbadd_owned);
    return 0;
}

int
halpr_describe_signal(hal_sig_t *sig, machinetalk::Signal *pbsig)
{
    pbsig->set_name(ho_name(sig));
    pbsig->set_handle(ho_id(sig));
    pbsig->set_type((machinetalk::ValueType)sig->type);
    pbsig->set_readers(sig->readers);
    pbsig->set_writers(sig->writers);
    pbsig->set_bidirs(sig->bidirs);
    return hal_sig2pb(sig, pbsig);
}

int
halpr_describe_ring(hal_ring_t *ring, machinetalk::Ring *pbring)
{
    pbring->set_name(ho_name(ring));
    pbring->set_handle(ho_id(ring));
    pbring->set_total_size(ring->total_size);
    bool halmem = (ring->flags &  ALLOC_HALMEM) != 0;
    pbring->set_rtapi_shm(! halmem);
    if (!halmem)
       pbring->set_ring_shmkey(ring->ring_shmkey);
    // XXX describing more detail would require a temporary attach.
    // FIXME use new attach function options to query flags

    return 0;
}

int halpr_describe_funct(hal_funct_t *funct, machinetalk::Function *pbfunct)
{
    int id;
    hal_comp_t *owner = halpr_find_owning_comp(ho_owner_id(funct));
    if (owner == NULL)
	id = -1;
    else
	id = ho_id(owner);
    pbfunct->set_owner_id(id);
    pbfunct->set_name(ho_name(funct));
    pbfunct->set_handle(ho_id(funct));
    pbfunct->set_users(funct->users);
    pbfunct->set_runtime(get_s32_pin(funct->f_runtime));
    pbfunct->set_maxtime(get_s32_pin(funct->f_maxtime));
    pbfunct->set_reentrant(funct->reentrant);
    return 0;
}

int halpr_describe_thread(hal_thread_t *thread, machinetalk::Thread *pbthread)
{
    pbthread->set_name(ho_name(thread));
    pbthread->set_handle(ho_id(thread));
    pbthread->set_uses_fp(thread->uses_fp);
    pbthread->set_period(thread->period);
    pbthread->set_priority(thread->priority);
    pbthread->set_task_id(thread->task_id);
    pbthread->set_cpu_id(thread->cpu_id);
    pbthread->set_task_id(thread->task_id);

    hal_list_t *list_root = &(thread->funct_list);
    hal_list_t *list_entry = (hal_list_t *) dlist_next(list_root);

    while (list_entry != list_root) {
	hal_funct_entry_t *fentry = (hal_funct_entry_t *) list_entry;
	hal_funct_t *funct = (hal_funct_t *) SHMPTR(fentry->funct_ptr);
	pbthread->add_function(hh_get_name(&funct->hdr));
	list_entry = (hal_list_t *)dlist_next(list_entry);
    }
    return 0;
}


int halpr_describe_member(hal_member_t *member, machinetalk::Member *pbmember)
{
    hal_sig_t *sig = (hal_sig_t *)SHMPTR(member->sig_ptr);
    pbmember->set_mtype(machinetalk::HAL_SIGNAL);
    pbmember->set_userarg1(member->userarg1);
    if (sig->type == HAL_FLOAT)
	pbmember->set_epsilon(hal_data->epsilon[member->eps_index]);
    machinetalk::Signal *pbsig = pbmember->mutable_signal();
    halpr_describe_signal(sig, pbsig);
    return 0;
}

static int describe_member(hal_object_ptr o, foreach_args_t *args)
{
    machinetalk::Group *pbgroup =  (machinetalk::Group *) args->user_ptr1;
    machinetalk::Member *pbmember = pbgroup->add_member();
    halpr_describe_member(o.member, pbmember);
    return 0;
}

int halpr_describe_group(hal_group_t *g, machinetalk::Group *pbgroup)
{
    pbgroup->set_name(ho_name(g));
    pbgroup->set_handle(ho_id(g));

    pbgroup->set_refcount(ho_refcnt(g));
    pbgroup->set_userarg1(g->userarg1);
    pbgroup->set_userarg2(g->userarg2);

    // gcc doesnt grok complex initializers:
    foreach_args_t args =  {};
    args.type = HAL_MEMBER;
    args.owner_id = ho_id(g);
    args.user_ptr1 = (void *) pbgroup;

    halg_foreach(0, &args, describe_member);
    return 0;
}
