#ifndef CAVC_INTERNAL_DIAGNOSTICS_HPP
#define CAVC_INTERNAL_DIAGNOSTICS_HPP
#include "../polyline.hpp"
#include <iomanip>
#include <sstream>
#include <string>
namespace cavc {
namespace internal {
template <typename Real>
std::string printVertexesToInitializerList(::cavc::Polyline<Real> const &pline) {
  std::stringstream ss;
  ss << std::setprecision(14) << "{ ";
  for (auto const &v : pline.vertexes()) {
    ss << "{ " << v.x() << ", " << v.y() << ", " << v.bulge() << " },\n";
  }
  std::string result = ss.str();
  result.pop_back();
  result.pop_back();
  result.push_back(' ');
  result.push_back('}');
  return result;
}

template <typename Real> std::string propertiesFromPolyline(::cavc::Polyline<Real> const &pline) {
  auto area = getArea(pline);
  auto pathLength = getPathLength(pline);
  auto extents = getExtents(pline);
  std::stringstream ss;
  ss << std::setprecision(14) << "(" << pline.size() << ", " << area << ", " << pathLength << ", "
     << extents.xMin << ", " << extents.yMin << ", " << extents.xMax << ", " << extents.yMax << ")";
  std::string result = ss.str();
  return result;
}
} // namespace internal
} // namespace cavc
#endif // CAVC_INTERNAL_DIAGNOSTICS_HPP
