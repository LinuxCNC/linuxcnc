
// the boost-wrapped Interp instance
typedef boost::shared_ptr< Interp > interp_ptr;

typedef boost::shared_ptr< block > block_ptr;

class ParamClass {
public:
    double getitem(bp::object sub);
    double setitem(bp::object sub, double dvalue);
};


extern setup_pointer current_setup;
extern Interp *current_interp;
