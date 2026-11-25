//    Copyright 2026 (c) Alexey Presnyakov
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifndef TIME_ESTIMATE_HH
#define TIME_ESTIMATE_HH

// How long a program takes to run, estimated during the parse.
//
// An addon to GCodeRenderer, fed every move and every event it sees. Not a
// trajectory planner: a 32-move look-ahead over trapezoid profiles, with the
// per-axis velocity and acceleration caps emccanon applies and the cosine
// junction rule standing in for G64 blending. Nominal 100%: overrides,
// adaptive feed, feed hold and M0/M1/M66 waits are not modelled.
//
// Everything here is inches, degrees and seconds.

#include <deque>
#include <string>
#include <vector>

#include "gcodemodule.hh"               // Point9, P9Axis

// Least time between two table samples. A progress display cannot show finer,
// and it bounds the sample count by the program's runtime in seconds rather
// than by its line count - an hour of milling is 3600 rows whether the file
// is a thousand lines or a million.
constexpr double TIME_SAMPLE_RESOLUTION = 1.0;
// Look-ahead depth, TP_DEFAULT_QUEUE_SIZE (tp_types.h).
constexpr size_t TIME_LOOKAHEAD = 32;

// Cumulative seconds at the end of one line's run. Two doubles so the vector
// is an (n, 2) float64 array as it stands; a line number is exact in a double
// far past any file's length.
struct TimeSample { double line, seconds; };

// The samples, thinned as they arrive. A *run* is a maximal stretch of
// consecutive moves sharing a line number; each run's end is a sample, kept
// when it is at least TIME_SAMPLE_RESOLUTION past the last one. Every sample
// is exact - the thinning drops rows, it never averages - and a run dropped
// for landing inside the same second is folded into the next one.
class TimeTable {
public:
    // One finalized move or event, at cumulative `t`.
    void note(int line, double t);
    // Close the table: the run in hand becomes a sample whatever its spacing,
    // so the table always ends at the end of the program.
    void finish(double t);

    const std::vector<TimeSample> &samples() const { return samples_; }

private:
    std::vector<TimeSample> samples_;
    int run_line_ = -1;
    double run_end_ = 0.0;
    bool have_run_ = false;
    double last_t_ = 0.0;
};

// [AXIS_*] and [TRAJ] limits, converted. A zero vmax means the axis does not
// limit (absent, or unstated).
struct MachineLimits {
    Point9 vmax = {};                   // inches/s, degrees/s
    Point9 amax = {};                   // inches/s2, degrees/s2
    double traj_vmax = 0.0;             // 0 = no global cap
    double traj_amax = 0.0;
    double tool_change_seconds = 0.0;

    // Enough to estimate with: some axis, or the global cap, has a velocity.
    bool usable() const;
};

// The feed and spindle modes as the renderer has them at one move.
struct FeedState {
    double rate = 0.0;                  // in/min, or in/rev when per_rev
    bool per_rev = false;               // G95
    double sync_pitch = 0.0;            // in/rev, G33/G76; wins over rate
    double rpm = 0.0;                   // spindle 0's commanded S
    double css_max = 0.0;               // 0 = G97; 1e30 = G96 with no D
    double css_factor = 0.0;            // rpm = css_factor / |program x|
    bool metric = false;                // program units, for the rotary rate
};

// What one parse's estimate comes to: the finished totals, the table, and
// what could not be modelled. Handed to the canon whole, as
// `gcode.TimeEstimate`, and independent of the drawn program - nothing here
// refers to a vertex.
struct TimeEstimateData {
    bool timed = false;                 // false: no usable limits, totals mean
                                        // nothing and total_time reads None
    std::vector<TimeSample> samples;
    double total = 0.0, rapid = 0.0, feed = 0.0, stop = 0.0;
    std::vector<std::string> flags;
};

class TimeEstimator {
public:
    // What could not be modelled, reported as `estimate_flags`.
    enum Flag {
        CssUnbounded,                   // G96 with no D word
        ProbeFullLength,                // a probe timed at its full move
        ZeroFeed,                       // a move with no velocity, skipped
        SyncNoSpindle,                  // G95/G33 with the spindle stopped
        FlagCount
    };

    explicit TimeEstimator(const MachineLimits &limits) : limits_(limits) {}

    // Every move the renderer sees, hidden or not, in machine coordinates.
    // `prog_x_*` are the untransformed X of the endpoints, which is the CSS
    // radius; `arc_radius` is 0 for a straight move.
    void traverse(int line, const Point9 &from, const Point9 &to,
                  const FeedState &state);
    void feed(int line, const Point9 &from, const Point9 &to,
              const FeedState &state, double prog_x_from, double prog_x_to,
              double arc_radius);
    void probe(int line, const Point9 &from, const Point9 &to,
               const FeedState &state, double prog_x_from, double prog_x_to);
    void rigid_tap(int line, const Point9 &from, const Point9 &to,
                   const FeedState &state, double retract_scale);

    // A zero-velocity junction: the queue drains and the next move starts
    // from rest.
    void stop();
    // A stop plus `seconds` of standing still: dwell, tool change, M1xx.
    void fixed(int line, double seconds);
    // End of program.
    void finish();

    double total() const { return total_; }
    double rapid_time() const { return rapid_; }
    double feed_time() const { return feed_; }
    double stop_time() const { return stop_; }
    std::vector<std::string> flags() const;
    TimeTable take_table() { return std::move(table_); }

private:
    // One move waiting in the look-ahead.
    struct Pending {
        int line;
        double length;                  // getStraightVelocity's dtot
        Point9 dir;                     // unit, over the nine
        double v_peak, a, v_start, v_end;
        bool rapid;
    };

    // The per-axis caps of one move, emccanon's rules.
    struct Shape {
        double length = 0.0;
        Point9 dir = {};
        double v_axis = 0.0;            // 0 = no axis limits apply
        double a_axis = 0.0;
        bool cartesian = false, angular = false;
    };

    Shape shape(const Point9 &from, const Point9 &to) const;
    // The commanded velocity, in/s or deg/s, at spindle speed `rpm`.
    double commanded(const FeedState &state, const Shape &s, double rpm);
    // One already-shaped piece into the queue.
    void emit(int line, const Shape &s, double v_command, double arc_radius,
              bool rapid);
    void push(Pending p);
    void propagate_back(size_t i);
    void finalize_front();
    void flush();
    void raise(Flag f) { flags_[f] = true; }

    MachineLimits limits_;
    std::deque<Pending> queue_;
    double carry_v_ = 0.0;              // v_end of the last finalized move
    double total_ = 0.0, rapid_ = 0.0, feed_ = 0.0, stop_ = 0.0;
    bool flags_[FlagCount] = {};
    TimeTable table_;
};

#endif  // TIME_ESTIMATE_HH
