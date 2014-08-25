from .hal_util cimport shmptr #hal2py, py2hal, shmptr, valid_dir, valid_type
from .hal_priv cimport MAX_EPSILON, hal_data
from .hal_group cimport *
from .rtapi cimport  RTAPI_BIT_TEST


cdef class Group:
    cdef hal_group_t *_grp
    cdef hal_compiled_group_t *_cg

    def __cinit__(self, char *name, int arg1=0, int arg2=0):
        hal_required()
        self._cg = NULL
        with HALMutex():
            # look it up
            self._grp = halpr_find_group_by_name(name)

        if self._grp == NULL:
            # not found, create a new group
            r = hal_group_new(name, arg1, arg2)
            if r:
                raise RuntimeError("Failed to create group %s: %s" % (name, hal_lasterror()))
            with HALMutex():
                # retrieve handle on new group
                self._grp = halpr_find_group_by_name(name)
                if self._grp == NULL:
                    raise InternalError("BUG: cannot find group '%s'" % name)
        else:
            # if wrapping existing group and args are nonzero, they better match up
            if arg1:
                if self._grp.userarg1 != arg1:
                    raise RuntimeError("userarg1 does not match for existing group %s: %d, was %d" %
                                       (name, arg1, self._grp.userarg1))
            if arg2:
                if self._grp.userarg2 != arg2:
                    raise RuntimeError("userarg2 does not match for existing group %s: %d, was %d" %
                                       (name, arg2, self._grp.userarg2))

    def signal_members(self):  # member groups resolved into signals
        result = []
        rc = halpr_foreach_member(self._grp.name, _list_signal_members_cb,
                                  <void*>result, RESOLVE_NESTED_GROUPS);
        if rc < 0:
            raise RuntimeError("signal_members: halpr_foreach_member(%s) failed %d" %
                               (self._grp.name,rc))
        return result

    def members(self):  # unresolved - result may contain groups and signals
        result = []
        rc = halpr_foreach_member(self._grp.name, _list_members_cb,
                                  <void*>result, 0);
        if rc < 0:
            raise RuntimeError("members: halpr_foreach_member(%s) failed %d" %
                               (self._grp.name,rc))
        return result

    def changed(self):
        cdef hal_sig_t *s
        if self._cg == NULL:
            self.compile()
        if self._cg.n_monitored == 0:
            return []
        if hal_cgroup_match(self._cg) == 0:
            return []
        result = []
        for i in range(self._cg.n_members):
            if RTAPI_BIT_TEST(self._cg.changed, i):
                s = <hal_sig_t *>shmptr(self._cg.member[i].sig_member_ptr)
                result.append(signals[s.name])
        return result

    def compile(self):
        with HALMutex():
            hal_cgroup_free(self._cg)
            rc =  halpr_group_compile(self._grp.name, &self._cg)
            if rc < 0:
                raise RuntimeError("hal_group_compile(%s) failed: %s" %
                                   (self._grp.name, hal_lasterror()))

    def add(self, member, int arg1=0, int eps_index=0):
        if isinstance(member, Signal) or isinstance(member, Group):
            member = member.name
        rc = hal_member_new(self._grp.name, member, arg1, eps_index)
        if rc:
            raise RuntimeError("Failed to add member '%s' to  group '%s': %s" %
                               (member, self._grp.name, hal_lasterror()))

    def delete(self, member):
        if isinstance(member, Signal) or isinstance(member, Group):
            member = member.name
        rc = hal_member_delete(self._grp.name, member)
        if rc:
            raise RuntimeError("Failed to delete member '%s' from  group '%s': %s" %
                               (member, self._grp.name, hal_lasterror()))

    property name:
        def __get__(self): return self._grp.name

    property refcount:
        def __get__(self): return self._grp.refcount
        def __set__(self, int r): self._grp.refcount = r

    property userarg1:
        def __get__(self): return self._grp.userarg1
        def __set__(self, int r): self._grp.userarg1 = r

    property userarg2:
        def __get__(self): return self._grp.userarg2
        def __set__(self, int r): self._grp.userarg2 = r

    property handle:
        def __get__(self): return self._grp.handle


# see last answer why this is required:
# http://stackoverflow.com/questions/12204441/passing-c-pointer-as-argument-into-cython-function

cdef Member_Init(hal_member_t *m):
      result = Member()
      result._m = m
      return result

cdef class Member:
    cdef hal_member_t *_m
    cdef hal_sig_t *s
    cdef hal_group_t *g

    def __cinit__(self):
        hal_required()

    def _name(self):
        if self._m == NULL:
            raise InternalError("BUG member: _m == NULL")
        if self._m.sig_member_ptr:
            s = <hal_sig_t *>shmptr(self._m.sig_member_ptr)
            return s.name
        if self._m.group_member_ptr:
            g = <hal_group_t *>shmptr(self._m.group_member_ptr)
            return g.name
        raise InternalError("BUG: member: both group_member_ptr and sig_member_ptr zero")

    property item:
        def __get__(self):
            if self._m == NULL:
                raise InternalError("BUG member: _m == NULL")
            if self._m.sig_member_ptr:
                s = <hal_sig_t *>shmptr(self._m.sig_member_ptr)
                return signals[s.name]  # signals dict
            if self._m.group_member_ptr:
                g = <hal_group_t *>shmptr(self._m.group_member_ptr)
                return Group(g.name)
            raise InternalError("BUG: __call__: both group_member_ptr and sig_member_ptr zero")

    property epsilon:
        def __get__(self):
            return hal_data.epsilon[self._m.eps_index]

    property eps:
        def __get__(self): return self._m.eps_index
        def __set__(self, int eps):
            if (eps < 0) or (eps > MAX_EPSILON-1):
                raise InternalError("member %s : epsilon index out of range" % (self._name(), eps))
            self._m.eps_index = eps

    property type:
        def __get__(self):
            if self._m.sig_member_ptr:
                return HAL_SIGNAL
            if self._m.group_member_ptr:
                return HAL_GROUP
            raise InternalError("BUG: member neither a signal nor group!")

    property handle:
        def __get__(self): return self._m.handle

    property userarg1:
        def __get__(self): return self._m.userarg1

def groups():
    ''' return list of group names'''
    hal_required()
    names = []
    with HALMutex():
        rc = halpr_foreach_group(NULL, _collect_group_names, <void *>names);
        if rc < 0:
            raise RuntimeError("halpr_foreach_group failed %d" % rc)
    return names


# callback for the groups() method
cdef int _collect_group_names(hal_group_t *group,  void *userdata):
    arg =  <object>userdata
    arg.append(group.name)
    return 0

# callback for the signal_members() method
cdef int _list_signal_members_cb(int level, hal_group_t **groups,
                          hal_member_t *member,
                          void *userdata):
    cdef hal_sig_t *s = <hal_sig_t *>shmptr(member.sig_member_ptr)
    arg =  <object>userdata
    arg.append(signals[s.name]) # go through signaldict
    return 0

# callback for the members() method
cdef int _list_members_cb(int level, hal_group_t **groups,
                          hal_member_t *member,
                          void *userdata):
    cdef hal_sig_t *s = <hal_sig_t *>shmptr(member.sig_member_ptr)
    arg =  <object>userdata
    arg.append(Member_Init(member))
    return 0
