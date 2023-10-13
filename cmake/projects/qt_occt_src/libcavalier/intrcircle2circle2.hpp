#ifndef CAVC_INTRCIRCLE2CIRCLE2_HPP
#define CAVC_INTRCIRCLE2CIRCLE2_HPP
#include "vector2.hpp"
namespace cavc {
enum class Circle2Circle2IntrType {
  // no intersect between circles
  NoIntersect,
  // one intersect between circles (tangent)
  OneIntersect,
  // two intersects between circles
  TwoIntersects,
  // circles are coincident
  Coincident
};

template <typename Real> struct IntrCircle2Circle2Result {
  // type of intersect
  Circle2Circle2IntrType intrType;
  // first intersect point if intrType is OneIntersect or TwoIntersects, undefined otherwise
  Vector2<Real> point1;
  // second intersect point if intrType is TwoIntersects, undefined otherwise
  Vector2<Real> point2;
};

// Find intersect between two circles in 2D.
template <typename Real>
IntrCircle2Circle2Result<Real> intrCircle2Circle2(Real radius1, Vector2<Real> const &center1,
                                                  Real radius2, Vector2<Real> const &center2) {
  // Reference algorithm: http://paulbourke.net/geometry/circlesphere/

  IntrCircle2Circle2Result<Real> result;
  Vector2<Real> cv = center2 - center1;
  Real d2 = dot(cv, cv);
  Real d = std::sqrt(d2);
  if (d < utils::realThreshold<Real>()) {
    // same center position
    if (utils::fuzzyEqual(radius1, radius2)) {
      result.intrType = Circle2Circle2IntrType::Coincident;
    } else {
      result.intrType = Circle2Circle2IntrType::NoIntersect;
    }
  } else {
    // different center position
    if (d > radius1 + radius2 + utils::realThreshold<Real>() ||
        d + utils::realThreshold<Real>() < std::abs(radius1 - radius2)) {
      result.intrType = Circle2Circle2IntrType::NoIntersect;
    } else {
      Real rad1Sq = radius1 * radius1;
      Real a = (rad1Sq - radius2 * radius2 + d2) / (Real(2) * d);
      Vector2<Real> midPoint = center1 + a * cv / d;
      Real diff = rad1Sq - a * a;
      if (diff < Real(0)) {
        result.intrType = Circle2Circle2IntrType::OneIntersect;
        result.point1 = midPoint;
      } else {
        Real h = std::sqrt(diff);
        Real hOverD = h / d;
        Real xTerm = hOverD * cv.y();
        Real yTerm = hOverD * cv.x();
        Real x1 = midPoint.x() + xTerm;
        Real y1 = midPoint.y() - yTerm;
        Real x2 = midPoint.x() - xTerm;
        Real y2 = midPoint.y() + yTerm;
        result.point1 = Vector2<Real>(x1, y1);
        result.point2 = Vector2<Real>(x2, y2);
        if (fuzzyEqual(result.point1, result.point2)) {
          result.intrType = Circle2Circle2IntrType::OneIntersect;
        } else {
          result.intrType = Circle2Circle2IntrType::TwoIntersects;
        }
      }
    }
  }

  return result;
}
} // namespace cavc

#endif // CAVC_INTRCIRCLE2CIRCLE2_HPP
