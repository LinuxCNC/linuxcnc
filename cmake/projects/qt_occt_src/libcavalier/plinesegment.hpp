#ifndef CAVC_PLINESEGMENT_HPP
#define CAVC_PLINESEGMENT_HPP
#include "intrcircle2circle2.hpp"
#include "intrlineseg2circle2.hpp"
#include "intrlineseg2lineseg2.hpp"
#include "mathutils.hpp"
#include "vector2.hpp"

// This header has the polyline vertex definition and functions that work with polyline segments
// defined by two polyline vertexes (e.g. intersects, arc information, splitting, etc.)

namespace cavc {
template <typename Real> class PlineVertex {
public:
  PlineVertex() = default;
  PlineVertex(Real x, Real y, Real bulge) : m_position(x, y), m_bulge(bulge) {}
  PlineVertex(Vector2<Real> position, Real bulge)
      : PlineVertex(position.x(), position.y(), bulge) {}

  Real x() const { return m_position.x(); }
  Real &x() { return m_position.x(); }

  Real y() const { return m_position.y(); }
  Real &y() { return m_position.y(); }

  Real bulge() const { return m_bulge; }
  Real &bulge() { return m_bulge; }

  bool bulgeIsZero(Real epsilon = utils::realPrecision<Real>()) const {
    return std::abs(m_bulge) < epsilon;
  }

  bool bulgeIsNeg() const { return m_bulge < Real(0); }
  bool bulgeIsPos() const { return m_bulge > Real(0); }

  Vector2<Real> const &pos() const { return m_position; }
  Vector2<Real> &pos() { return m_position; }

private:
  Vector2<Real> m_position;
  Real m_bulge;
};

/// Axis aligned bounding box (AABB).
template <typename Real> struct AABB {
  Real xMin;
  Real yMin;
  Real xMax;
  Real yMax;

  void expand(Real val) {
    xMin -= val;
    yMin -= val;
    xMax += val;
    yMax += val;
  }
};

/// Result from computing the arc radius and arc center of a segment.
template <typename Real> struct ArcRadiusAndCenter {
  Real radius;
  Vector2<Real> center;
};

/// Compute the arc radius and arc center of a arc segment defined by v1 to v2.
template <typename Real>
ArcRadiusAndCenter<Real> arcRadiusAndCenter(PlineVertex<Real> const &v1,
                                            PlineVertex<Real> const &v2) {
  CAVC_ASSERT(!v1.bulgeIsZero(), "v1 to v2 must be an arc");
  CAVC_ASSERT(!fuzzyEqual(v1.pos(), v2.pos()), "v1 must not be ontop of v2");

  // compute radius
  Real b = std::abs(v1.bulge());
  Vector2<Real> v = v2.pos() - v1.pos();
  Real d = length(v);
  Real r = d * (b * b + Real(1)) / (Real(4) * b);

  // compute center
  Real s = b * d / Real(2);
  Real m = r - s;
  Real offsX = -m * v.y() / d;
  Real offsY = m * v.x() / d;
  if (v1.bulgeIsNeg()) {
    offsX = -offsX;
    offsY = -offsY;
  }

  Vector2<Real> c(v1.x() + v.x() / Real(2) + offsX, v1.y() + v.y() / Real(2) + offsY);
  return ArcRadiusAndCenter<Real>{r, c};
}

/// Result of splitting a segment v1 to v2.
template <typename Real> struct SplitResult {
  /// Updated starting vertex.
  PlineVertex<Real> updatedStart;
  /// Vertex at the split point.
  PlineVertex<Real> splitVertex;
};

/// Split the segment defined by v1 to v2 at some point defined along it.
template <typename Real>
SplitResult<Real> splitAtPoint(PlineVertex<Real> const &v1, PlineVertex<Real> const &v2,
                               Vector2<Real> const &point) {
  SplitResult<Real> result;
  if (v1.bulgeIsZero()) {
    result.updatedStart = v1;
    result.splitVertex = PlineVertex<Real>(point, Real(0));
  } else if (fuzzyEqual(v1.pos(), v2.pos(), utils::realPrecision<Real>()) ||
             fuzzyEqual(v1.pos(), point, utils::realPrecision<Real>())) {
    result.updatedStart = PlineVertex<Real>(point, Real(0));
    result.splitVertex = PlineVertex<Real>(point, v1.bulge());
  } else if (fuzzyEqual(v2.pos(), point, utils::realPrecision<Real>())) {
    result.updatedStart = v1;
    result.splitVertex = PlineVertex<Real>(v2.pos(), Real(0));
  } else {
    auto radiusAndCenter = arcRadiusAndCenter(v1, v2);
    Vector2<Real> arcCenter = radiusAndCenter.center;
    Real a = angle(arcCenter, point);
    Real arcStartAngle = angle(arcCenter, v1.pos());
    Real theta1 = utils::deltaAngle(arcStartAngle, a);
    Real bulge1 = std::tan(theta1 / Real(4));
    Real arcEndAngle = angle(arcCenter, v2.pos());
    Real theta2 = utils::deltaAngle(a, arcEndAngle);
    Real bulge2 = std::tan(theta2 / Real(4));

    result.updatedStart = PlineVertex<Real>(v1.pos(), bulge1);
    result.splitVertex = PlineVertex<Real>(point, bulge2);
  }

  return result;
}

template <typename Real>
Vector2<Real> segTangentVector(PlineVertex<Real> const &v1, PlineVertex<Real> const &v2,
                               Vector2<Real> const &pointOnSeg) {
  if (v1.bulgeIsZero()) {
    return v2.pos() - v1.pos();
  }

  auto arc = arcRadiusAndCenter(v1, v2);
  if (v1.bulgeIsPos()) {
    // ccw, rotate vector from center to pointOnCurve 90 degrees
    return Vector2<Real>(-(pointOnSeg.y() - arc.center.y()), pointOnSeg.x() - arc.center.x());
  }

  // cw, rotate vector from center to pointOnCurve -90 degrees
  return Vector2<Real>(pointOnSeg.y() - arc.center.y(), -(pointOnSeg.x() - arc.center.x()));
}

/// Compute the closest point on a segment defined by v1 to v2 to the point given.
template <typename Real>
Vector2<Real> closestPointOnSeg(PlineVertex<Real> const &v1, PlineVertex<Real> const &v2,
                                Vector2<Real> const &point) {
  if (v1.bulgeIsZero()) {
    return closestPointOnLineSeg(v1.pos(), v2.pos(), point);
  }

  auto arc = arcRadiusAndCenter(v1, v2);

  if (fuzzyEqual(point, arc.center)) {
    // avoid normalizing zero length vector (point is at center, just return start point)
    return v1.pos();
  }

  if (pointWithinArcSweepAngle(arc.center, v1.pos(), v2.pos(), v1.bulge(), point)) {
    // closest point is on the arc
    Vector2<Real> vToPoint = point - arc.center;
    normalize(vToPoint);
    return arc.radius * vToPoint + arc.center;
  }

  // else closest point is one of the ends
  Real dist1 = distSquared(v1.pos(), point);
  Real dist2 = distSquared(v2.pos(), point);
  if (dist1 < dist2) {
    return v1.pos();
  }

  return v2.pos();
}

/// Computes a fast approximate AABB of a segment described by v1 to v2, bounding box may be larger
/// than the true bounding box for the segment
template <typename Real>
AABB<Real> createFastApproxBoundingBox(PlineVertex<Real> const &v1, PlineVertex<Real> const &v2) {
  AABB<Real> result;
  if (v1.bulgeIsZero()) {
    if (v1.x() < v2.x()) {
      result.xMin = v1.x();
      result.xMax = v2.x();
    } else {
      result.xMin = v2.x();
      result.xMax = v1.x();
    }

    if (v1.y() < v2.y()) {
      result.yMin = v1.y();
      result.yMax = v2.y();
    } else {
      result.yMin = v2.y();
      result.yMax = v1.y();
    }

    return result;
  }

  // For arcs we don't compute the actual extents which is slower, instead we create an approximate
  // bounding box from the rectangle formed by extending the chord by the sagitta, NOTE: this
  // approximate bounding box is always equal to or bigger than the true bounding box
  Real b = v1.bulge();
  Real offsX = b * (v2.y() - v1.y()) / Real(2);
  Real offsY = -b * (v2.x() - v1.x()) / Real(2);

  Real pt1X = v1.x() + offsX;
  Real pt2X = v2.x() + offsX;
  Real pt1Y = v1.y() + offsY;
  Real pt2Y = v2.y() + offsY;

  Real endPointXMin, endPointXMax;
  if (v1.x() < v2.x()) {
    endPointXMin = v1.x();
    endPointXMax = v2.x();
  } else {
    endPointXMin = v2.x();
    endPointXMax = v1.x();
  }

  Real ptXMin, ptXMax;
  if (pt1X < pt2X) {
    ptXMin = pt1X;
    ptXMax = pt2X;
  } else {
    ptXMin = pt2X;
    ptXMax = pt1X;
  }

  Real endPointYMin, endPointYMax;
  if (v1.y() < v2.y()) {
    endPointYMin = v1.y();
    endPointYMax = v2.y();
  } else {
    endPointYMin = v2.y();
    endPointYMax = v1.y();
  }

  Real ptYMin, ptYMax;
  if (pt1Y < pt2Y) {
    ptYMin = pt1Y;
    ptYMax = pt2Y;
  } else {
    ptYMin = pt2Y;
    ptYMax = pt1Y;
  }

  result.xMin = std::min(endPointXMin, ptXMin);
  result.yMin = std::min(endPointYMin, ptYMin);
  result.xMax = std::max(endPointXMax, ptXMax);
  result.yMax = std::max(endPointYMax, ptYMax);
  return result;
}

/// Calculate the path length for the segment defined from v1 to v2.
template <typename Real> Real segLength(PlineVertex<Real> const &v1, PlineVertex<Real> const &v2) {
  if (fuzzyEqual(v1.pos(), v2.pos())) {
    return Real(0);
  }

  if (v1.bulgeIsZero()) {
    return std::sqrt(distSquared(v1.pos(), v2.pos()));
  }

  auto arc = arcRadiusAndCenter(v1, v2);
  Real startAngle = angle(arc.center, v1.pos());
  Real endAngle = angle(arc.center, v2.pos());
  return std::abs(arc.radius * utils::deltaAngle(startAngle, endAngle));
}

/// Return the mid point along a segment path.
template <typename Real>
Vector2<Real> segMidpoint(PlineVertex<Real> const &v1, PlineVertex<Real> const &v2) {
  if (v1.bulgeIsZero()) {
    return midpoint(v1.pos(), v2.pos());
  }

  auto arc = arcRadiusAndCenter(v1, v2);
  Real a1 = angle(arc.center, v1.pos());
  Real a2 = angle(arc.center, v2.pos());
  Real angleOffset = std::abs(utils::deltaAngle(a1, a2) / Real(2));
  // use arc direction to determine offset sign to robustly handle half circles
  Real midAngle = v1.bulgeIsPos() ? a1 + angleOffset : a1 - angleOffset;
  return pointOnCircle(arc.radius, arc.center, midAngle);
}

enum class PlineSegIntrType {
  NoIntersect,
  TangentIntersect,
  OneIntersect,
  TwoIntersects,
  SegmentOverlap,
  ArcOverlap
};

template <typename Real> struct IntrPlineSegsResult {
  PlineSegIntrType intrType;
  Vector2<Real> point1;
  Vector2<Real> point2;
};

template <typename Real>
IntrPlineSegsResult<Real> intrPlineSegs(PlineVertex<Real> const &v1, PlineVertex<Real> const &v2,
                                        PlineVertex<Real> const &u1, PlineVertex<Real> const &u2) {
  IntrPlineSegsResult<Real> result;
  const bool vIsLine = v1.bulgeIsZero();
  const bool uIsLine = u1.bulgeIsZero();

  // helper function to process line arc intersect
  auto processLineArcIntr = [&result](Vector2<Real> const &p0, Vector2<Real> const &p1,
                                      PlineVertex<Real> const &a1, PlineVertex<Real> const &a2) {
    auto arc = arcRadiusAndCenter(a1, a2);
    auto intrResult = intrLineSeg2Circle2(p0, p1, arc.radius, arc.center);

    // helper function to test and get point within arc sweep
    auto pointInSweep = [&](Real t) {
      if (t + utils::realThreshold<Real>() < Real(0) ||
          t > Real(1) + utils::realThreshold<Real>()) {
        return std::make_pair(false, Vector2<Real>());
      }

      Vector2<Real> p = pointFromParametric(p0, p1, t);
      bool withinSweep = pointWithinArcSweepAngle(arc.center, a1.pos(), a2.pos(), a1.bulge(), p);
      return std::make_pair(withinSweep, p);
    };

    if (intrResult.numIntersects == 0) {
      result.intrType = PlineSegIntrType::NoIntersect;
    } else if (intrResult.numIntersects == 1) {
      auto p = pointInSweep(intrResult.t0);
      if (p.first) {
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = p.second;
      } else {
        result.intrType = PlineSegIntrType::NoIntersect;
      }
    } else {
      CAVC_ASSERT(intrResult.numIntersects == 2, "shouldn't get here without 2 intersects");
      auto p1 = pointInSweep(intrResult.t0);
      auto p2 = pointInSweep(intrResult.t1);

      if (p1.first && p2.first) {
        result.intrType = PlineSegIntrType::TwoIntersects;
        result.point1 = p1.second;
        result.point2 = p2.second;
      } else if (p1.first) {
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = p1.second;
      } else if (p2.first) {
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = p2.second;
      } else {
        result.intrType = PlineSegIntrType::NoIntersect;
      }
    }
  };

  if (vIsLine && uIsLine) {
    auto intrResult = intrLineSeg2LineSeg2(v1.pos(), v2.pos(), u1.pos(), u2.pos());
    switch (intrResult.intrType) {
    case LineSeg2LineSeg2IntrType::None:
      result.intrType = PlineSegIntrType::NoIntersect;
      break;
    case LineSeg2LineSeg2IntrType::True:
      result.intrType = PlineSegIntrType::OneIntersect;
      result.point1 = intrResult.point;
      break;
    case LineSeg2LineSeg2IntrType::Coincident:
      result.intrType = PlineSegIntrType::SegmentOverlap;
      // build points from parametric parameters (using second segment as defined by the function)
      result.point1 = pointFromParametric(u1.pos(), u2.pos(), intrResult.t0);
      result.point2 = pointFromParametric(u1.pos(), u2.pos(), intrResult.t1);
      break;
    case LineSeg2LineSeg2IntrType::False:
      result.intrType = PlineSegIntrType::NoIntersect;
      break;
    }

  } else if (vIsLine) {
    processLineArcIntr(v1.pos(), v2.pos(), u1, u2);
  } else if (uIsLine) {
    processLineArcIntr(u1.pos(), u2.pos(), v1, v2);
  } else {
    auto arc1 = arcRadiusAndCenter(v1, v2);
    auto arc2 = arcRadiusAndCenter(u1, u2);

    auto startAndSweepAngle = [](Vector2<Real> const &sp, Vector2<Real> const &center, Real bulge) {
      Real startAngle = utils::normalizeRadians(angle(center, sp));
      Real sweepAngle = Real(4) * std::atan(bulge);
      return std::make_pair(startAngle, sweepAngle);
    };

    auto bothArcsSweepPoint = [&](Vector2<Real> const &pt) {
      return pointWithinArcSweepAngle(arc1.center, v1.pos(), v2.pos(), v1.bulge(), pt) &&
             pointWithinArcSweepAngle(arc2.center, u1.pos(), u2.pos(), u1.bulge(), pt);
    };

    auto intrResult = intrCircle2Circle2(arc1.radius, arc1.center, arc2.radius, arc2.center);

    switch (intrResult.intrType) {
    case Circle2Circle2IntrType::NoIntersect:
      result.intrType = PlineSegIntrType::NoIntersect;
      break;
    case Circle2Circle2IntrType::OneIntersect:
      if (bothArcsSweepPoint(intrResult.point1)) {
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = intrResult.point1;
      } else {
        result.intrType = PlineSegIntrType::NoIntersect;
      }
      break;
    case Circle2Circle2IntrType::TwoIntersects: {
      const bool pt1InSweep = bothArcsSweepPoint(intrResult.point1);
      const bool pt2InSweep = bothArcsSweepPoint(intrResult.point2);
      if (pt1InSweep && pt2InSweep) {
        result.intrType = PlineSegIntrType::TwoIntersects;
        result.point1 = intrResult.point1;
        result.point2 = intrResult.point2;
      } else if (pt1InSweep) {
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = intrResult.point1;
      } else if (pt2InSweep) {
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = intrResult.point2;
      } else {
        result.intrType = PlineSegIntrType::NoIntersect;
      }
    } break;
    case Circle2Circle2IntrType::Coincident:
      // determine if arcs overlap along their sweep
      // start and sweep angles
      auto arc1StartAndSweep = startAndSweepAngle(v1.pos(), arc1.center, v1.bulge());
      // we have the arcs go the same direction to simplify checks
      auto arc2StartAndSweep = [&] {
        if (v1.bulgeIsNeg() == u1.bulgeIsNeg()) {
          return startAndSweepAngle(u1.pos(), arc2.center, u1.bulge());
        }

        return startAndSweepAngle(u2.pos(), arc2.center, -u1.bulge());
      }();
      // end angles (start + sweep)
      auto arc1End = arc1StartAndSweep.first + arc1StartAndSweep.second;
      auto arc2End = arc2StartAndSweep.first + arc2StartAndSweep.second;

      if (std::abs(utils::deltaAngle(arc1StartAndSweep.first, arc2End)) <
          utils::realThreshold<Real>()) {
        // only end points touch at start of arc1
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = v1.pos();
      } else if (std::abs(utils::deltaAngle(arc2StartAndSweep.first, arc1End)) <
                 utils::realThreshold<Real>()) {
        // only end points touch at start of arc2
        result.intrType = PlineSegIntrType::OneIntersect;
        result.point1 = u1.pos();
      } else {
        const bool arc2StartsInArc1Sweep = utils::angleIsWithinSweep(
            arc1StartAndSweep.first, arc1StartAndSweep.second, arc2StartAndSweep.first);
        const bool arc2EndsInArc1Sweep =
            utils::angleIsWithinSweep(arc1StartAndSweep.first, arc1StartAndSweep.second, arc2End);
        if (arc2StartsInArc1Sweep && arc2EndsInArc1Sweep) {
          // arc2 is fully overlapped by arc1
          result.intrType = PlineSegIntrType::ArcOverlap;
          result.point1 = u1.pos();
          result.point2 = u2.pos();
        } else if (arc2StartsInArc1Sweep) {
          // overlap from arc2 start to arc1 end
          result.intrType = PlineSegIntrType::ArcOverlap;
          result.point1 = u1.pos();
          result.point2 = v2.pos();
        } else if (arc2EndsInArc1Sweep) {
          // overlap from arc1 start to arc2 end
          result.intrType = PlineSegIntrType::ArcOverlap;
          result.point1 = v1.pos();
          result.point2 = u2.pos();
        } else {
          const bool arc1StartsInArc2Sweep = utils::angleIsWithinSweep(
              arc2StartAndSweep.first, arc2StartAndSweep.second, arc1StartAndSweep.first);
          if (arc1StartsInArc2Sweep) {
            result.intrType = PlineSegIntrType::ArcOverlap;
            result.point1 = v1.pos();
            result.point2 = v2.pos();
          } else {
            result.intrType = PlineSegIntrType::NoIntersect;
          }
        }
      }

      break;
    }
  }

  return result;
}

} // namespace cavc
#endif // CAVC_PLINESEGMENT_HPP
