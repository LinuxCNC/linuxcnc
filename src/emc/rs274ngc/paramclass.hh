struct ParamClass {

    Interp &interp;

    ParamClass(Interp &i);
    double getitem( bp::object sub);
    double setitem(bp::object sub, double dvalue);
    bp::list namelist(context &c) const;
    bp::list locals();
    bp::list globals();
    bp::list operator()() const;
    int length();
};

extern void export_ParamClass();
