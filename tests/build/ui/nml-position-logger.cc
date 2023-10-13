// g++ nml-position-logger.cc -I include -L lib -l nml -l linuxcnc
#include "emc.hh"
#include "emc_nml.hh"
#include <unistd.h>
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv) {
    if(argc < 2) { std::cerr << "Usage: " << argv[0] << " NMLFILE\n"; abort(); }
    const char *nmlfile = argv[1];
    RCS_STAT_CHANNEL *stat = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", nmlfile);
    while(1) {
        usleep(100*1000);
        if(!stat->valid()) continue;
        if(stat->peek() != EMC_STAT_TYPE) continue;
        EMC_STAT *emcStatus = static_cast<EMC_STAT*>(stat->get_address());
        std::cout << emcStatus->motion.traj.position.tran.x << " "
            << emcStatus->motion.traj.position.tran.y << " "
            << emcStatus->motion.traj.position.tran.z << "\n";
    }
    return 0;
}
