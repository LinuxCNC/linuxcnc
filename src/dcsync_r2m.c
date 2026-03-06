#include "priv.h"

static void cycle_start(struct lcec_master *master) {
  master->app_time_ns = dc_time_offset + rtapi_task_pll_get_reference();
  ecrt_master_application_time(master->master, master->app_time_ns);
}

static void pre_send(struct lcec_master *master) {
  if (master->ref_clock_sync_cycles <= 0) {
    return;
  }

  master->ref_clock_sync_counter--;
  if (master->ref_clock_sync_counter <= 0) {
    master->ref_clock_sync_counter = master->ref_clock_sync_cycles;

    // sync reference clock to master
    // use current time here to compensate run time delay, as this get called
    // late in the rt cycle
    ecrt_master_sync_reference_clock_to(master->master, dc_time_offset + rtapi_get_time());
  }

  // call to sync slaves to ref slave
  ecrt_master_sync_slave_clocks(master->master);
}

static void post_send(struct lcec_master *master) {
  // NOP
}

void lcec_dc_init_r2m(struct lcec_master *master) {
  master->dcsync_callbacks.cycle_start = cycle_start;
  master->dcsync_callbacks.pre_send = pre_send;
  master->dcsync_callbacks.post_send = post_send;

  master->app_time_ns = 0;
  master->ref_clock_sync_counter = 0;
}

