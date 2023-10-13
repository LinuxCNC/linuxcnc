#ifndef CAVC_TESTHELPERS_HPP
#define CAVC_TESTHELPERS_HPP
#include "cavaliercontours.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cmath>
#include <iostream>
constexpr inline cavc_real PI() { return 3.14159265358979323846264338327950288; }
constexpr inline cavc_real TEST_EPSILON() { return 1e-5; }
template <typename Real> inline bool fuzzyEqual(Real const &left, cavc_real const &right) {
  return std::abs(left - right) < TEST_EPSILON();
}

// type to hold summary properties of a polyline for test comparing, acts as a sort of geometric
// hash of the polyline, it is very unlikely that two polylines have the same PolylineProperties
// without being the same polyline, especially accidentally via generation in an algorithm
struct PolylineProperties {
  std::size_t vertexCount;
  cavc_real area;
  cavc_real pathLength;
  cavc_real minX;
  cavc_real minY;
  cavc_real maxX;
  cavc_real maxY;

  PolylineProperties(std::size_t vertexCount, cavc_real area, cavc_real pathLength, cavc_real minX,
                     cavc_real minY, cavc_real maxX, cavc_real maxY)
      : vertexCount(vertexCount), area(area), pathLength(pathLength), minX(minX), minY(minY),
        maxX(maxX), maxY(maxY) {}

  PolylineProperties(cavc_pline *pline) {
    vertexCount = cavc_pline_vertex_count(pline);
    area = cavc_get_area(pline);
    pathLength = cavc_get_path_length(pline);
    cavc_get_extents(pline, &minX, &minY, &maxX, &maxY);
  }
};

MATCHER(EqIgnoreSignOfArea, "") {
  auto const &left = std::get<0>(arg);
  auto const &right = std::get<1>(arg);
  return left.vertexCount == right.vertexCount &&
         fuzzyEqual(std::abs(left.area), std::abs(right.area)) &&
         fuzzyEqual(left.pathLength, right.pathLength) && fuzzyEqual(left.minX, right.minX) &&
         fuzzyEqual(left.minY, right.minY) && fuzzyEqual(left.maxX, right.maxX) &&
         fuzzyEqual(left.maxY, right.maxY);
}

// fuzzy equality operator== for testing
inline bool operator==(PolylineProperties const &left, PolylineProperties const &right) {
  return left.vertexCount == right.vertexCount && fuzzyEqual(left.area, right.area) &&
         fuzzyEqual(left.pathLength, right.pathLength) && fuzzyEqual(left.minX, right.minX) &&
         fuzzyEqual(left.minY, right.minY) && fuzzyEqual(left.maxX, right.maxX) &&
         fuzzyEqual(left.maxY, right.maxY);
}

inline std::ostream &operator<<(std::ostream &os, PolylineProperties const &p) {
  os << "{ vertexCount: " << p.vertexCount << ", area: " << p.area
     << ", pathLength: " << p.pathLength << ", minX: " << p.minX << ", minY: " << p.minY
     << ", maxX: " << p.maxX << ", maxY: " << p.maxY << " }";
  return os;
}
#endif // CAVC_TESTHELPERS_HPP
