#include "c_api_test_helpers.hpp"
#include "cavaliercontours.h"
#include <iostream>
#include <string>
#include <vector>

namespace t = testing;
struct cavc_plineFunctionsTestCase {

  cavc_plineFunctionsTestCase(std::string name, std::vector<cavc_vertex> vertexes, bool isClosed)
      : name(std::move(name)),
        pline(plineFromVertexes(vertexes, isClosed)),
        plineVertexes(std::move(vertexes)) {}

  // simple name for the test case
  std::string name;

  // polyline for the test case
  cavc_pline *pline = nullptr;
  // vertexes of pline for easy access
  std::vector<cavc_vertex> plineVertexes;

  // expected signed area
  cavc_real signedArea = std::numeric_limits<cavc_real>::quiet_NaN();
  bool skipAreaTest() const { return std::isnan(signedArea); }

  // expected path length
  cavc_real pathLength = std::numeric_limits<cavc_real>::quiet_NaN();
  bool skipPathLengthTest() const { return std::isnan(signedArea); }

  // expected extents
  cavc_real minX = std::numeric_limits<double>::quiet_NaN();
  cavc_real minY = std::numeric_limits<double>::quiet_NaN();
  cavc_real maxX = std::numeric_limits<double>::quiet_NaN();
  cavc_real maxY = std::numeric_limits<double>::quiet_NaN();
  bool skipExtentsTest() const {
    return std::isnan(minX) && std::isnan(minY) && std::isnan(maxX) && std::isnan(maxY);
  }

  // winding number test input points
  std::vector<cavc_point> windingNumberTestPts;
  // expected winding number results for test pts
  std::vector<int> windingNumberResults;
  bool skipWindingNumberTest() const { return windingNumberTestPts.empty(); }

  // add a winding number test input point
  void addWindingNumberTestPt(cavc_point testPt, int result) {
    windingNumberTestPts.push_back(testPt);
    windingNumberResults.push_back(result);
  }

  // closest point test input points
  std::vector<cavc_point> closestPointTestPts;
  // expected results from closest point calculation
  std::vector<uint32_t> closestPointIndexResults;
  std::vector<cavc_point> closestPointResults;
  std::vector<cavc_real> closestPointDistanceResults;
  bool skipClosestPointTest() const { return closestPointTestPts.empty(); }

  // add a closestPoint test input point
  void addClosestPointTestPt(cavc_point testPt, cavc_point closestPointResult,
                             cavc_real distanceResult,
                             uint32_t indexResult = std::numeric_limits<uint32_t>::max()) {
    closestPointTestPts.push_back(testPt);
    closestPointResults.push_back(closestPointResult);
    closestPointDistanceResults.push_back(distanceResult);
    closestPointIndexResults.push_back(indexResult);
  }

  std::vector<cavc_real> offsetTestDeltas;
  std::vector<std::vector<cavc_vertex>> resultOffsetPlines;
  bool skipOffsetTest() const { return offsetTestDeltas.empty(); }

  // add an offset test input
  void addOffsetTest(cavc_real offsetDelta, std::vector<cavc_vertex> vertexes) {
    offsetTestDeltas.push_back(offsetDelta);
    resultOffsetPlines.emplace_back(std::move(vertexes));
  }

  std::vector<cavc_real> collapsedOffsetDeltas;
  void addCollapsedOffsetTest(cavc_real offsetDelta) {
    collapsedOffsetDeltas.push_back(offsetDelta);
  }
  bool skipCollapsedOffsetTest() const { return collapsedOffsetDeltas.empty(); }

  bool isClosed() const { return cavc_pline_is_closed(pline); }
};

std::ostream &operator<<(std::ostream &os, cavc_plineFunctionsTestCase const &c) {
  os << c.name;
  return os;
}

// Adds closest point test points for ontop of all the vertexes of the test case
void addClosestPointOnVertexes(cavc_plineFunctionsTestCase &testCase) {
  if (testCase.plineVertexes.empty()) {
    return;
  }
  auto &vertexes = testCase.plineVertexes;
  for (std::size_t i = 0; i < vertexes.size() - 1; ++i) {
    auto const &v = vertexes[i];
    testCase.addClosestPointTestPt({v.x, v.y}, {v.x, v.y}, 0.0, static_cast<uint32_t>(i));
  }

  std::size_t lastPtIndex = testCase.isClosed() ? vertexes.size() - 1 : vertexes.size() - 2;
  auto const &v = vertexes.back();
  testCase.addClosestPointTestPt({v.x, v.y}, {v.x, v.y}, 0.0, static_cast<uint32_t>(lastPtIndex));
}

void addCircleCases(std::vector<cavc_plineFunctionsTestCase> &cases, cavc_real circleRadius,
                    cavc_point circleCenter, bool reverse) {
  cavc_real expectedArea = PI() * circleRadius * circleRadius;
  cavc_real expectedLength = 2 * PI() * circleRadius;

  auto createCaseName = [&](std::string const &n) {
    std::stringstream ss;
    ss << n;
    if (reverse) {
      ss << "_rev";
    }
    ss << " (radius: " << circleRadius << ", center: " << circleCenter << ")";
    return ss.str();
  };

  auto addCase = [&](std::string const &name, std::vector<cavc_vertex> const &vertexes,
                     int direction) {
    cavc_plineFunctionsTestCase testCase(createCaseName(name), vertexes, true);
    testCase.signedArea = direction * expectedArea;
    testCase.pathLength = expectedLength;
    testCase.minX = circleCenter.x - circleRadius;
    testCase.minY = circleCenter.y - circleRadius;
    testCase.maxX = circleCenter.x + circleRadius;
    testCase.maxY = circleCenter.y + circleRadius;

    // axis aligned and center cases
    testCase.addWindingNumberTestPt({testCase.minX - 0.01, circleCenter.y}, 0);
    testCase.addWindingNumberTestPt({testCase.maxX + 0.01, circleCenter.y}, 0);
    testCase.addWindingNumberTestPt({circleCenter.x, testCase.minY - 0.01}, 0);
    testCase.addWindingNumberTestPt({circleCenter.x, testCase.maxY + 0.01}, 0);
    testCase.addWindingNumberTestPt(circleCenter, direction);
    testCase.addWindingNumberTestPt({testCase.minX + 0.01, circleCenter.y}, direction);
    testCase.addWindingNumberTestPt({testCase.maxX - 0.01, circleCenter.y}, direction);
    testCase.addWindingNumberTestPt({circleCenter.x, testCase.minY + 0.01}, direction);
    testCase.addWindingNumberTestPt({circleCenter.x, testCase.maxY - 0.01}, direction);

    addClosestPointOnVertexes(testCase);

    testCase.addClosestPointTestPt({circleCenter.x - 0.1, circleCenter.y},
                                   {circleCenter.x - circleRadius, circleCenter.y},
                                   circleRadius - 0.1);
    testCase.addClosestPointTestPt({circleCenter.x + 0.1, circleCenter.y},
                                   {circleCenter.x + circleRadius, circleCenter.y},
                                   circleRadius - 0.1);
    testCase.addClosestPointTestPt({circleCenter.x, circleCenter.y - 0.1},
                                   {circleCenter.x, circleCenter.y - circleRadius},
                                   circleRadius - 0.1);
    testCase.addClosestPointTestPt({circleCenter.x, circleCenter.y + 0.1},
                                   {circleCenter.x, circleCenter.y + circleRadius},
                                   circleRadius - 0.1);

    // points at 45 deg inside and outside
    std::vector<cavc_point> insideAt45Deg;
    insideAt45Deg.reserve(4);
    std::vector<cavc_point> outsideAt45Deg;
    outsideAt45Deg.reserve(4);
    cavc_real insideDist = 0.33 * circleRadius;
    cavc_real outsideDist = 1.5 * circleRadius;
    std::vector<cavc_point> onCirclAt45deg;
    onCirclAt45deg.reserve(4);
    for (std::size_t i = 0; i < 4; ++i) {
      cavc_real xCos = std::cos(PI() / 4 + i * PI() / 2);
      cavc_real ySin = std::sin(PI() / 4 + i * PI() / 2);

      cavc_real x = circleCenter.x + insideDist * xCos;
      cavc_real y = circleCenter.y + insideDist * ySin;
      insideAt45Deg.push_back({x, y});

      x = circleCenter.x + outsideDist * xCos;
      y = circleCenter.y + outsideDist * ySin;
      outsideAt45Deg.push_back({x, y});

      x = circleCenter.x + circleRadius * xCos;
      y = circleCenter.y + circleRadius * ySin;
      onCirclAt45deg.push_back({x, y});
    }

    for (std::size_t i = 0; i < insideAt45Deg.size(); ++i) {
      testCase.addWindingNumberTestPt(insideAt45Deg[i], direction);
      testCase.addWindingNumberTestPt(outsideAt45Deg[i], 0);
      testCase.addClosestPointTestPt(insideAt45Deg[i], onCirclAt45deg[i],
                                     circleRadius - insideDist);
      testCase.addClosestPointTestPt(outsideAt45Deg[i], onCirclAt45deg[i],
                                     outsideDist - circleRadius);
    }

    cavc_real offsetOutwardDelta = -direction * 0.25 * circleRadius;
    std::vector<cavc_vertex> offsetOutwardVertexes;

    cavc_real offsetInwardDelta = direction * 0.5 * circleRadius;
    std::vector<cavc_vertex> offsetInwardVertexes;

    for (auto const &v : testCase.plineVertexes) {
      cavc_point dirVec = {v.x - circleCenter.x, v.y - circleCenter.y};
      cavc_real dirVecLength = std::sqrt(dirVec.x * dirVec.x + dirVec.y * dirVec.y);
      cavc_point unitDirVec = {dirVec.x / dirVecLength, dirVec.y / dirVecLength};
      cavc_real outwardMagnitude = circleRadius + std::abs(offsetOutwardDelta);
      cavc_real inwardMagnitude = circleRadius - std::abs(offsetInwardDelta);
      offsetOutwardVertexes.push_back({outwardMagnitude * unitDirVec.x + circleCenter.x,
                                       outwardMagnitude * unitDirVec.y + circleCenter.y, v.bulge});
      offsetInwardVertexes.push_back({inwardMagnitude * unitDirVec.x + circleCenter.x,
                                      inwardMagnitude * unitDirVec.y + circleCenter.y, v.bulge});
    }
    testCase.addOffsetTest(offsetOutwardDelta, std::move(offsetOutwardVertexes));
    testCase.addOffsetTest(offsetInwardDelta, std::move(offsetInwardVertexes));

    // offsetting inward by full radius or more should result in no offsets
    testCase.addCollapsedOffsetTest(direction * circleRadius);
    testCase.addCollapsedOffsetTest(direction * 1.5 * circleRadius);
    testCase.addCollapsedOffsetTest(direction * 2.0 * circleRadius);

    cases.push_back(std::move(testCase));
  };

  std::vector<cavc_vertex> vertexes = {{circleCenter.x - circleRadius, circleCenter.y, 1},
                                       {circleCenter.x + circleRadius, circleCenter.y, 1}};
  if (reverse) {
    std::reverse(vertexes.begin(), vertexes.end());
  }

  addCase("ccw_circle_x_aligned", vertexes, 1);

  for (auto &v : vertexes) {
    v = {v.x, v.y, -v.bulge};
  }
  addCase("cw_circle_x_aligned", vertexes, -1);

  vertexes = {{circleCenter.x, circleCenter.y - circleRadius, 1},
              {circleCenter.x, circleCenter.y + circleRadius, 1}};
  if (reverse) {
    std::reverse(vertexes.begin(), vertexes.end());
  }
  addCase("ccw_circle_y_aligned", vertexes, 1);

  for (auto &v : vertexes) {
    v = {v.x, v.y, -v.bulge};
  }
  addCase("cw_circle_y_aligned", vertexes, -1);

  vertexes = {{circleCenter.x + circleRadius * std::cos(PI() / 4),
               circleCenter.y + circleRadius * std::sin(PI() / 4), 1},
              {circleCenter.x + circleRadius * std::cos(5 * PI() / 4),
               circleCenter.y + circleRadius * std::sin(5 * PI() / 4), 1}};
  if (reverse) {
    std::reverse(vertexes.begin(), vertexes.end());
  }
  addCase("ccw_circle_not_axis_aligned", vertexes, 1);

  for (auto &v : vertexes) {
    v = {v.x, v.y, -v.bulge};
  }
  addCase("cw_circle_not_axis_aligned", vertexes, -1);
}

std::vector<cavc_plineFunctionsTestCase> createCircleCases() {
  std::vector<cavc_plineFunctionsTestCase> result;
  addCircleCases(result, 5.0, {1, 1}, false);
  addCircleCases(result, 5.0, {-1, 1}, false);
  addCircleCases(result, 5.0, {-1, -1}, false);
  addCircleCases(result, 5.0, {1, -1}, false);

  addCircleCases(result, 5.0, {1, 1}, true);
  addCircleCases(result, 5.0, {-1, 1}, true);
  addCircleCases(result, 5.0, {-1, -1}, true);
  addCircleCases(result, 5.0, {1, -1}, true);
  return result;
}

void addHalfCircleCases(std::vector<cavc_plineFunctionsTestCase> &cases, cavc_real circleRadius,
                        cavc_point circleCenter, bool isClosed) {
  cavc_real expectedArea = isClosed ? PI() * circleRadius * circleRadius / 2 : 0;
  cavc_real expectedLength = PI() * circleRadius;
  if (isClosed) {
    expectedLength += 2 * circleRadius;
  }

  auto createCaseName = [&](std::string const &n) {
    std::stringstream ss;
    ss << n;
    if (isClosed) {
      ss << "_closed";
    } else {
      ss << "_open";
    }
    ss << " (radius: " << circleRadius << ", center: " << circleCenter << ")";
    return ss.str();
  };

  auto addCase = [&](std::string const &name, std::vector<cavc_vertex> const &vertexes,
                     int direction, bool isXAligned) {
    cavc_plineFunctionsTestCase testCase(createCaseName(name), vertexes, isClosed);
    testCase.signedArea = direction * expectedArea;
    testCase.pathLength = expectedLength;
    testCase.minX = circleCenter.x - circleRadius;
    testCase.minY = circleCenter.y - circleRadius;
    testCase.maxX = circleCenter.x + circleRadius;
    testCase.maxY = circleCenter.y + circleRadius;
    // set extents (note we only start from x negative or y negative)
    if (direction > 0) {
      if (isXAligned) {
        testCase.maxY -= circleRadius;
      } else {
        testCase.minX += circleRadius;
      }
    } else {
      if (isXAligned) {
        testCase.minY += circleRadius;
      } else {
        testCase.maxX -= circleRadius;
      }
    }

    int expectedWindingInside = isClosed ? direction : 0;
    // axis aligned and center cases
    testCase.addWindingNumberTestPt({testCase.minX - 0.01, circleCenter.y}, 0);
    testCase.addWindingNumberTestPt({testCase.maxX + 0.01, circleCenter.y}, 0);
    testCase.addWindingNumberTestPt({circleCenter.x, testCase.minY - 0.01}, 0);
    testCase.addWindingNumberTestPt({circleCenter.x, testCase.maxY + 0.01}, 0);

    if (isXAligned) {
      testCase.addWindingNumberTestPt({circleCenter.x, testCase.minY + 0.01},
                                      expectedWindingInside);
      testCase.addWindingNumberTestPt({circleCenter.x, testCase.maxY - 0.01},
                                      expectedWindingInside);

    } else {
      testCase.addWindingNumberTestPt({testCase.minX + 0.01, circleCenter.y},
                                      expectedWindingInside);
      testCase.addWindingNumberTestPt({testCase.maxX - 0.01, circleCenter.y},
                                      expectedWindingInside);
    }

    addClosestPointOnVertexes(testCase);

    // if not closed then index will always be 0 since it returns the starting vertex of the segment
    // closest
    uint32_t endPointIndex = isClosed ? 1 : 0;
    if (isXAligned) {
      // test just outside ends
      testCase.addClosestPointTestPt({testCase.minX - 0.01, circleCenter.y},
                                     {testCase.minX, circleCenter.y}, 0.01, 0);
      testCase.addClosestPointTestPt({testCase.maxX + 0.01, circleCenter.y},
                                     {testCase.maxX, circleCenter.y}, 0.01, endPointIndex);
      // test near arc segment midpoint
      cavc_real arcMidpointY = direction > 0 ? testCase.minY : testCase.maxY;
      testCase.addClosestPointTestPt({circleCenter.x, arcMidpointY - 0.01},
                                     {circleCenter.x, arcMidpointY}, 0.01, 0);
      testCase.addClosestPointTestPt({circleCenter.x, arcMidpointY + 0.01},
                                     {circleCenter.x, arcMidpointY}, 0.01, 0);
      if (isClosed) {
        // test near straight segment midpoint
        testCase.addClosestPointTestPt({circleCenter.x, circleCenter.y - 0.01},
                                       {circleCenter.x, circleCenter.y}, 0.01, 1);
        testCase.addClosestPointTestPt({circleCenter.x, circleCenter.y + 0.01},
                                       {circleCenter.x, circleCenter.y}, 0.01, 1);
      }
    } else {
      // test just outside ends
      testCase.addClosestPointTestPt({circleCenter.x, testCase.minY - 0.01},
                                     {circleCenter.x, testCase.minY}, 0.01, 0);
      testCase.addClosestPointTestPt({circleCenter.x, testCase.maxY + 0.01},
                                     {circleCenter.x, testCase.maxY}, 0.01, endPointIndex);
      // test near arc segment midpoint
      cavc_real arcMidpointX = direction > 0 ? testCase.maxX : testCase.minX;
      testCase.addClosestPointTestPt({arcMidpointX - 0.01, circleCenter.y},
                                     {arcMidpointX, circleCenter.y}, 0.01, 0);
      testCase.addClosestPointTestPt({arcMidpointX + 0.01, circleCenter.y},
                                     {arcMidpointX, circleCenter.y}, 0.01, 0);
      if (isClosed) {
        // test near straight segment midpoint
        testCase.addClosestPointTestPt({circleCenter.x - 0.01, circleCenter.y},
                                       {circleCenter.x, circleCenter.y}, 0.01, 1);
        testCase.addClosestPointTestPt({circleCenter.x + 0.01, circleCenter.y},
                                       {circleCenter.x, circleCenter.y}, 0.01, 1);
      }
    }

    cavc_real offsetOutwardDelta = -direction * 0.25 * circleRadius;
    std::vector<cavc_vertex> offsetOutwardVertexes;

    cavc_real offsetInwardDelta = direction * 0.4 * circleRadius;
    std::vector<cavc_vertex> offsetInwardVertexes;

    cavc_real absOutwardDelta = std::abs(offsetOutwardDelta);
    cavc_real absInwardDelta = std::abs(offsetInwardDelta);
    cavc_real outwardMagnitude = circleRadius + absOutwardDelta;
    cavc_real inwardMagnitude = circleRadius - absInwardDelta;

    for (auto const &v : testCase.plineVertexes) {
      cavc_point dirVec = {v.x - circleCenter.x, v.y - circleCenter.y};
      cavc_real dirVecLength = std::sqrt(dirVec.x * dirVec.x + dirVec.y * dirVec.y);
      cavc_point unitDirVec = {dirVec.x / dirVecLength, dirVec.y / dirVecLength};
      offsetOutwardVertexes.push_back({outwardMagnitude * unitDirVec.x + circleCenter.x,
                                       outwardMagnitude * unitDirVec.y + circleCenter.y, v.bulge});
      offsetInwardVertexes.push_back({inwardMagnitude * unitDirVec.x + circleCenter.x,
                                      inwardMagnitude * unitDirVec.y + circleCenter.y, v.bulge});
    }
    if (isClosed) {
      // must add connection arc vertexes for outward offset and find intersects for inward offset
      // (4 axis aligned cases)

      // for computing circle intersects on inward offsets
      auto intersectsAtYVal = [&](cavc_real y) {
        cavc_real yTerm = (y - circleCenter.y);
        cavc_real r = circleRadius - absInwardDelta;
        cavc_real root = std::sqrt(r * r - yTerm * yTerm);
        return std::make_pair(cavc_point{circleCenter.x + root, y},
                              cavc_point{circleCenter.x - root, y});
      };
      auto intersectsAtXVal = [&](cavc_real x) {
        cavc_real xTerm = (x - circleCenter.x);
        cavc_real r = circleRadius - absInwardDelta;
        cavc_real root = std::sqrt(r * r - xTerm * xTerm);
        return std::make_pair(cavc_point{x, circleCenter.y + root},
                              cavc_point{x, circleCenter.y - root});
      };
      // for finding bulge between points for inward offsets
      auto pointAngle = [&](cavc_point p) {
        cavc_point v = {p.x - circleCenter.x, p.y - circleCenter.y};
        cavc_real a = std::atan2(v.y, v.x);

        return a;
      };
      auto absBulgeBetweenPoints = [&](cavc_point p1, cavc_point p2) {
        cavc_real a1 = pointAngle(p1);
        cavc_real a2 = pointAngle(p2);
        cavc_real aDiff = a1 - a2;
        aDiff = std::fmod((aDiff + PI()), (2 * PI())) - PI();
        return std::abs(std::tan(aDiff / 4));
      };

      cavc_real rightAngleBulge = std::tan(PI() / 8);
      if (isXAligned) {
        if (direction > 0) {
          // line segment is on y max edge
          offsetOutwardVertexes.back().bulge = rightAngleBulge;
          offsetOutwardVertexes.push_back({testCase.maxX, circleCenter.y + absOutwardDelta, 0});
          offsetOutwardVertexes.push_back(
              {testCase.minX, circleCenter.y + absOutwardDelta, rightAngleBulge});

          cavc_real yIntr = circleCenter.y - absInwardDelta;
          auto intrPoints = intersectsAtYVal(yIntr);
          cavc_real absBulge = absBulgeBetweenPoints(intrPoints.first, intrPoints.second);
          offsetInwardVertexes[0] = {intrPoints.first.x, intrPoints.first.y, 0};
          offsetInwardVertexes[1] = {intrPoints.second.x, intrPoints.second.y, absBulge};
        } else {
          // line segment on y min edge
          offsetOutwardVertexes.back().bulge = -rightAngleBulge;
          offsetOutwardVertexes.push_back({testCase.maxX, circleCenter.y - absOutwardDelta, 0});
          offsetOutwardVertexes.push_back(
              {testCase.minX, circleCenter.y - absOutwardDelta, -rightAngleBulge});

          cavc_real yIntr = circleCenter.y + absInwardDelta;
          auto intrPoints = intersectsAtYVal(yIntr);
          cavc_real absBulge = absBulgeBetweenPoints(intrPoints.first, intrPoints.second);
          offsetInwardVertexes[0] = {intrPoints.first.x, intrPoints.first.y, 0};
          offsetInwardVertexes[1] = {intrPoints.second.x, intrPoints.second.y, -absBulge};
        }
      } else {
        if (direction > 0) {
          // line segment on x min edge
          offsetOutwardVertexes.back().bulge = rightAngleBulge;
          offsetOutwardVertexes.push_back({circleCenter.x - absOutwardDelta, testCase.maxY, 0});
          offsetOutwardVertexes.push_back(
              {circleCenter.x - absOutwardDelta, testCase.minY, rightAngleBulge});

          cavc_real xIntr = circleCenter.x + absInwardDelta;
          auto intrPoints = intersectsAtXVal(xIntr);
          cavc_real absBulge = absBulgeBetweenPoints(intrPoints.first, intrPoints.second);
          offsetInwardVertexes[0] = {intrPoints.first.x, intrPoints.first.y, 0};
          offsetInwardVertexes[1] = {intrPoints.second.x, intrPoints.second.y, absBulge};
        } else {
          // line segment on x max edge
          offsetOutwardVertexes.back().bulge = -rightAngleBulge;
          offsetOutwardVertexes.push_back({circleCenter.x + absOutwardDelta, testCase.maxY, 0});
          offsetOutwardVertexes.push_back(
              {circleCenter.x + absOutwardDelta, testCase.minY, -rightAngleBulge});

          cavc_real xIntr = circleCenter.x - absInwardDelta;
          auto intrPoints = intersectsAtXVal(xIntr);
          cavc_real absBulge = absBulgeBetweenPoints(intrPoints.first, intrPoints.second);
          offsetInwardVertexes[0] = {intrPoints.first.x, intrPoints.first.y, 0};
          offsetInwardVertexes[1] = {intrPoints.second.x, intrPoints.second.y, -absBulge};
        }
      }
    }
    testCase.addOffsetTest(offsetOutwardDelta, std::move(offsetOutwardVertexes));
    testCase.addOffsetTest(offsetInwardDelta, std::move(offsetInwardVertexes));

    if (isClosed) {
      // offsetting inward by half radius or more should result in no offsets if closed
      testCase.addCollapsedOffsetTest(direction * 0.5 * circleRadius);
    } else {
      testCase.addCollapsedOffsetTest(direction * circleRadius);
    }
    testCase.addCollapsedOffsetTest(direction * 1.5 * circleRadius);
    testCase.addCollapsedOffsetTest(direction * 2.0 * circleRadius);

    cases.push_back(std::move(testCase));
  };

  std::vector<cavc_vertex> vertexes = {{circleCenter.x - circleRadius, circleCenter.y, 1},
                                       {circleCenter.x + circleRadius, circleCenter.y, 0}};

  addCase("ccw_half_circle_x_aligned", vertexes, 1, true);
  vertexes[0].bulge = -1;
  addCase("cw_half_circle_x_aligned", vertexes, -1, true);

  vertexes = {{circleCenter.x, circleCenter.y - circleRadius, 1},
              {circleCenter.x, circleCenter.y + circleRadius, 0}};
  addCase("ccw_half_circle_y_aligned", vertexes, 1, false);
  vertexes[0].bulge = -1;
  addCase("cw_half_circle_y_aligned", vertexes, -1, false);
}

std::vector<cavc_plineFunctionsTestCase> createHalfCircleCases() {
  std::vector<cavc_plineFunctionsTestCase> result;
  addHalfCircleCases(result, 5.0, {1, 1}, false);
  addHalfCircleCases(result, 5.0, {-1, 1}, false);
  addHalfCircleCases(result, 5.0, {-1, -1}, false);
  addHalfCircleCases(result, 5.0, {1, -1}, false);

  addHalfCircleCases(result, 5.0, {1, 1}, true);
  addHalfCircleCases(result, 5.0, {-1, 1}, true);
  addHalfCircleCases(result, 5.0, {-1, -1}, true);
  addHalfCircleCases(result, 5.0, {1, -1}, true);

  return result;
}

class cavc_plineFunctionTests : public t::TestWithParam<cavc_plineFunctionsTestCase> {
protected:
  void SetUp() override;
  void TearDown() override;

  static void SetUpTestSuite() {}
  static void TearDownTestSuite() {}

public:
  static std::vector<cavc_plineFunctionsTestCase> circleCases;
  static std::vector<cavc_plineFunctionsTestCase> halfCircleCases;
};
void cavc_plineFunctionTests::SetUp() {}
void cavc_plineFunctionTests::TearDown() {}

std::vector<cavc_plineFunctionsTestCase> cavc_plineFunctionTests::circleCases = createCircleCases();
std::vector<cavc_plineFunctionsTestCase> cavc_plineFunctionTests::halfCircleCases =
    createHalfCircleCases();

INSTANTIATE_TEST_SUITE_P(cavc_pline_circles, cavc_plineFunctionTests,
                         t::ValuesIn(cavc_plineFunctionTests::circleCases));

INSTANTIATE_TEST_SUITE_P(cavc_pline_half_circles, cavc_plineFunctionTests,
                         t::ValuesIn(cavc_plineFunctionTests::halfCircleCases));

TEST_P(cavc_plineFunctionTests, cavc_get_path_length) {
  cavc_plineFunctionsTestCase const &testCase = GetParam();
  if (testCase.skipPathLengthTest()) {
    GTEST_SKIP();
  }
  ASSERT_NEAR(cavc_get_path_length(testCase.pline), testCase.pathLength, TEST_EPSILON());
}

TEST_P(cavc_plineFunctionTests, cavc_get_area) {
  cavc_plineFunctionsTestCase const &testCase = GetParam();
  if (testCase.skipAreaTest()) {
    GTEST_SKIP();
  }
  ASSERT_NEAR(cavc_get_area(testCase.pline), testCase.signedArea, TEST_EPSILON());
}

TEST_P(cavc_plineFunctionTests, cavc_get_winding_number) {
  cavc_plineFunctionsTestCase const &testCase = GetParam();
  if (testCase.skipWindingNumberTest()) {
    GTEST_SKIP();
  }
  std::vector<int> windingNumberResults;
  windingNumberResults.reserve(testCase.windingNumberTestPts.size());
  for (auto const &pt : testCase.windingNumberTestPts) {
    windingNumberResults.push_back(cavc_get_winding_number(testCase.pline, pt));
  }

  ASSERT_THAT(windingNumberResults, t::Pointwise(t::Eq(), testCase.windingNumberResults));
}

TEST_P(cavc_plineFunctionTests, cavc_get_extents) {
  cavc_plineFunctionsTestCase const &testCase = GetParam();
  if (testCase.skipExtentsTest()) {
    GTEST_SKIP();
  }
  cavc_real minX;
  cavc_real minY;
  cavc_real maxX;
  cavc_real maxY;
  cavc_get_extents(testCase.pline, &minX, &minY, &maxX, &maxY);
  ASSERT_NEAR(minX, testCase.minX, TEST_EPSILON());
  ASSERT_NEAR(minY, testCase.minY, TEST_EPSILON());
  ASSERT_NEAR(maxX, testCase.maxX, TEST_EPSILON());
  ASSERT_NEAR(maxY, testCase.maxY, TEST_EPSILON());
}

TEST_P(cavc_plineFunctionTests, cavc_get_closest_point) {
  cavc_plineFunctionsTestCase const &testCase = GetParam();
  if (testCase.skipClosestPointTest()) {
    GTEST_SKIP();
  }
  std::size_t ptCount = testCase.closestPointTestPts.size();
  std::vector<uint32_t> indexResults;
  indexResults.reserve(ptCount);
  std::vector<cavc_point> pointResults;
  pointResults.reserve(ptCount);
  std::vector<cavc_real> distanceResults;
  distanceResults.reserve(ptCount);

  for (std::size_t i = 0; i < testCase.closestPointTestPts.size(); ++i) {
    auto const &pt = testCase.closestPointTestPts[i];
    uint32_t index;
    cavc_point p;
    cavc_real dist;
    cavc_get_closest_point(testCase.pline, pt, &index, &p, &dist);
    // set to max index so they compare equal (effectively skipping start index check)
    if (testCase.closestPointIndexResults[i] == std::numeric_limits<uint32_t>::max()) {
      indexResults.push_back(std::numeric_limits<uint32_t>::max());
    } else {
      indexResults.push_back(index);
    }
    pointResults.push_back(p);
    distanceResults.push_back(dist);
  }

  ASSERT_THAT(indexResults, t::Pointwise(t::Eq(), testCase.closestPointIndexResults));
  ASSERT_THAT(pointResults, t::Pointwise(PointFuzzyEqual(), testCase.closestPointResults));
  ASSERT_THAT(distanceResults,
              t::Pointwise(t::DoubleNear(TEST_EPSILON()), testCase.closestPointDistanceResults));
}

TEST_P(cavc_plineFunctionTests, cavc_parallel_offset) {
  cavc_plineFunctionsTestCase const &testCase = GetParam();
  if (testCase.skipOffsetTest() && testCase.skipCollapsedOffsetTest()) {
    GTEST_SKIP();
  }

  if (!testCase.skipOffsetTest()) {
    std::vector<cavc_pline_list *> results(testCase.offsetTestDeltas.size());
    for (std::size_t i = 0; i < testCase.offsetTestDeltas.size(); ++i) {
      cavc_parallel_offset(testCase.pline, testCase.offsetTestDeltas[i], &results[i], 0);
    }

    // test there is only one resulting offset pline
    ASSERT_THAT(results, t::Each(t::ResultOf(cavc_pline_list_count, t::Eq(1))));

    std::vector<cavc_pline *> resultPlines;
    resultPlines.reserve(results.size());
    for (cavc_pline_list const *list : results) {
      resultPlines.push_back(cavc_pline_list_get(list, 0));
    }

    int caseIsClosed = testCase.isClosed() ? 1 : 0;
    ASSERT_THAT(resultPlines, t::Each(t::ResultOf(cavc_pline_is_closed, t::Eq(caseIsClosed))));

    // test all vertex values
    std::vector<std::vector<cavc_vertex>> vertexResults;
    for (cavc_pline *pline : resultPlines) {
      vertexResults.push_back(std::vector<cavc_vertex>(cavc_pline_vertex_count(pline)));
      cavc_pline_vertex_data(pline, &vertexResults.back()[0]);
    }

    ASSERT_THAT(vertexResults, t::Pointwise(VertexListsFuzzyEqual(testCase.isClosed()),
                                            testCase.resultOffsetPlines));

    for (auto result : results) {
      cavc_pline_list_delete(result);
    }
  }

  if (!testCase.skipCollapsedOffsetTest()) {
    std::vector<cavc_pline_list *> results(testCase.collapsedOffsetDeltas.size());
    for (std::size_t i = 0; i < testCase.collapsedOffsetDeltas.size(); ++i) {
      cavc_parallel_offset(testCase.pline, testCase.collapsedOffsetDeltas[i], &results[i], 0);
    }
    ASSERT_THAT(results, t::Each(t::ResultOf(cavc_pline_list_count, t::Eq(0))));
    for (auto result : results) {
      cavc_pline_list_delete(result);
    }
  }
}

TEST_P(cavc_plineFunctionTests, cavc_combine_with_self_invariants) {
  cavc_plineFunctionsTestCase const &testCase = GetParam();
  if (!testCase.isClosed()) {
    GTEST_SKIP();
  }
  cavc_pline_list *remaining = nullptr;
  cavc_pline_list *subtracted = nullptr;

  // test union with self is always self
  cavc_combine_plines(testCase.pline, testCase.pline, 0, &remaining, &subtracted);
  ASSERT_EQ(cavc_pline_list_count(remaining), 1);
  ASSERT_EQ(cavc_pline_list_count(subtracted), 0);
  cavc_pline *combineResult = cavc_pline_list_get(remaining, 0);
  std::vector<cavc_vertex> resultVertexes(cavc_pline_vertex_count(combineResult));
  cavc_pline_vertex_data(combineResult, &resultVertexes[0]);
  ASSERT_EQ(resultVertexes.size(), testCase.plineVertexes.size());
  ASSERT_THAT(resultVertexes, t::Pointwise(VertexEqual(), testCase.plineVertexes));
  cavc_pline_list_delete(remaining);
  cavc_pline_list_delete(subtracted);

  // test exclude with self is always empty
  cavc_combine_plines(testCase.pline, testCase.pline, 1, &remaining, &subtracted);
  ASSERT_EQ(cavc_pline_list_count(remaining), 0);
  ASSERT_EQ(cavc_pline_list_count(subtracted), 0);
  cavc_pline_list_delete(remaining);
  cavc_pline_list_delete(subtracted);

  // test intersect with self is always self
  cavc_combine_plines(testCase.pline, testCase.pline, 2, &remaining, &subtracted);
  ASSERT_EQ(cavc_pline_list_count(remaining), 1);
  ASSERT_EQ(cavc_pline_list_count(subtracted), 0);
  combineResult = cavc_pline_list_get(remaining, 0);
  resultVertexes.resize(cavc_pline_vertex_count(combineResult));
  cavc_pline_vertex_data(combineResult, &resultVertexes[0]);
  ASSERT_EQ(resultVertexes.size(), testCase.plineVertexes.size());
  ASSERT_THAT(resultVertexes, t::Pointwise(VertexEqual(), testCase.plineVertexes));
  cavc_pline_list_delete(remaining);
  cavc_pline_list_delete(subtracted);

  // test XOR with self is always empty
  cavc_combine_plines(testCase.pline, testCase.pline, 3, &remaining, &subtracted);
  ASSERT_EQ(cavc_pline_list_count(remaining), 0);
  ASSERT_EQ(cavc_pline_list_count(subtracted), 0);
  cavc_pline_list_delete(remaining);
  cavc_pline_list_delete(subtracted);
}

int main(int argc, char **argv) {
  t::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
