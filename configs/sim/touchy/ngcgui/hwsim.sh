#!/bin/bash
# simulate hardware buttons and jogwheel for touchy
# Note: some pins are not used in some configs

# identity kins assumed: connect one-to-one joints & axis letters

halcmd net :xen touchy.jog.wheel.x  joint.0.jog-enable axis.x.jog-enable
halcmd net :yen touchy.jog.wheel.y  joint.1.jog-enable axis.y.jog-enable
halcmd net :zen touchy.jog.wheel.z  joint.2.jog-enable axis.z.jog-enable

halcmd net :scale  touchy.jog.wheel.increment
halcmd net :scale  joint.0.jog-scale axis.x.jog-scale
halcmd net :scale  joint.1.jog-scale axis.y.jog-scale
halcmd net :scale  joint.2.jog-scale axis.z.jog-scale

halcmd net :counts touchy.wheel-counts
halcmd net :counts joint.0.jog-counts axis.x.jog-counts
halcmd net :counts joint.1.jog-counts axis.y.jog-counts
halcmd net :counts joint.2.jog-counts axis.z.jog-counts

sim_pin \
touchy.cycle-start/mode=pulse \
  touchy.abort/mode=pulse \
  halui.program.pause/mode=pulse \
  motion.feed-hold/mode=hold \
  touchy.jog.continuous.x.negative/mode=hold \
  touchy.jog.continuous.x.positive/mode=hold \
  touchy.jog.continuous.y.negative/mode=hold \
  touchy.jog.continuous.y.positive/mode=hold \
  touchy.jog.continuous.z.negative/mode=hold \
  touchy.jog.continuous.z.positive/mode=hold \
  :counts/mode=pulse \
  >/dev/null
