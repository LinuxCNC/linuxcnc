#ifndef CAVC_VECTOR2_HPP
#define CAVC_VECTOR2_HPP
#include "mathutils.hpp"
#include "vector.hpp"
#include <cmath>
namespace cavc {
template <typename Real> using Vector2 = Vector<Real, 2>;

/// Perpendicular vector to v (rotating counter clockwise).
template <typename Real> Vector2<Real> perp(Vector2<Real> const &v) {
  return Vector2<Real>{-v.y(), v.x()};
}

/// Normalized perpendicular vector to v (rotating counter clockwise).
template <typename Real> Vector2<Real> unitPerp(Vector2<Real> const &v) {
  Vector2<Real> result{-v.y(), v.x()};
  normalize(result);
  return result;
}

/// Perpendicular dot product. Equivalent to dot(v0, perp(v1)).
template <typename Real> Real perpDot(Vector2<Real> const &v0, Vector2<Real> const &v1) {
  return v0.x() * v1.y() - v0.y() * v1.x();
}

/// Returns the distance squared between p0 and p1. Equivalent to dot(p1 - p0, p1 - p0).
template <typename Real> Real distSquared(Vector2<Real> const &p0, Vector2<Real> const &p1) {
  Vector2<Real> d = p1 - p0;
  return dot(d, d);
}

/// Counter clockwise angle of the vector going from p0 to p1.
template <typename Real> Real angle(Vector2<Real> const &p0, Vector2<Real> const &p1) {
  return std::atan2(p1.y() - p0.y(), p1.x() - p0.x());
}

/// Returns the midpoint between p0 and p1.
template <typename Real> Vector2<Real> midpoint(Vector2<Real> const &p0, Vector2<Real> const &p1) {
  return Vector2<Real>{(p0.x() + p1.x()) / Real(2), (p0.y() + p1.y()) / Real(2)};
}

/// Computes the point on the circle with radius, center, and polar angle given.
template <typename Real>
Vector2<Real> pointOnCircle(Real radius, Vector2<Real> const &center, Real angle) {
  return Vector2<Real>{center.x() + radius * std::cos(angle),
                       center.y() + radius * std::sin(angle)};
}

/// Return the point on the segment going from p0 to p1 at parametric value t.
template <typename Real>
Vector2<Real> pointFromParametric(Vector2<Real> const &p0, Vector2<Real> const &p1, Real t) {
  return p0 + t * (p1 - p0);
}

/// Returns the closest point that lies on the line segment from p0 to p1 to the point given.
template <typename Real>
Vector2<Real> closestPointOnLineSeg(Vector2<Real> const &p0, Vector2<Real> const &p1,
                                    Vector2<Real> const &point) {
  // Dot product used to find angles
  // See: http://geomalgorithms.com/a02-_lines.html
  Vector2<Real> v = p1 - p0;
  Vector2<Real> w = point - p0;
  Real c1 = dot(w, v);
  if (c1 < utils::realThreshold<Real>()) {
    return p0;
  }

  Real c2 = dot(v, v);
  if (c2 < c1 + utils::realThreshold<Real>()) {
    return p1;
  }

  Real b = c1 / c2;
  return p0 + b * v;
}

/// Returns true if point is left of the line pointing in the direction of the vector (p1 - p0).
template <typename Real>
bool isLeft(Vector2<Real> const &p0, Vector2<Real> const &p1, Vector2<Real> const &point) {
  return (p1.x() - p0.x()) * (point.y() - p0.y()) - (p1.y() - p0.y()) * (point.x() - p0.x()) >
         Real(0);
}

/// Same as isLeft but uses <= operator rather than < for boundary inclusion.
template <typename Real>
bool isLeftOrEqual(Vector2<Real> const &p0, Vector2<Real> const &p1, Vector2<Real> const &point) {
  return (p1.x() - p0.x()) * (point.y() - p0.y()) - (p1.y() - p0.y()) * (point.x() - p0.x()) >=
         Real(0);
}

/// Returns true if point is left or fuzzy coincident with the line pointing in the direction of the
/// vector (p1 - p0).
template <typename Real>
bool isLeftOrCoincident(Vector2<Real> const &p0, Vector2<Real> const &p1,
                        Vector2<Real> const &point, Real epsilon = utils::realThreshold<Real>()) {
  return (p1.x() - p0.x()) * (point.y() - p0.y()) - (p1.y() - p0.y()) * (point.x() - p0.x()) >
         -epsilon;
}

/// Returns true if point is right or fuzzy coincident with the line pointing in the direction of
/// the vector (p1 - p0).
template <typename Real>
bool isRightOrCoincident(Vector2<Real> const &p0, Vector2<Real> const &p1,
                         Vector2<Real> const &point, Real epsilon = utils::realThreshold<Real>()) {
  return (p1.x() - p0.x()) * (point.y() - p0.y()) - (p1.y() - p0.y()) * (point.x() - p0.x()) <
         epsilon;
}

/// Test if a point is within a arc sweep angle region defined by center, start, end, and bulge.
template <typename Real>
bool pointWithinArcSweepAngle(Vector2<Real> const &center, Vector2<Real> const &arcStart,
                              Vector2<Real> const &arcEnd, Real bulge, Vector2<Real> const &point) {
  CAVC_ASSERT(std::abs(bulge) > utils::realThreshold<Real>(), "expected arc");
  CAVC_ASSERT(std::abs(bulge) <= Real(1), "bulge should always be between -1 and 1");

  if (bulge > Real(0)) {
    return isLeftOrCoincident(center, arcStart, point) &&
           isRightOrCoincident(center, arcEnd, point);
  }

  return isRightOrCoincident(center, arcStart, point) && isLeftOrCoincident(center, arcEnd, point);
}
} // namespace cavc

#endif // CAVC_VECTOR2_HPP
