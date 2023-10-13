#ifndef CAVC_INTRLINESEG2LINESEG2_HPP
#define CAVC_INTRLINESEG2LINESEG2_HPP
#include "vector2.hpp"
#include <algorithm>

namespace cavc {
enum class LineSeg2LineSeg2IntrType {
  // no intersect (segments are parallel and not collinear)
  None,
  // true intersect between line segments
  True,
  // segments overlap each other by some amount
  Coincident,
  // false intersect between line segments (one or both of the segments must be extended)
  False
};

template <typename Real> struct IntrLineSeg2LineSeg2Result {
  // holds the type of intersect, if True or False then point holds the point that they intersect,
  // if True then t0 and t1 are undefined, if False then t0 is the parametric value of the first
  // segment and t1 is the parametric value of the second segment, if Coincident then point is
  // undefined and t0 holds the parametric value start of coincidence and t1 holds the parametric
  // value of the end of the coincidence for the second segment's equation
  LineSeg2LineSeg2IntrType intrType;
  Real t0;
  Real t1;
  Vector2<Real> point;
};

template <typename Real>
IntrLineSeg2LineSeg2Result<Real>
intrLineSeg2LineSeg2(Vector2<Real> const &u1, Vector2<Real> const &u2, Vector2<Real> const &v1,
                     Vector2<Real> const &v2) {
  // This implementation works by processing the segments in parametric equation form and using
  // perpendicular products
  // see: http://geomalgorithms.com/a05-_intersect-1.html and
  // http://mathworld.wolfram.com/PerpDotProduct.html

  IntrLineSeg2LineSeg2Result<Real> result;
  Vector2<Real> u = u2 - u1;
  Vector2<Real> v = v2 - v1;
  Real d = perpDot(u, v);

  Vector2<Real> w = u1 - v1;

  // Test if point is inside a segment, NOTE: assumes points are aligned
  auto isInSegment = [](Vector2<Real> const &pt, Vector2<Real> const &segStart,
                        Vector2<Real> const &segEnd) {
    if (utils::fuzzyEqual(segStart.x(), segEnd.x())) {
      // vertical segment, test y coordinate
      auto minMax = std::minmax(segStart.y(), segEnd.y());
      return utils::fuzzyInRange(minMax.first, pt.y(), minMax.second);
    }

    // else just test x coordinate
    auto minMax = std::minmax(segStart.x(), segEnd.x());
    return utils::fuzzyInRange(minMax.first, pt.x(), minMax.second);
  };

  // threshold check here to avoid almost parallel lines resulting in very distant intersection
  if (std::abs(d) > utils::realThreshold<Real>()) {
    // segments not parallel or collinear
    result.t0 = perpDot(v, w) / d;
    result.t1 = perpDot(u, w) / d;
    result.point = v1 + result.t1 * v;
    if (result.t0 + utils::realThreshold<Real>() < Real(0) ||
        result.t0 > Real(1) + utils::realThreshold<Real>() ||
        result.t1 + utils::realThreshold<Real>() < Real(0) ||
        result.t1 > Real(1) + utils::realThreshold<Real>()) {
      result.intrType = LineSeg2LineSeg2IntrType::False;
    } else {
      result.intrType = LineSeg2LineSeg2IntrType::True;
    }
  } else {
    // segments are parallel or collinear
    Real a = perpDot(u, w);
    Real b = perpDot(v, w);
    // threshold check here, we consider almost parallel lines to be parallel
    if (std::abs(a) > utils::realThreshold<Real>() || std::abs(b) > utils::realThreshold<Real>()) {
      // parallel and not collinear so no intersect
      result.intrType = LineSeg2LineSeg2IntrType::None;
    } else {
      // either collinear or degenerate (segments are single points)
      bool uIsPoint = fuzzyEqual(u1, u2);
      bool vIsPoint = fuzzyEqual(v1, v2);
      if (uIsPoint && vIsPoint) {
        // both segments are just points
        if (fuzzyEqual(u1, v1)) {
          // same point
          result.point = u1;
          result.intrType = LineSeg2LineSeg2IntrType::True;
        } else {
          // distinct points
          result.intrType = LineSeg2LineSeg2IntrType::None;
        }

      } else if (uIsPoint) {
        if (isInSegment(u1, v1, v2)) {
          result.intrType = LineSeg2LineSeg2IntrType::True;
          result.point = u1;
        } else {
          result.intrType = LineSeg2LineSeg2IntrType::None;
        }

      } else if (vIsPoint) {
        if (isInSegment(v1, u1, u2)) {
          result.intrType = LineSeg2LineSeg2IntrType::True;
          result.point = v1;
        } else {
          result.intrType = LineSeg2LineSeg2IntrType::None;
        }
      } else {
        // neither segment is a point, check if they overlap
        Vector2<Real> w2 = u2 - v1;
        if (std::abs(v.x()) < utils::realThreshold<Real>()) {
          result.t0 = w.y() / v.y();
          result.t1 = w2.y() / v.y();
        } else {
          result.t0 = w.x() / v.x();
          result.t1 = w2.x() / v.x();
        }

        if (result.t0 > result.t1) {
          using std::swap;
          swap(result.t0, result.t1);
        }

        // using threshold check here to make intersect "sticky" to prefer considering it an
        // intersect
        if (result.t0 > Real(1) + utils::realThreshold<Real>() ||
            result.t1 + utils::realThreshold<Real>() < Real(0)) {
          // no overlap
          result.intrType = LineSeg2LineSeg2IntrType::None;
        } else {
          result.t0 = std::max(result.t0, Real(0));
          result.t1 = std::min(result.t1, Real(1));
          if (std::abs(result.t1 - result.t0) < utils::realThreshold<Real>()) {
            // intersect is a single point (segments line up end to end)
            result.intrType = LineSeg2LineSeg2IntrType::True;
            result.point = v1 + result.t0 * v;
          } else {
            result.intrType = LineSeg2LineSeg2IntrType::Coincident;
          }
        }
      }
    }
  }

  return result;
}
} // namespace cavc
#endif // CAVC_INTRLINESEG2LINESEG2_HPP
