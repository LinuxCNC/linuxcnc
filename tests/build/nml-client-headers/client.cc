/* Copyright (c) 2026 All rights reserved.
 * License: GPL Version 2
 *
 * Compiles and links against the installed headers only, the way an
 * out-of-tree NML client does: one include path, no source tree, no
 * generated files.
 */
#include "emc.hh"
#include "emc_nml.hh"
#include "emccfg.h"
#include "emcglb.h"
#include "state_tag.h"

/* The libnml headers keep their directory under $(includedir)/linuxcnc,
   because that is how they include each other. */
#include "libnml/nml/nml.hh"
#include "libnml/nml/nml_oi.hh"

int main()
{
    EMC_STAT status;
    EMC_TASK_SET_MODE set_mode;
    EMC_TRAJ_STAT traj;

    set_mode.mode = EMC_TASK_MODE::MANUAL;
    traj.mode = EMC_TRAJ_MODE::COORD;
    (void)status;
    (void)set_mode;
    (void)traj;

    /* Addresses only: the link has to find the two format functions in
       liblinuxcnc and libnml, which is what proves the libraries that go
       with these headers are installed too. */
    int (*emc_format)(NMLTYPE, void *, CMS *) = &emcFormat;
    int (*nml_error_format)(NMLTYPE, void *, CMS *) = &nmlErrorFormat;
    return emc_format == nullptr || nml_error_format == nullptr;
}
