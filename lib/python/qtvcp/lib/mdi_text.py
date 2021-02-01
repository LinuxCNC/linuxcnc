def gcode_titles():
    titles = {'G0': 'Coordinated Motion at Rapid Rate',
              'G1': 'Coordinated Motion at Feed Rate',
              'G2': 'Coordinated CW Helical Motion at Feed Rate',
              'G3': 'Coordinated CCW Helical Motion at Feed Rate',
              'G4': 'Dwell',
              'G5': 'Cubic Spline',
              'G5.1': 'Quadratic B-Spline',
              'G5.2': 'NURBS, add control point',
              'G5.3': 'NURBS, close control point',
              'G7': 'Diameter Mode (lathe)',
              'G8': 'Radius Mode (lathe)',
              'G10': 'Offsets L1, L2, L10, L11, L20',
              'G10L1': 'Set Tool Table Entry',
              'G10L2': 'Set Tool Table, Calculated, Workpiece',
              'G10L10': 'Set Tool Table, Calculated, Fixture',
              'G10L11': 'Coordinate System Origin Setting',
              'G10L20': 'Coordinate System Origin Setting Calculated',
              'G17': 'Plane Select XY',
              'G18': 'Plane Select XZ',
              'G19': 'Plane Select YZ',
              'G17.1': 'Plane Select UV',
              'G17.2': 'Plane Select WU',
              'G17.3': 'Plane Select VW',
              'G18.1': 'Plane Select WU',
              'G20': 'Set Units to Inch',
              'G21': 'Set Units to Millimeters',
              'G28': 'Go to Predefined Position',
              'G28.1': 'Set Predefined Position',
              'G30': 'Go to Predefined Position',
              'G30.1': 'Set Predefined Position',
              'G33': 'Spindle Synchronized Motion',
              'G33.1': 'Rigid Tapping',
              'G38.2': 'Probing Toward Workpiece, Signal Error',
              'G38.3': 'Probing Toward Workpiece',
              'G38.4': 'Probing Away Workpiece, Signal Error',
              'G38.5': 'Probing Away Workpiece',
              'G40': 'Cancel Cutter Compensation',
              'G41': 'Cutter Compensation, Left of Programmed Path',
              'G41.1': 'Dynamic Cutter Compensation, Left of Path',
              'G42': 'Cutter Compensation, Right of Programmed Path',
              'G42.1': 'Dynamic Cutter Compensation, Right of Path',
              'G43': 'Use Tool Length Offset from Tool Table',
              'G43.1': 'Dynamic Tool Length Offset',
              'G43.2': 'Apply additional Tool Length Offset',
              'G49': 'Cancel Tool Length Offset',
              'G52': 'Local Coordinate System Offset',
              'G53': 'Move in Machine Coordinates',
              'G54': 'Select Coordinate System 1',
              'G55': 'Select Coordinate System 2',
              'G56': 'Select Coordinate System 3',
              'G57': 'Select Coordinate System 4',
              'G58': 'Select Coordinate System 5',
              'G59': 'Select Coordinate System 6',
              'G59.1': 'Select Coordinate System 7',
              'G59.2': 'Select Coordinate System 8',
              'G59.3': 'Select Coordinate System 9',
              'G61': 'Exact Path Mode',
              'G61.1': 'Exact Stop Mode',
              'G64': 'Path Control Mode with Tolerance',
              'G73': 'Drilling Cycle with Chip Breaking',
              'G74': 'Left-hand Tapping Cycle with Dwell',
              'G76': 'Multi-pass Threading Cycle (Lathe)',
              'G80': 'Cancel Motion Modes',
              'G81': 'Drilling Cycle',
              'G82': 'Drilling Cycle with Dwell',
              'G83': 'Drilling Cycle with Peck',
              'G84': 'Right-hand Tapping Cycle with Dwell',
              'G85': 'Boring Cycle, No Dwell, Feed Out',
              'G86': 'Boring Cycle, Stop, Rapid Out',
              'G89': 'Boring Cycle, Dwell, Feed Out',
              'G90': 'Absolute Distance Mode',
              'G90.1': 'Absolute Distance Mode IJK Arc Offsets',
              'G91': 'Incremental Distance Mode',
              'G91.1': 'Incremental Distance Mode IJK Arc Offsets',
              'G92': 'Coordinate System Offset',
              'G92.1': 'Reset G92 Offsets, Reset Parameters',
              'G92.2': 'Reset G92 Offsets, Keep Parameters ',
              'G92.3': 'Restore G92 Offsets',
              'G93': 'Feed Rate Inverse Time Mode',
              'G94': 'Feed Rate Units per Minute Mode',
              'G95': 'Feed Rate Units per Revolution Mode',
              'G96': 'Spindle Control Constant Surface Speed Mode',
              'G97': 'Spindle Control RPM Mode',
              'G98': 'Canned Cycle Return Level',
              'G99': 'Canned Cycle Return Level',
              'M0': 'Program Pause',
              'M1': 'Program Pause if Optional Stop is Enabled',
              'M2, M30': 'Program End',
              'M3': 'Spindle Start Clockwise',
              'M4': 'Spindle Start Counterclockwise',
              'M5': 'Spindle Stop',
              'M6': 'Tool Change',
              'M7': 'Mist Coolant On',
              'M8': 'Flood Coolant On',
              'M9': 'Flood and Mist Coolant Off',
              'M19': 'Orient Spindle',
              'M48': 'Enable Spindle Speed and Feed Rate Override',
              'M49': 'Disable Spindle Speed and Feed Rate Override',
              'M50': 'Feed Override Control',
              'M51': 'Spindle Speed Override Control',
              'M52': 'Adaptive Feed Control',
              'M53': 'Feed Stop Control',
              'M61': 'Set Current Tool',
              'M62': 'Turn On Digital Output Synchronized with Motion',
              'M63': 'Turn Off Digital Output Synchronized with Motion',
              'M64': 'Turn On Digital Output Immediately',
              'M65': 'Turn Off Digital Output Immediately',
              'M66': 'Wait on Input',
              'M67': 'Set an Analog Output Synchronized with Motion',
              'M68': 'Set an Analog Output Immediately',
              'M70': 'Save Modal State',
              'M71': 'Invalidate Stored Modal State',
              'M72': 'Restore Modal State',
              'M73': 'Save and Autorestore Modal State',
              'M98': 'Call Faunc Style Subroutine',
              'M99': 'Return from Fanuc Style Subroutine', }

    return titles


def gcode_words():
    words = {'G0': ['X', 'Y', 'Z'],
             'G1': ['X', 'Y', 'Z'],
             'G2': ['X', 'Y', 'Z', 'I', 'J', 'K', 'R', 'P'],
             'G3': ['X', 'Y', 'Z', 'I', 'J', 'K', 'R', 'P'],
             'G4': ['P'],
             'G5': ['I', 'J', 'P', 'Q'],
             'G5.1': ['I', 'J'],
             'G5.2': ['P', 'L'],
             'G10': ['L'],
             'G10L1': ['P', 'X', 'Y', 'Z', 'R', 'I', 'J', 'Q'],
             'G10L2': ['P', 'X', 'Y', 'Z', 'R'],
             'G10L10': ['P', 'X', 'Y', 'Z', 'R', 'I', 'J', 'Q'],
             'G10L11': ['P', 'X', 'Y', 'Z', 'R', 'I', 'J', 'Q'],
             'G10L20': ['P', 'X', 'Y', 'Z'],
             'G33': ['X', 'Y', 'Z', 'K', '$'],
             'G33.1': ['X', 'Y', 'Z', 'K', '$'],
             'G38.2': ['X', 'Y', 'Z'],
             'G38.3': ['X', 'Y', 'Z'],
             'G38.4': ['X', 'Y', 'Z'],
             'G38.5': ['X', 'Y', 'Z'],
             'G41': ['D'],
             'G41.1': ['D', 'L'],
             'G42': ['D'],
             'G42.1': ['D', 'L'],
             'G43': ['H'],
             'G43.1': ['X', 'Y', 'Z'],
             'G43.2': ['H'],
             'G52': ['X', 'Y', 'Z'],
             'G53': ['X', 'Y', 'Z'],
             'G64': ['P', 'Q'],
             'G73': ['X', 'Y', 'Z', 'R', 'Q', 'L'],
             'G74': ['X', 'Y', 'Z', 'R', 'L', 'P', '$'],
             'G76': ['P', 'Z', 'I', 'J', 'K', 'Q', 'H', 'E', 'L', '$'],
             'G81': ['X', 'Y', 'Z', 'R', 'L'],
             'G82': ['X', 'Y', 'Z', 'R', 'L', 'P'],
             'G83': ['X', 'Y', 'Z', 'R', 'L', 'Q'],
             'G84': ['X', 'Y', 'Z', 'R', 'L', 'P', '$'],
             'G85': ['X', 'Y', 'Z', 'R', 'L'],
             'G86': ['X', 'Y', 'Z', 'R', 'L', 'P', '$'],
             'G89': ['X', 'Y', 'Z', 'R', 'L', 'P'],
             'G92': ['X', 'Y', 'Z'],
             'G96': ['D', 'S', '$'],
             'G97': ['S', '$'],
             'M1': ['P', 'Q'],
             'M3': ['S', '$'],
             'M4': ['S', '$'],
             'M19': ['R', 'Q', 'P', '$'],
             'M50': ['P'],
             'M51': ['P', '$'],
             'M52': ['P'],
             'M61': ['Q'],
             'M62': ['P'],
             'M63': ['P'],
             'M64': ['P'],
             'M65': ['P'],
             'M66': ['P', 'E', 'L', 'Q'],
             'M67': ['E', 'Q'],
             'M68': ['E', 'Q'], }

    return words


def gcode_descriptions(gcode):
    gcodeTitle = {'G0': G0,
                  'G1': G1,
                  'G2': G2,
                  'G3': G3,
                  'G4': G4,
                  'G5': G5,
                  'G5.1': G5_1,
                  'G5.2': G5_2,
                  'G5.3': G5_3,
                  'G7': G7,
                  'G8': G8,
                  'G10L1': G10L1,
                  'G10L2': G10L2,
                  'G10L10': G10L10,
                  'G10L11': G10L11,
                  'G10L20': G10L20,
                  'G17': G17,
                  'G18': G18,
                  'G19': G19,
                  'G17.1': G17_1,
                  'G18.1': G18_1,
                  'G19.1': G19_1,
                  'G20': G20,
                  'G21': G21,
                  'G28': G28,
                  'G28.1': G28_1,
                  'G30': G30,
                  'G30.1': G30_1,
                  'G33': G33,
                  'G33.1': G33_1,
                  'G38.2': G38_2,
                  'G38.3': G38_3,
                  'G38.4': G38_4,
                  'G38.5': G38_5,
                  'G40': G40,
                  'G41': G41,
                  'G41.1': G41_1,
                  'G42': G42,
                  'G42.1': G42_1,
                  'G43': G43,
                  'G43.1': G43_1,
                  'G43.2': G43_2,
                  'G49': G49,
                  'G52': G52,
                  'G53': G53,
                  'G54': G54,
                  'G55': G55,
                  'G56': G56,
                  'G57': G57,
                  'G58': G58,
                  'G59': G59,
                  'G59.1': G59_1,
                  'G59.2': G59_2,
                  'G59.3': G59_3,
                  'G61': G61,
                  'G61.1': G61_1,
                  'G64': G64,
                  'G73': G73,
                  'G74': G74,
                  'G76': G76,
                  'G80': G80,
                  'G81': G81,
                  'G82': G82,
                  'G83': G83,
                  'G84': G84,
                  'G85': G85,
                  'G86': G86,
                  'G89': G89,
                  'G90': G90,
                  'G90.1': G90_1,
                  'G91': G91,
                  'G91.1': G91_1,
                  'G92': G92,
                  'G92.1': G92_1,
                  'G92.2': G92_2,
                  'G92.3': G92_3,
                  'G93': G93,
                  'G94': G94,
                  'G95': G95,
                  'G96': G96,
                  'G97': G97,
                  'G98': G98,
                  'G99': G99, }

    if gcode in gcodeTitle:
        return gcodeTitle[gcode]
    else:
        return ''


# Maximum Width is 58

G0 = """G0 axes
Coordinated motion at maximum speed
"""

G1 = """G1 axes
Coordinated motion at feed speed
"""

G2 = """G2 Coordinated CW Helical Motion at Feed Rate
Center Format G2 axes offsets <P>
XY plane (G17) Z = helix I = X offset J = Y offset
XZ plane (G18) Y = helix I = X offset K = Z offset
YZ plane (G19) X = helix J = Y offset K = Z offset
P = Number of Turns 
Radius Format G2 axes R <P>
R = Radius from Current Position
"""

G3 = """G3 Coordinated CCW Helical Motion at Feed Rate
Center Format G3 axes offsets <P>
XY plane (G17) Z = helix I = X offset J = Y offset
XZ plane (G18) Y = helix I = X offset K = Z offset
YZ plane (G19) X = helix J = Y offset K = Z offset
P = Number of Turns
Radius Format G3 axes R <P>
R = Radius from Current Position
"""

G4 = """G4 P
Dwell program for P seconds, floating point.
"""

G5 = """G5 Cubic Spline
G5 X Y <I J> P Q
I = X incremental offset from start point to first
    control point
J = Y incremental offset from start point to first
    control point
P = X incremental offset from end point to second
    control point
Q = Y incremental offset from end point to second
    control point

G5 creates a cubic B-spline in the XY plane with
the X and Y axes only.
P and Q must both be specified for every G5
command.
"""

G5_1 = """G5.1 Quadratic Spline
G5.1 X Y I J
I = X incremental offset from start point to
    control point
J = Y incremental offset from start point to
    control point
G5.1 creates a quadratic B-spline in the XY plane
with the X and Y axis only. Not specifying I or J
gives zero offset for the unspecified axis, so one
or both must be given.
"""

G5_2 = """G5.2 is for opening the data block defining a NURBS
"""

G5_3 = """G5.3 is for closing the data block defining a NURBS
"""

G7 = """G7 Lathe Diameter Mode
"""

G8 = """G8 Lathe Radius Mode
"""

G10L1 = """G10 L1 Set Tool Table
G10 L1 P axes <R I J Q>
Set Tool Table
P = tool number
R = radius of tool
I = front angle (lathe)
J = back angle (lathe)
Q = orientation (lathe)
G10 L1 sets the tool table for the P tool number
to the values of the words.
"""

G10L2 = """G10 L2 Set Coordinate System
G10 L2 P <axes R>
P = coordinate system (0-9)
R = rotation about the Z axis
G10 L2 offsets the origin of the axes in the
coordinate system specified to the value of the
axis word.
"""

G10L10 = """G10 L10 Set Tool Table
G10 L10 P axes <R I J Q>
P =- tool number
R = radius of tool
I = front angle (lathe)
J = back angle (lathe)
Q = orientation (lathe)
G10 L10 changes the tool table entry for tool P so
that if the tool offset is reloaded, with the
machine in its current position and with the
current G5x and G52/G92 offsets active, the
current coordinates for the given axes will become
the given values.
"""

G10L11 = """G10 L11 Set Tool Table
G10 L11 P axes <R I J Q>
P = tool number
R = radius of tool
I = front angle (lathe)
J = back angle (lathe)
Q = orientation (lathe)
G10 L11 is just like G10 L10 except that instead
of setting the entry according to the current
offsets, it is set so that the current coordinates
would become the given value if the new tool
offset is reloaded and the machine is placed in
the G59.3 coordinate system without any G52/G92
offset active.
"""

G10L20 = """G10 L20 Set Coordinate System
G10 L20 P- axes
P - coordinate system (0-9)
G10 L20 is similar to G10 L2 except that instead
of setting the offset/entry to the given value, it
is set to a calculated value that makes the
current coordinates become the given value.
"""

G17 = """G17 Plane Select
G17 = XY Plane
"""

G18 = """G18 Plane Select
G18 = ZX Plane
"""

G19 = """G19 Plane Select
G19 = YZ Plane
"""

G17_1 = """G17.1 Plane Select
G17.1 = UV Plane
The UV plane does not support arcs.
"""

G18_1 = """G18.1 Plane Select
G18.1 = WU Plane
The WU plane does not support arcs.
"""

G19_1 = """G19.1 Plane Select
G19.1 = VW Plane
The VW plane does not support arcs.
"""

G20 = """G20 Inch Units
"""

G21 = """G21 Millimeter Units
"""

G28 = """G28 Go to Predefined Position
G28 uses the values stored in parameters 5161-5169
as the X Y Z A B C U V W final point to move to.
The parameter values are absolute machine
coordinates in the native machine units as
specified in the ini file. All axes defined in the
ini file will be moved when a G28 is issued. If no
positions are stored with G28.1 then all axes will
go to the machine origin.

G28 - makes a rapid move from the current position
to the absolute position of the values in
parameters 5161-5166.

G28 axes - makes a rapid move to the position
specified by axes including any offsets, then will
make a rapid move to the absolute position of the
values in parameters 5161-5166 for all axes
specified. Any axis not specified will not move.
"""

G28_1 = """G28.1 Set Predefined Position
G28.1 - stores the current absolute position into
parameters 5161-5166.
"""
G30 = """G30 Go to Predefined Position
G30 uses the values stored in parameters 5181-5189
as the X Y Z A B C U V W final point to move to.
The parameter values are absolute machine
coordinates in the native machine units as
specified in the ini file. All axes defined in the
ini file will be moved when a G28 is issued. If no
positions are stored with G28.1 then all axes will
go to the machine origin.

G30 parameters will be used to move the tool when
a M6 is programmed if TOOL_CHANGE_AT_G30=1 is in
the [EMCIO] section of the ini file.

G30 - makes a rapid move from the current position
to the absolute position of the values in
parameters 5181-5189.

G30 axes - makes a rapid move to the position
specified by axes including any offsets, then will
make a rapid move to the absolute position of the
values in parameters 5181-5189 for all axes
specified. Any axis not specified will not move.
"""

G30_1 = """G30.1 Set Predefined Position
G30.1 - stores the current absolute position into
parameters 5181-5189.
"""

G33 = """G33 Spindle Synchronized Motion
G33 X Y Z K $
K = Distance per revolution
$ = Spindle to use, optional

For spindle-synchronized motion in one direction,
code G33 X- Y- Z- K- where K gives the distance
moved in XYZ for each revolution of the spindle.
"""

G33_1 = """Rigid Tapping
G33.1 X Y Z K I $
K = distance per revolution
I = optional spindle speed multiplier for faster
    return
$ = optional spindle selector

G33.1 moves from the current coordinate to the
specified coordinate, synchronized with the
selected spindle at the given ratio and starting
from the current coordinate with a spindle index
pulse.

Note: To tap a straight hole a pre-position move
to the desired X Y coordinates before issuing a
G33.1.
"""

G38_2 = """G38.2 Straight Probe
G38.2 axes

G38.2 - probe toward workpiece, stop on contact,
signal error if failure

Program G38.2 axes to perform a straight probe
operation. The axis words are optional, except
that at least one of them must be used. The axis
words together define the destination point that
the probe will move towards, starting from the
current location. If the probe is not tripped
before the destination is reached G38.2 will
signal an error.
"""

G38_3 = """G38.3 Straight Probe
G38.3 axes

G38.3 - probe toward workpiece, stop on contact

Program G38.2 axes to perform a straight probe
operation. The axis words are optional, except
that at least one of them must be used. The axis
words together define the destination point that
the probe will move towards, starting from the
current location. If the probe is not tripped
before the destination is reached G38.2 will
signal an error.
"""

G38_4 = """G38.4 Straight Probe
G38.4 axes

G38.4 = probe away from workpiece, stop on loss of
contact, signal error if failure

Program G38.4 axes to perform a straight probe
operation. The axis words are optional, except
that at least one of them must be used. The axis
words together define the destination point that
the probe will move towards, starting from the
current location. If the probe is not tripped
before the destination is reached G38.4 will
signal an error.
"""

G38_5 = """G38.5 Straight Probe
G38.5 axes

G38.5 = probe away from workpiece,
        stop on loss of contact

Program G38.5 axes to perform a straight probe
operation. The axis words are optional, except
that at least one of them must be used. The axis
words together define the destination point that
the probe will move towards, starting from the
current location.
"""

G40 = """G40 Cutter Compensation Off
G40 - turn cutter compensation off. If tool
compensation was on the next move must be a linear
move and longer than the tool diameter.
"""

G41 = """G41 Cutter Compensation Left of Path
G41 D
D = tool number

The D word is optional; if there is no D word the
radius of the currently loaded tool will be used
(if no tool is loaded and no D word is given,
a radius of 0 will be used).
"""

G41_1 = """G41.1 Dynamic Cutter Compensation Left of Path
G41.1 D L
D = cutter diameter
L = lathe tool orientation


"""

G42 = """G42 Cutter Compensation Right of Path
G42 D
D = tool number

The D word is optional; if there is no D word the
radius of the currently loaded tool will be used
(if no tool is loaded and no D word is given,
a radius of 0 will be used).
"""

G42_1 = """G42.1 Dynamic Cutter Compensation Right of Path
G41.1 D L
D = cutter diameter
L = lathe tool orientation


"""
G43 = """G43 Tool Length Offset
G43 H
H = tool number (optional)

G43 enables tool length compensation. G43 changes
subsequent motions by offsetting the axis
coordinates by the length of the offset. G43 does
not cause any motion. The next time a compensated
axis is moved, that axis's endpoint is the
compensated location.

G43 without an H word uses the currently loaded
tool from the last Tn M6.

G43 Hn uses the offset for tool n.
"""

G43_1 = """G43.1 Dynamic Tool Length Offset
G43.1 axes

G43.1 axes - change subsequent motions by
replacing the current offset(s) of axes. G43.1
does not cause any motion. The next time a
compensated axis is moved, that axis's endpoint is
the compensated location.
"""

G43_2 = """G43.2 Apply additional Tool Length Offset
G43.2 H
H - tool number

G43.2 applies an additional simultaneous tool
offset.

You can sum together an arbitrary number of
offsets by calling G43.2 more times. There are no
built-in assumptions about which numbers are
geometry offsets and which are wear offsets, or
that you should have only one of each.

Like the other G43 commands, G43.2 does not cause
any motion. The next time a compensated axis is
moved, that axis's endpoint is the compensated
location.
"""

G49 = """G49 Cancel Tool Length Compensation
"""

G52 = """G52 Local Coordinate System Offset
G53 axes

G52 is used in a part program as a temporary
"local coordinate system offset" within the
workpiece coordinate system.
"""

G53 = """G53 Move in Machine Coordinates
G53 axes

To move in the machine coordinate system, program
G53 on the same line as a linear move. G53 is not
modal and must be programmed on each line. G0 or
G1 does not have to be programmed on the same
line if one is currently active.
"""

G54 = """G54 Select Coordinate System
G54 = select coordinate system 1
"""

G55 = """G55 Select Coordinate System
G55 = select coordinate system 2
"""

G56 = """G56 Select Coordinate System
G56 = select coordinate system 3
"""

G57 = """G57 Select Coordinate System
G57 = select coordinate system 4
"""

G58 = """G58 Select Coordinate System
G58 = select coordinate system 5
"""

G59 = """G59 Select Coordinate System
G59 = select coordinate system 6
"""

G59_1 = """G59.1 Select Coordinate System
G59.1 = select coordinate system 7
"""

G59_2 = """G59.2 Select Coordinate System
G59.2 = select coordinate system 8
"""

G59_3 = """G59.3 Select Coordinate System
G59.3 = select coordinate system 9
"""

G61 = """G61 Exact Path Mode
G61 = Exact path mode, movement exactly as
programmed. Moves will slow or stop as needed to
reach every programmed point. If two sequential
moves are exactly co-linear movement will not stop
"""

G61_1 = """G61.1 Exact Stop Mode
G61.1 - Exact stop mode, movement will stop at the
end of each programmed segment.
"""

G64 = """G64 Path Blending
G64 P Q
P = motion blending tolerance
Q = naive cam tolerance

G64 - without P means to keep the best speed
possible, no matter how far away from the
programmed point you end up.

The P tolerance means that the actual path will be
no more than P away from the programmed endpoint.
The velocity will be reduced if needed to maintain
the path.

G64 P- Q- turns on the naive cam detector. When
there are a series of linear XYZ feed moves at the
same feed rate that are less than Q away from
being collinear, they are collapsed into a single
linear move.
"""

G73 = """G73 Drilling Cycle with Chip Breaking
G73 X Y Z R Q L
R = retract position along the Z axis
Q = delta increment along the Z axis
L = repeat

The G73 cycle is drilling or milling with chip
breaking. This cycle takes a Q number which
represents a delta increment along the Z axis.

Motion Sequence

If the current Z position is below the R position,
The Z axis does a rapid move to the R position.

Move to the X Y coordinates.

Move the Z-axis only at the current feed rate
downward by delta or to the Z position, whichever
is less deep.

Rapid up a bit.

Repeat steps 2 and 3 until the Z position is
reached at step 2.

The Z axis does a rapid move to the R position.
"""

G74 = """G74 Left-hand Tapping Cycle, Dwell
G74 (X Y Z) or (U V W) R L P $
R = retract position along the Z axis
L = repeat
P = dwell
$ = spindle

The G74 cycle is for tapping with floating chuck
and dwell at the bottom of the hole.
"""

G76 = """Lathe Threading Cycle
G76 P Z I J R K Q H E L $
P = The thread pitch in distance per revolution.
Z = The final position of threads.
I = The thread peak offset from the drive line.
J = A positive value specifying the initial cut
    depth.
K = A positive value specifying the full thread
    depth.

Optional settings
$ = spindle to which the motion will be
    synchronised.
R = The depth degression. R1.0 selects constant
    depth on successive threading passes.
Q = The compound slide angle is the angle
    (in degrees) describing to what extent
    successive passes should be offset along the
    drive line.
H = The number of spring passes.
E = Specifies the distance along the drive line
    used for the taper.
L = Specifies which ends of the thread get the
    taper.
    L0 for no taper (the default)
    L1 for entry taper
    L2 for exit taper
    L3 for both entry and exit tapers
    
"""

G80 = """Cancel Canned Cycle
"""

G81 = """G81 Drilling Cycle
G81 X Y Z or (U V W) R L

R = retract position along the Z axis
L = repeat
"""

G82 = """G82 Drilling Cycle, Dwell
G82 X Y Z or (U V W) R L P

R = retract position along the Z axis
L = repeat
P = dwell
"""

G83 = """G83 Peck Drilling Cycle
G83 X Y Z or (U V W) R L Q

R = retract position along the Z axis
L = repeat
Q = delta increment along the Z axis
"""

G84 = """G84 Right-hand Tapping Cycle, Dwell
G84 X Y Z or (U V W) R L P $

R = retract position along the Z axis
L = repeat
P = dwell
$ = spindle
"""

G85 = """G85 Boring Cycle, Feed Out
G85 X Y Z or (U V W) R L

R = retract position along the Z axis
L = repeat
"""

G86 = """G86 Boring Cycle, Spindle Stop, Rapid Move Out
G84 X Y Z or (U V W) R L P $

R = retract position along the Z axis
L = repeat
P = dwell
$ = spindle
"""

G89 = """G89 Boring Cycle, Dwell, Feed Out
G89 X Y Z or (U V W) R L P

R = retract position along the Z axis
L = repeat
P = dwell
"""

G90 = """G90 Absolute Distance Mode
In absolute distance mode, axis numbers
(X, Y, Z, A, B, C, U, V, W) usually represent
positions in terms of the currently active
coordinate system.
"""

G90_1 = """G90.1 Absolute Distance Mode
for I, J & K offsets
"""

G91 = """G91 Incremental Distance Mode
In incremental distance mode axis numbers usually
represent increments from the current coordinate.
"""

G91_1 = """G91.1 Incremental Distance Mode
for I, J & K offsets
"""

G92 = """G92 Coordinate System Offset
G92 axes

G92 makes the current point have the coordinates
you want (without motion), where the axis words
contain the axis numbers you want. All axis words
are optional, except that at least one must be
used. If an axis word is not used for a given
axis, the offset for that axis will be zero.
"""

G92_1 = """G92.1 Turn Off G92, Reset
G92.1 turns off G92 offsets and resets
parameters 5211 - 5219 to zero.
"""

G92_2 = """G92.2 Turn Off G92
G92.2 turns off G92 offsets and does not
change parameters 5211 - 5219.
"""

G92_3 = """G92.3 Restore G92 Offsets
G92.3 sets the G92 offset to the values
saved in parameters 5211 to 5219.
"""

G93 = """G93 Inverse Time Feed Mode
G93 inverse time feed rate mode, an F word means
the move should be completed in [one divided by
the F number of minutes.

When the inverse time feed rate mode is active, an
F word must appear on every line which has a G1,
G2, or G3 motion, and an F word on a line that
does not have G1, G2, or G3 is ignored.
"""

G94 = """G94 Units per Minute Feed Mode
G94 units per minute feed mode, an F word is
interpreted to mean the controlled point should
move at a certain number of inches per minute,
millimeters per minute, or degrees per minute,
depending upon what length units are being used
and which axis or axes are moving.
"""

G95 = """G95 Units per Revolution Feed Mode
G95 units per revolution mode, an F word is
interpreted to mean the controlled point should
move a certain number of inches per revolution of
the spindle, depending on what length units are
being used and which axis or axes are moving.
"""

G96 = """G96 Spindle Constant Surface Speed Mode
G96 D S $
D = maximum spindle RPM
S = surface speed
$ = spindle
"""

G97 = """G97 Spindle RPM Mode
G97 selects RPM mode.
"""

G98 = """G98 Canned Cycle Return Level
G98 - retract to the position that axis was in
just before this series of one or more contiguous
canned cycles was started.
"""

G99 = """G99 Canned Cycle Return Level
G99 - retract to the position specified
by the R word of the canned cycle.
"""
