/********************************************************************
* Description: axis_kinds.hh
*   Which of the nine axes are lengths and which are angles, and which
*   axes the programmed feed is measured along, as the INI file says.
*   The interpreter and canon both read it, so that they measure a move
*   the same way.
*
* License: GPL Version 2
********************************************************************/
#ifndef AXIS_KINDS_HH
#define AXIS_KINDS_HH

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <string>
#include <inifile.hh>

#define AXIS_KINDS_ALL     0x1ffu   /* X Y Z A B C U V W, bit 0 is X */
#define AXIS_KINDS_ABC     0x038u   /* angular unless the INI says otherwise */
#define AXIS_KINDS_XYZ     0x007u   /* the feed group unless the INI says otherwise */

struct AxisKinds {
    unsigned angular;   /* a bit per axis whose [AXIS_<letter>] TYPE is ANGULAR */
    unsigned feed;      /* a bit per axis in [TRAJ] FEED_AXES */
};

static const char axis_kinds_letters[] = "XYZABCUVW";

inline AxisKinds axisKindsDefault()
{
    return AxisKinds{AXIS_KINDS_ABC, AXIS_KINDS_XYZ};
}

inline bool axisKindsAngular(const AxisKinds &k, int axis)
{
    return (k.angular >> axis) & 1;
}

/* Read [AXIS_<letter>] TYPE for every letter, the default being the
   letter's (A B C angular, the others linear), and [TRAJ] FEED_AXES, the
   default XYZ.  X Y Z are always linear: arcs, cutter compensation and
   tool length take them as lengths.  Returns 0, or -1 with *err set when
   a TYPE is neither LINEAR nor ANGULAR, X Y or Z is ANGULAR, or FEED_AXES
   names a letter that is not a linear axis. */
inline int axisKindsRead(const linuxcnc::IniFile &ini, AxisKinds *k, std::string *err)
{
    *k = axisKindsDefault();
    for (int i = 0; i < 9; i++) {
        char section[] = "AXIS_X";
        section[5] = axis_kinds_letters[i];
        auto type = ini.findString("TYPE", section);
        if (!type) { continue; }
        if (*type == "ANGULAR") {
            k->angular |= 1u << i;
        } else if (*type == "LINEAR") {
            k->angular &= ~(1u << i);
        } else {
            *err = "[" + std::string(section) + "] TYPE must be LINEAR or ANGULAR, not " + *type;
            return -1;
        }
    }
    if (k->angular & AXIS_KINDS_XYZ) {
        *err = "[AXIS_X], [AXIS_Y] and [AXIS_Z] TYPE must be LINEAR";
        return -1;
    }
    auto feed = ini.findString("FEED_AXES", "TRAJ");
    if (!feed) { return 0; }
    k->feed = 0;
    for (char ch : *feed) {
        if (ch == ' ' || ch == '\t') { continue; }
        const char *at = strchr(axis_kinds_letters, toupper((unsigned char)ch));
        if (!at || !*at) {
            *err = std::string("[TRAJ] FEED_AXES: ") + ch + " is not an axis letter";
            return -1;
        }
        int i = at - axis_kinds_letters;
        if (axisKindsAngular(*k, i)) {
            *err = std::string("[TRAJ] FEED_AXES: ") + axis_kinds_letters[i] + " is an ANGULAR axis; the feed group holds linear axes only";
            return -1;
        }
        k->feed |= 1u << i;
    }
    if (!k->feed) {
        *err = "[TRAJ] FEED_AXES names no axis";
        return -1;
    }
    return 0;
}

/* The axes a move is measured along, given a bit per axis that moves: the
   feed group when any of it moves, else the other linear axes, else the
   angular axes, in degrees.  Every axis of the chosen set counts, moving
   or not.  With the defaults this is XYZ, else UVW, else ABC. */
inline unsigned axisKindsMeasured(const AxisKinds &k, unsigned moving)
{
    unsigned linear = ~k.angular & ~k.feed & AXIS_KINDS_ALL;
    if (moving & k.feed) { return k.feed; }
    if (moving & linear) { return linear; }
    return k.angular & ~k.feed & AXIS_KINDS_ALL;
}

/* Whether a set from axisKindsMeasured() is angles. */
inline bool axisKindsMeasuredAngular(const AxisKinds &k, unsigned set)
{
    return (set & k.angular) != 0;
}

/* The Euclidean length of the deltas d over the axes of set, summed in
   axis order. */
inline double axisKindsLength(unsigned set, const double d[9])
{
    double sum = 0.0;
    for (int i = 0; i < 9; i++) {
        if (set & (1u << i)) { sum += d[i] * d[i]; }
    }
    return sqrt(sum);
}

#endif
