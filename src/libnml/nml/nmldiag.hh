#ifndef NMLDIAG_HH
#define NMLDIAG_HH

#include "cmsdiag.hh"

class NML_DIAGNOSTICS_INFO:public CMS_DIAGNOSTICS_INFO {
  public:
    void print();
};

extern "C" int nml_print_diag_list();

#endif
