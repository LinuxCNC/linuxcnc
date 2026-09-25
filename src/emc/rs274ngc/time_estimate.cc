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

#include "time_estimate.hh"

#include <algorithm>
#include <cmath>

namespace {

// sqrt(1 - BLEND_ACC_RATIO_TANGENTIAL^2), the share of the acceleration a TP
// blend leaves for the normal direction (blendmath.h).
constexpr double BLEND_ACC_RATIO_NORMAL = 0.86602540378443865;

// Largest radius ratio one CSS piece may span before it is split.
constexpr double CSS_SPLIT_RATIO = 1.25;
constexpr int CSS_MAX_PIECES = 32;

// A G96 with no D word arrives as this (interp_convert.cc).
constexpr double CSS_NO_LIMIT = 1e29;

}  // namespace

// ---------------------------------------------------------------------------
// The table
// ---------------------------------------------------------------------------

void TimeTable::note(int line, double t) {
    // The run that just ended is a sample, unless it lands inside the same
    // second as the last one.
    if(have_run_ && line != run_line_
       && run_end_ - last_t_ >= TIME_SAMPLE_RESOLUTION) {
        samples_.push_back({(double)run_line_, run_end_});
        last_t_ = run_end_;
    }
    run_line_ = line;
    run_end_ = t;
    have_run_ = true;
}

void TimeTable::finish(double t) {
    if(!have_run_) return;
    have_run_ = false;
    if(!samples_.empty() && samples_.back().line == (double)run_line_
       && samples_.back().seconds == t)
        return;
    samples_.push_back({(double)run_line_, t});
}

// ---------------------------------------------------------------------------
// Limits
// ---------------------------------------------------------------------------

bool MachineLimits::usable() const {
    if(traj_vmax > 0.0) return true;
    for(int i = 0; i < P9_COUNT; i++)
        if(vmax[i] > 0.0) return true;
    return false;
}

// ---------------------------------------------------------------------------
// One move's shape
// ---------------------------------------------------------------------------

TimeEstimator::Shape TimeEstimator::shape(const Point9 &from,
                                          const Point9 &to) const {
    Shape s;
    Point9 d;
    for(int i = 0; i < P9_COUNT; i++) d[i] = to[i] - from[i];

    bool xyz = d[P9_X] || d[P9_Y] || d[P9_Z];
    bool uvw = d[P9_U] || d[P9_V] || d[P9_W];
    s.cartesian = xyz || uvw;
    s.angular = d[P9_A] || d[P9_B] || d[P9_C];
    if(!s.cartesian && !s.angular) return s;

    // dtot and the direction are over one triple, getStraightVelocity's rule:
    // XYZ if it moves, else UVW, else the rotary axes.
    int first = s.cartesian ? (xyz ? P9_X : P9_U) : P9_A;
    double sum = 0.0;
    for(int i = first; i < first + 3; i++) sum += d[i] * d[i];
    s.length = sqrt(sum);
    if(s.length <= 0.0) return s;
    for(int i = first; i < first + 3; i++) s.dir[i] = d[i] / s.length;

    // A combination move is capped by every axis; a pure one by its own kind.
    int lo = s.cartesian ? P9_X : P9_A;
    int hi = s.angular ? (s.cartesian ? P9_W : P9_C) : P9_W;
    double tv = 0.0, ta = 0.0;
    for(int i = lo; i <= hi; i++) {
        double dist = fabs(d[i]);
        if(dist <= 0.0) continue;
        if(limits_.vmax[i] > 0.0) tv = std::max(tv, dist / limits_.vmax[i]);
        if(limits_.amax[i] > 0.0) ta = std::max(ta, dist / limits_.amax[i]);
    }
    s.v_axis = tv > 0.0 ? s.length / tv : 0.0;
    s.a_axis = ta > 0.0 ? s.length / ta : 0.0;
    return s;
}

double TimeEstimator::commanded(const FeedState &st, const Shape &s,
                                double rpm) {
    if(st.sync_pitch > 0.0 || st.per_rev) {
        double pitch = st.sync_pitch > 0.0 ? st.sync_pitch : st.rate;
        if(rpm <= 0.0 || pitch <= 0.0) {
            raise(SyncNoSpindle);
            return 0.0;
        }
        return pitch * rpm / 60.0;
    }
    // A rotary-only move's F is degrees per minute, and SET_FEED_RATE divided
    // it by 25.4 anyway under G21. Put that back.
    double rate = st.rate;
    if(s.angular && !s.cartesian && st.metric) rate *= 25.4;
    return rate / 60.0;
}

// ---------------------------------------------------------------------------
// The look-ahead
// ---------------------------------------------------------------------------

void TimeEstimator::emit(int line, const Shape &s, double v_command,
                         double arc_radius, bool rapid) {
    if(s.length <= 0.0) return;
    double v = s.v_axis;
    if(rapid) {
        if(v <= 0.0) v = v_command;
    } else {
        v = v > 0.0 ? std::min(v, v_command) : v_command;
    }
    if(limits_.traj_vmax > 0.0 && s.cartesian)
        v = std::min(v, limits_.traj_vmax);
    double a = s.a_axis;
    if(limits_.traj_amax > 0.0 && s.cartesian && a > 0.0)
        a = std::min(a, limits_.traj_amax);
    if(arc_radius > 0.0 && a > 0.0)
        v = std::min(v, sqrt(BLEND_ACC_RATIO_NORMAL * a * arc_radius));
    if(!(v > 0.0)) {
        raise(ZeroFeed);
        return;
    }
    push(Pending{line, s.length, s.dir, v, a, v, v, rapid});
}

void TimeEstimator::push(Pending p) {
    if(!queue_.empty()) {
        // Collinear passes at full speed, a right angle stops: one constant,
        // standing in for what G64 blending would actually do.
        Pending &prev = queue_.back();
        double dot = 0.0;
        for(int i = 0; i < P9_COUNT; i++) dot += prev.dir[i] * p.dir[i];
        double vj = std::min(prev.v_peak, p.v_peak) * std::max(0.0, dot);
        if(prev.v_end > vj) prev.v_end = vj;
        if(p.v_start > vj) p.v_start = vj;
        propagate_back(queue_.size() - 1);
    }
    queue_.push_back(p);
    while(queue_.size() > TIME_LOOKAHEAD) finalize_front();
}

// A tightened v_end may be unreachable from the move's own start, which
// tightens the move before it. Walks back only as far as it keeps changing.
void TimeEstimator::propagate_back(size_t i) {
    for(;;) {
        Pending &q = queue_[i];
        double reach = q.a > 0.0
                ? sqrt(q.v_end * q.v_end + 2.0 * q.a * q.length)
                : q.v_peak;
        if(q.v_start <= reach) return;
        q.v_start = reach;
        if(i == 0) return;
        Pending &prev = queue_[i - 1];
        if(prev.v_end <= q.v_start) return;
        prev.v_end = q.v_start;
        i--;
    }
}

void TimeEstimator::finalize_front() {
    Pending p = queue_.front();
    queue_.pop_front();

    double L = p.length, a = p.a, vp = p.v_peak;
    double vs = std::min(std::min(p.v_start, vp), carry_v_);
    double ve = std::min(p.v_end, vp);
    double t;
    if(a > 0.0) {
        double reach = sqrt(vs * vs + 2.0 * a * L);
        if(ve > reach) ve = reach;
        double vc = sqrt((2.0 * a * L + vs * vs + ve * ve) * 0.5);
        vc = std::min(vc, vp);
        vc = std::max(vc, std::max(vs, ve));
        double ramp = (2.0 * vc * vc - vs * vs - ve * ve) / (2.0 * a);
        double cruise = std::max(0.0, L - ramp);
        t = (vc - vs) / a + (vc - ve) / a + cruise / vc;
    } else {
        // No acceleration limit: straight to the cruise speed.
        t = L / vp;
        ve = vp;
    }
    carry_v_ = ve;
    total_ += t;
    (p.rapid ? rapid_ : feed_) += t;
    table_.note(p.line, total_);
}

void TimeEstimator::flush() {
    while(!queue_.empty()) finalize_front();
}

// ---------------------------------------------------------------------------
// What the renderer feeds in
// ---------------------------------------------------------------------------

void TimeEstimator::traverse(int line, const Point9 &from, const Point9 &to,
                             const FeedState &st) {
    Shape s = shape(from, to);
    // The fallback for a move none of whose axes state a limit: emccanon uses
    // the feed rate there, angular for a rotary-only move. The plain
    // units/minute reading of it, since a per-revolution rate is not a
    // traverse rate. traj_vmax still caps a cartesian move below.
    double rate = st.rate;
    if(!s.cartesian && s.angular && st.metric) rate *= 25.4;
    emit(line, s, rate / 60.0, 0.0, true);
}

void TimeEstimator::feed(int line, const Point9 &from, const Point9 &to,
                         const FeedState &st, double prog_x_from,
                         double prog_x_to, double arc_radius) {
    Shape s = shape(from, to);
    if(s.length <= 0.0) return;

    bool rpm_fed = st.sync_pitch > 0.0 || st.per_rev;
    bool css = st.css_max != 0.0 && st.css_factor > 0.0;
    if(css && st.css_max >= CSS_NO_LIMIT) raise(CssUnbounded);
    if(!rpm_fed || !css) {
        emit(line, s, commanded(st, s, st.rpm), arc_radius, false);
        return;
    }

    // Constant surface speed: the rpm follows the program radius, so the move
    // is cut into pieces no wider than CSS_SPLIT_RATIO and each is timed at
    // its midpoint's rpm. Below css_factor/css_max the rpm is flat, so that
    // radius is the floor the ratio is measured from. The pieces are
    // collinear, so the junctions between them cost nothing.
    double floor_r = st.css_max > 0.0 ? st.css_factor / st.css_max : 0.0;
    double ra = std::max(fabs(prog_x_from), floor_r);
    double rb = std::max(fabs(prog_x_to), floor_r);
    int n = 1;
    if(ra > 0.0 && rb > 0.0) {
        double ratio = ra > rb ? ra / rb : rb / ra;
        if(ratio > CSS_SPLIT_RATIO)
            n = (int)ceil(log(ratio) / log(CSS_SPLIT_RATIO));
    }
    if(prog_x_from * prog_x_to < 0.0 && n < 2) n = 2;   // crosses the centre
    n = std::min(std::max(n, 1), CSS_MAX_PIECES);

    Shape piece = s;
    piece.length = s.length / n;
    for(int i = 0; i < n; i++) {
        double r = fabs(prog_x_from
                        + (prog_x_to - prog_x_from) * (i + 0.5) / n);
        double rpm = r > 0.0 ? std::min(st.css_max, st.css_factor / r)
                             : st.css_max;
        emit(line, piece, commanded(st, piece, rpm), arc_radius, false);
    }
}

void TimeEstimator::probe(int line, const Point9 &from, const Point9 &to,
                          const FeedState &st, double prog_x_from,
                          double prog_x_to) {
    // Timed at the full commanded length: where it trips is not knowable here.
    raise(ProbeFullLength);
    stop();
    feed(line, from, to, st, prog_x_from, prog_x_to, 0.0);
    stop();
}

void TimeEstimator::rigid_tap(int line, const Point9 &from, const Point9 &to,
                              const FeedState &st, double retract_scale) {
    stop();
    double pitch = st.sync_pitch > 0.0 ? st.sync_pitch : st.rate;
    if(st.rpm <= 0.0 || pitch <= 0.0) {
        raise(SyncNoSpindle);
        return;
    }
    double scale = std::max(1.0, retract_scale);
    emit(line, shape(from, to), pitch * st.rpm / 60.0, 0.0, false);
    stop();
    emit(line, shape(to, from), pitch * st.rpm * scale / 60.0, 0.0, false);
    stop();
}

void TimeEstimator::stop() {
    if(!queue_.empty()) {
        queue_.back().v_end = 0.0;
        propagate_back(queue_.size() - 1);
        flush();
    }
    carry_v_ = 0.0;
}

void TimeEstimator::fixed(int line, double seconds) {
    stop();
    if(seconds <= 0.0) return;
    total_ += seconds;
    stop_ += seconds;
    table_.note(line, total_);
}

void TimeEstimator::finish() {
    stop();
    table_.finish(total_);
}

std::vector<std::string> TimeEstimator::flags() const {
    static const char *names[FlagCount] = {
        "css-unbounded", "probe-full-length", "zero-feed", "sync-no-spindle",
    };
    std::vector<std::string> out;
    for(int i = 0; i < FlagCount; i++)
        if(flags_[i]) out.push_back(names[i]);
    return out;
}
