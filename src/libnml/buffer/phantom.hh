#ifndef PHANTOM_HH
#define PHANTOM_HH

#include "cms.hh"

class PHANTOMMEM:public CMS {
  public:
    PHANTOMMEM(char *bufline, char *procline);
      virtual ~ PHANTOMMEM();
    virtual CMS_STATUS main_access(void *_local);
};

#endif
