This tests @sittner's [add-task-pll-functions.patch][1] as ported to
Machinekit in [PR #1373][2].

The test .hal file starts the custom `pll_correction` component, and a
`halsampler` takes `numsamps*4` samples (`numsamps` configured in
`params.py.sh`).

The `pll_correction.comp` HAL component sets the phase to ten times
the period.  The first `numsamps*3` samples should be enough time to
lock the phase, and the final `numsamps` samples are counted to see
how many are within 1% of the expected phase difference.

Finally, the python `checkresult` script parses the `halsampler`
output, checking the final sample.  Most of the samples' PLL phase
error must be within 1% of the total phase shift, or the test is
deemed to have failed.  (This is set very loosely in order to succeed
even on systems with pathological jitter.)

[1]: https://github.com/sittner/linuxcnc-ethercat/blob/c84a868b/patches/add-task-pll-functions.patch
[2]: https://github.com/machinekit/machinekit/pull/1373
