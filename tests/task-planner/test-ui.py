#!/usr/bin/env python3

# Bring task into each situation the planner tells apart and have
# test_task_probe send it every command there, one at a time. For each probe
# this prints
#
#     PROBE <situation> <command> <serial> <situation afterwards, or ->
#
# checkresult then reads, from task's own output, what the planner did with
# the command, and compares the table with the expected one.

import linuxcnc
import os
import subprocess
import sys
import time

program_start = time.time()

def log(msg):
    print("%.3f: %s" % (time.time() - program_start, msg))
    sys.stdout.flush()


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

probe = subprocess.Popen(['test_task_probe', '-ini', os.environ['INI_FILE_NAME']],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def probe_cmd(line):
    probe.stdin.write(line + "\n")
    probe.stdin.flush()
    return probe.stdout.readline().strip()

probe.stdin.write("list\n")
probe.stdin.flush()
commands = []
while True:
    name = probe.stdout.readline().strip()
    if name == 'end' or name == '':
        break
    commands.append(name)

# Commands from EMC_TOOL_LOAD_TOOL_TABLE on (test_task_probe lists them
# last) set the situation up again afterwards, changed or not: they may
# leave something behind that the situation doesn't show, such as a wait
# for the interpreter, a pause pending, or a move in motion.
first_unsafe = commands.index('EMC_TOOL_LOAD_TOOL_TABLE')

reading = os.path.abspath('reading.ngc')
waiting = os.path.abspath('waiting.ngc')
flush = os.path.abspath('flush.ngc')

# The three tool commands set task's wait flag (emcTaskPlanSetWait()), and
# only a program reading or an MDI command waiting clears it again; an
# abort does not. While it is set, an MDI command in MDI mode is queued
# rather than issued. So that a probe does not depend on the ones before it,
# the flag is cleared, by running a program that ends at once, whenever a
# situation is set up afresh and after each tool command.
TOOL_COMMANDS = {'EMC_TOOL_LOAD_TOOL_TABLE', 'EMC_TOOL_SET_NUMBER', 'EMC_TOOL_SET_OFFSET'}

STATE = {linuxcnc.STATE_ESTOP: 'ESTOP', linuxcnc.STATE_ESTOP_RESET: 'OFF',
         linuxcnc.STATE_OFF: 'OFF', linuxcnc.STATE_ON: 'ON'}
MODE = {linuxcnc.MODE_MANUAL: 'MANUAL', linuxcnc.MODE_AUTO: 'AUTO',
        linuxcnc.MODE_MDI: 'MDI'}
INTERP = {linuxcnc.INTERP_IDLE: 'IDLE', linuxcnc.INTERP_READING: 'READING',
          linuxcnc.INTERP_PAUSED: 'PAUSED', linuxcnc.INTERP_WAITING: 'WAITING'}


def situation():
    s.poll()
    return "%s/%s/%s%s%s" % (STATE[s.task_state], MODE[s.task_mode],
                             INTERP[s.interp_state],
                             '/queued-mdi' if s.queued_mdi_commands else '',
                             '/' + os.path.basename(s.file) if s.file else '')


def drain_errors():
    while e.poll():
        pass


def wait_for(what, cond, timeout=10.0):
    end = time.time() + timeout
    while time.time() < end:
        s.poll()
        if cond():
            return
        time.sleep(0.01)
    log("timeout waiting for %s, situation %s" % (what, situation()))
    sys.exit(1)


# Each step only when it is needed: setting up again is most of the time
# this test takes.

def command(what, cond, *args):
    what(*args)
    # a program run is EXEC until it ends: wait for the state it is after
    if what != c.auto:
        c.wait_complete()
    wait_for(what.__name__, cond)


def state(st):
    s.poll()
    if st == linuxcnc.STATE_ON and s.task_state != linuxcnc.STATE_ON:
        if s.task_state == linuxcnc.STATE_ESTOP:
            command(c.state, lambda: s.task_state != linuxcnc.STATE_ESTOP,
                    linuxcnc.STATE_ESTOP_RESET)
        command(c.state, lambda: s.task_state == linuxcnc.STATE_ON, linuxcnc.STATE_ON)
    elif st == linuxcnc.STATE_ESTOP and s.task_state != linuxcnc.STATE_ESTOP:
        command(c.state, lambda: s.task_state == linuxcnc.STATE_ESTOP, linuxcnc.STATE_ESTOP)
    elif st == linuxcnc.STATE_OFF and s.task_state not in (linuxcnc.STATE_OFF,
                                                            linuxcnc.STATE_ESTOP_RESET):
        if s.task_state == linuxcnc.STATE_ESTOP:
            command(c.state, lambda: s.task_state != linuxcnc.STATE_ESTOP,
                    linuxcnc.STATE_ESTOP_RESET)
        else:
            command(c.state, lambda: s.task_state != linuxcnc.STATE_ON, linuxcnc.STATE_OFF)


def idle():
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE or s.queued_mdi_commands:
        command(c.abort, lambda: s.interp_state == linuxcnc.INTERP_IDLE)


def mode(m):
    s.poll()
    if s.task_mode != m:
        command(c.mode, lambda: s.task_mode == m, m)


def program(f):
    s.poll()
    if s.file != f:
        c.program_open(f)
        c.wait_complete()


def run(f, interp):
    state(linuxcnc.STATE_ON)
    idle()
    mode(linuxcnc.MODE_AUTO)
    program(f)
    command(c.auto, lambda: s.interp_state == interp, linuxcnc.AUTO_RUN, 0)


def clear_wait_flag():
    state(linuxcnc.STATE_ON)
    idle()
    mode(linuxcnc.MODE_AUTO)
    program(flush)
    c.auto(linuxcnc.AUTO_RUN, 0)
    wait_for("flush.ngc to end", lambda: s.interp_state == linuxcnc.INTERP_IDLE
             and s.exec_state == linuxcnc.EXEC_DONE)


def set_up(name, afresh=False):
    if afresh:
        clear_wait_flag()
    # the same program is open in every situation, so that the open file
    # is part of the situation without being set up again for each one
    if name in ('estop', 'off', 'manual', 'mdi', 'mdi-busy', 'auto'):
        idle()
        program(reading)
    if name == 'estop':
        mode(linuxcnc.MODE_MANUAL)
        state(linuxcnc.STATE_ESTOP)
    elif name == 'off':
        mode(linuxcnc.MODE_AUTO)
        state(linuxcnc.STATE_OFF)
    elif name in ('manual', 'mdi', 'auto'):
        state(linuxcnc.STATE_ON)
        mode({'manual': linuxcnc.MODE_MANUAL, 'mdi': linuxcnc.MODE_MDI,
              'auto': linuxcnc.MODE_AUTO}[name])
    elif name == 'mdi-busy':
        state(linuxcnc.STATE_ON)
        mode(linuxcnc.MODE_MDI)
        c.mdi("G4 P600")
        wait_for("MDI running", lambda: s.interp_state != linuxcnc.INTERP_IDLE)
    elif name == 'auto-reading':
        run(reading, linuxcnc.INTERP_READING)
    elif name == 'auto-waiting':
        run(waiting, linuxcnc.INTERP_WAITING)
    elif name == 'auto-paused':
        run(reading, linuxcnc.INTERP_READING)
        command(c.auto, lambda: s.interp_state == linuxcnc.INTERP_PAUSED, linuxcnc.AUTO_PAUSE)
    time.sleep(0.02)
    drain_errors()
    return situation()


SITUATIONS = ['estop', 'off', 'manual', 'mdi', 'mdi-busy',
              'auto', 'auto-reading', 'auto-waiting', 'auto-paused']

for name in SITUATIONS:
    expected = set_up(name, afresh=True)
    log("situation %s: %s" % (name, expected))
    for i, cmd in enumerate(commands):
        reply = probe_cmd("probe %s %s" % (name, cmd))
        if not reply.startswith('serial ') or int(reply.split()[1]) < 0:
            log("probe %s %s failed: %s" % (name, cmd, reply))
            sys.exit(1)
        serial = int(reply.split()[1])
        time.sleep(0.05)
        after = situation()
        print("PROBE %s %s %d %s" % (name, cmd, serial,
                                     after if after != expected else '-'))
        sys.stdout.flush()
        if after != expected or i >= first_unsafe:
            if set_up(name, afresh=cmd in TOOL_COMMANDS) != expected:
                log("cannot set up %s again after %s: %s" % (name, cmd, situation()))
                sys.exit(1)
        drain_errors()

probe.stdin.close()
probe.wait()

c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()
log("done")
sys.exit(0)
