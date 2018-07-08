"""
LinuxCNC User Interface helper functions
"""

import linuxcnc

import sys
import time
import math


class LinuxCNC_Exception(Exception):
    pass


class LinuxCNC:
    """
    This class implements the main interface to LinuxCNC.
    """

    def __init__(self, command=None, status=None, error=None):
        """init docs"""
        self.command = command
        self.status = status
        self.error = error

        if not self.command:
            self.command = linuxcnc.command()

        if not self.status:
            self.status = linuxcnc.stat()

        if not self.error:
            self.error = linuxcnc.error_channel()


    def wait_for_linuxcnc_startup(self, timeout=10.0):

        """Poll the Status buffer waiting for it to look initialized,
        rather than just allocated (all-zero).  Returns on success, throws
        RuntimeError on failure."""

        start_time = time.time()
        while time.time() - start_time < timeout:
            self.status.poll()
            if (self.status.angular_units == 0.0) \
                or (self.status.axes == 0) \
                or (self.status.axis_mask == 0) \
                or (self.status.cycle_time == 0.0) \
                or (self.status.exec_state != linuxcnc.EXEC_DONE) \
                or (self.status.interp_state != linuxcnc.INTERP_IDLE) \
                or (self.status.inpos == False) \
                or (self.status.linear_units == 0.0) \
                or (self.status.max_acceleration == 0.0) \
                or (self.status.max_velocity == 0.0) \
                or (self.status.program_units == 0.0) \
                or (self.status.rapidrate == 0.0) \
                or (self.status.state != linuxcnc.STATE_ESTOP) \
                or (self.status.task_state != linuxcnc.STATE_ESTOP):
                time.sleep(0.1)
            else:
                # looks good
                return

        # timeout, throw an exception
        raise RuntimeError


    def all_joints_homed(self, joints):
        """Arguments:

            'joints' is an array of booleans, indicating the joints to
            check for homed-ness.

        Returns True if all the specified joints are homed, False if
        any are unhomed."""

        self.status.poll()
        for i in range(0,9):
            if joints[i] and not self.status.homed[i]:
                return False
        return True


    def wait_for_home(self, joints, timeout=10.0):
        """Arguments:

            'joints' is an array of booleans, indicating the joints that
            are homing.

            'timeout' is a float, the number of seconds to wait for
            homing before giving up.

        Returns if all the specified joints homed before the timeout,
        raises LinuxCNC_Exception if the timeout expired first."""

        start_time = time.time()
        while (time.time() - start_time) < timeout:
            if self.all_joints_homed(joints):
                return
            time.sleep(0.1)

        raise LinuxCNC_Exception("timeout waiting for homing to complete:\nstatus.homed:" + str(self.status.homed) + "\nstatus.position:" + str(self.status.position))


    def wait_for_axis_to_stop(self, axis_letter, timeout=10.0):
	"""Arguments:
            'axis_letter' is a single character from the set [xyzabcuvw]
            indicating the axis to wait for stillness on.

            'timeout' is a float, the number of seconds to wait for the
            axis to stop before giving up.

        Returns if the specified axis stops moving before the timeout
        expires.  Raises LinuxCNC_Exception if the timeout expires before
        the axis stops.
        """

	axis_letter = axis_letter.lower()
	axis_index = 'xyzabcuvw'.index(axis_letter)
	print "waiting for axis", axis_letter, "to stop"
	self.status.poll()
	start_time = time.time()
	prev_pos = self.status.position[axis_index]
	while (time.time() - start_time) < timeout:
	    time.sleep(0.1)
	    self.status.poll()
	    new_pos = self.status.position[axis_index]
	    if new_pos == prev_pos:
		return
	    prev_pos = new_pos
	raise LinuxCNC_Exception("axis %s didn't stop jogging!\n" % axis_letter + "axis %s is at %.3f %.3f seconds after reaching target (prev_pos=%.3f)\n" % (axis_letter, self.status.position[axis_index], timeout, prev_pos))


    def jog_axis(self, axis_letter, target, vel=5.0, timeout=10.0):
        """Arguments:

            'axis_letter' is a single character from the set [xyzabcuvw]
            indicating an axis.

            'target' is a float indicating the location to jog to.

            'vel' is a float indicating the jog speed.

            'timeout' is a float, the number of seconds to wait for the
            jog to reach the target before giving up.

        This function uses the linuxcnc.command.jog() function to do
        a continuous jog of the specified axis towards the target, at
        the specified speed.  Stops the jog when the target is reached.
        Will overshoot some.  The function returns after the axis has
        stopped moving.

        If the axis does not reach the target before the timeout, or if
        any other axis moved, raises LinuxCNC_Exeption.
        """

	self.status.poll()
	axis_letter = axis_letter.lower()
	axis_index = 'xyzabcuvw'.index(axis_letter)
	start_pos = self.status.position

	print "jogging axis %s from %.3f to %.3f" % (axis_letter, start_pos[axis_index], target)

	if self.status.position[axis_index] < target:
	    vel = abs(vel)
	    done = lambda pos: pos > target
	else:
	    vel = -1.0 * abs(vel)
	    done = lambda pos: pos < target

	# the 0 here means "jog axis, not joint"
	self.command.jog(linuxcnc.JOG_CONTINUOUS, 0, axis_index, vel)

	start = time.time()
	while not done(self.status.position[axis_index]) and ((time.time() - start) < timeout):
	    time.sleep(0.1)
	    self.status.poll()

	# the 0 here means "jog axis, not joint"
	self.command.jog(linuxcnc.JOG_STOP, 0, axis_index)

	if not done(self.status.position[axis_index]):
            raise LinuxCNC_Exception("failed to jog axis %s to %.3f\n" % (axis_letter, target) + "timed out at %.3f after %.3f seconds" % (self.status.position[axis_index], timeout))

	print "    jogged axis %d past target %.3f" % (axis_index, target)

	self.wait_for_axis_to_stop(axis_letter)

	success = True
	for i in range(0, 9):
	    if i == axis_index:
		continue;
	    if start_pos[i] != self.status.position[i]:
		raise LinuxCNC_Exception("axis %s moved from %.3f to %.3f but shouldnt have!" % ('xyzabcuvw'[i], start_pos[i], self.status.position[i]))


    def wait_for_axis_to_stop_at(self, axis_letter, target, timeout=10.0, tolerance = 0.0001):
	"""
        Arguments:
            'axis_letter' is a single character from the set [xyzabcuvw]
            indicating the axis to wait for stillness on.

            'target' is a float, where the specified axis is trying to
            move to.

            'timeout' is a float, the number of seconds to wait for the
            axis to stop before giving up.

            'tolerance' is a float, how close to the target the axis
            has to be before we consider it there.

        This function polls the LinuxCNC Status buffer and monitors the
        position field, waiting for the specified axis to come to rest
        at the specified target.  If this does not happen before the
        timeout, the function raises LinuxCNC_Exception.
        """

	axis_letter = axis_letter.lower()
	axis_index = 'xyzabcuvw'.index(axis_letter)

	self.status.poll()
	start = time.time()

	while ((time.time() - start) < timeout):
	    prev_pos = self.status.position[axis_index]
	    self.status.poll()
	    vel = self.status.position[axis_index] - prev_pos
	    error = math.fabs(self.status.position[axis_index] - target)
	    if (error < tolerance) and (vel == 0):
		print "axis %s stopped at %.3f" % (axis_letter, target)
		return
	    time.sleep(0.1)
	raise LinuxCNC_Exception("timeout waiting for axis %s to stop at %.3f (pos=%.3f, vel=%.3f)" % (axis_letter, target, self.status.position[axis_index], vel))


    def wait_for_interp_state(self, target_state, timeout=10.0):
	"""
	Arguments:
            'target_state' is the Interpreter state to wait for,
            one of linuxcnc.INTERP_IDLE, linuxcnc.INTERP_PAUSED,
            linuxcnc.INTERP_READING, or linuxcnc.INTERP_WAITING.

            'timeout' is a float, the number of seconds to wait for the
            Interpreter to reach the target state.

        This function polls the LinuxCNC Status buffer waiting for the
        specified Interpreter state.  If the timeout expires before
        the Interpreter reaches the specified status, it raises
        LinuxCNC_Exception.
	"""

	start_time = time.time()
	while (time.time() - start_time) < timeout:
	    self.status.poll()
	    if self.status.interp_state == target_state:
		return
	    time.sleep(0.001)

	if self.status.interp_state != target_state:
	    raise LinuxCNC_Exception("interpreter state %d did not reach target state %d" % (self.state.interp_state, target_state))


    def wait_for_tool_in_spindle(self, expected_tool, timeout=10.0):
	"""
        Arguments:
            'expected_tool', integer, the tool we're waiting to find in
            the spindle.

            'timeout', float, the number of seconds to wait for the
            specfied tool to show up in the spindle.

        This function polls the LinuxCNC Status buffer, waiting for the
        specified tool to appear in the spindle.  If the timeout expires
        before the tool appears, it raises LinuxCNC_Exception.
        """

	start_time = time.time()
	while (time.time() - start_time) < timeout:
	    time.sleep(0.1)
            self.status.poll()
	    if self.status.tool_in_spindle == expected_tool:
		print "the Stat buffer's toolInSpindle reached the value of %d after %f seconds" % (expected_tool, time.time() - start_time)
		return
	raise LinuxCNC_Exception("the Stat buffer's toolInSpindle value is %d, expected %d" % (self.status.tool_in_spindle, expected_tool))

