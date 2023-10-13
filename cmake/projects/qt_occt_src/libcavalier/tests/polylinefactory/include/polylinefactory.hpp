#ifndef CAVC_POLYLINEFACTORY_HPP
#define CAVC_POLYLINEFACTORY_HPP
#include "cavaliercontours.h"
#include "cavc/polyline.hpp"

struct cavc_pline_deleter {
  void operator()(cavc_pline *pline) { cavc_pline_delete(pline); }
};

using cavc_pline_ptr = std::unique_ptr<cavc_pline, cavc_pline_deleter>;

class PolylineFactory {
public:
  // Create a circle with radius and center, vertexRotAngle rotates the vertexes about the circle
  // center, if isCW is true then circle goes clockwise else counter clockwise.
  static std::vector<cavc_vertex> createCircle(cavc_real radius, cavc_point center,
                                               cavc_real vertexRotAngle, bool isCW);
  static cavc_pline_ptr vertexesToPline(std::vector<cavc_vertex> const &vertexes, bool isClosed);
};

#endif // CAVC_POLYLINEPATHFACTORY_HPP
