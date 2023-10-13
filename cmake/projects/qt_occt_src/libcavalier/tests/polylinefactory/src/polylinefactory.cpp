#include "polylinefactory.hpp"

static cavc_point pointOnCircle(cavc_real radius, cavc_point center, cavc_real angle) {
  return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}

inline static cavc_real PI() { return 3.14159265358979323846264338327950288; }

std::vector<cavc_vertex> PolylineFactory::createCircle(cavc_real radius, cavc_point center,
                                                       cavc_real vertexRotAngle, bool isCW) {
  std::vector<cavc_vertex> result;
  result.reserve(2);

  cavc_point point1 = pointOnCircle(radius, center, vertexRotAngle);
  cavc_point point2 = pointOnCircle(radius, center, vertexRotAngle + PI());
  cavc_real bulge = isCW ? -1.0 : 1.0;
  result.push_back({point1.x, point1.y, bulge});
  result.push_back({point2.x, point2.y, bulge});

  return result;
}

cavc_pline_ptr PolylineFactory::vertexesToPline(std::vector<cavc_vertex> const &vertexes,
                                                bool isClosed) {
  return cavc_pline_ptr(
      cavc_pline_new(&vertexes[0], static_cast<uint32_t>(vertexes.size()), isClosed));
}
