#ifndef CAVC_POLYLINEINTERSECTS_HPP
#define CAVC_POLYLINEINTERSECTS_HPP
#include "mathutils.hpp"
#include "polyline.hpp"
#include "vector2.hpp"
#include <unordered_set>
#include <vector>

// This header has functions for finding and working with polyline intersects (self intersects and
// intersects between polylines)

namespace cavc {
/// Represents a non-coincident polyline intersect.
template <typename Real> struct PlineIntersect {
  /// Index of the start vertex of the first segment
  std::size_t sIndex1;
  /// Index of the start vertex of the second segment
  std::size_t sIndex2;
  /// Point of intersection
  Vector2<Real> pos;
  // type of intersect
  PlineIntersect() = default;
  PlineIntersect(std::size_t si1, std::size_t si2, Vector2<Real> p)
      : sIndex1(si1), sIndex2(si2), pos(p) {}
};

/// Represents a coincident polyline intersect (stretch).
template <typename Real> struct PlineCoincidentIntersect {
  /// Index of the start vertex of the first segment
  std::size_t sIndex1;
  /// Index of the start vertex of the second segment
  std::size_t sIndex2;
  /// One end point of the coincident slice
  Vector2<Real> point1;
  /// Other end point of the coincident slice
  Vector2<Real> point2;
  PlineCoincidentIntersect() = default;
  PlineCoincidentIntersect(std::size_t si1, std::size_t si2, Vector2<Real> const &point1,
                           Vector2<Real> const &point2)
      : sIndex1(si1), sIndex2(si2), point1(point1), point2(point2) {}
};

/// Holds a collection of intersects found
template <typename Real> struct PlineIntersectsResult {
  std::vector<PlineIntersect<Real>> intersects;
  std::vector<PlineCoincidentIntersect<Real>> coincidentIntersects;
  bool hasIntersects() { return intersects.size() != 0 || coincidentIntersects.size() != 0; }
};

template <typename Real> struct CoincidentSlicesResult {
  std::vector<Polyline<Real>> coincidentSlices;
  std::vector<PlineIntersect<Real>> sliceStartPoints;
  std::vector<PlineIntersect<Real>> sliceEndPoints;
  std::vector<bool> coincidentIsOpposingDirection;
};

template <typename Real>
CoincidentSlicesResult<Real>
sortAndjoinCoincidentSlices(std::vector<PlineCoincidentIntersect<Real>> &coincidentIntrs,
                            Polyline<Real> const &pline1, Polyline<Real> const &pline2) {
  CoincidentSlicesResult<Real> result;

  if (coincidentIntrs.size() == 0) {
    return result;
  }

  for (auto &intr : coincidentIntrs) {
    auto const &sp = pline1[intr.sIndex1].pos();
    Real dist1 = distSquared(sp, intr.point1);
    Real dist2 = distSquared(sp, intr.point2);
    if (dist1 > dist2) {
      using namespace std;
      swap(intr.point1, intr.point2);
    }
  }

  std::sort(coincidentIntrs.begin(), coincidentIntrs.end(),
            [&](auto const &intr1, auto const &intr2) {
              if (intr1.sIndex1 != intr2.sIndex1) {
                return intr1.sIndex1 < intr2.sIndex1;
              }
              // equal index so sort distance from start
              auto const &sp = pline1[intr1.sIndex1].pos();
              Real dist1 = distSquared(sp, intr1.point1);
              Real dist2 = distSquared(sp, intr2.point1);
              return dist1 < dist2;
            });

  auto &sliceStartPoints = result.sliceStartPoints;
  auto &sliceEndPoints = result.sliceEndPoints;
  auto &coincidentSlices = result.coincidentSlices;
  auto &coincidentIsOpposingDirection = result.coincidentIsOpposingDirection;

  Polyline<Real> currCoincidentSlice;

  auto startCoincidentSliceAt = [&](std::size_t intrIndex) {
    const auto &intr = coincidentIntrs[intrIndex];
    const auto &v1 = pline1[intr.sIndex1];
    const auto &v2 = pline1[utils::nextWrappingIndex(intr.sIndex1, pline1)];
    const auto &u1 = pline2[intr.sIndex2];
    const auto &u2 = pline2[utils::nextWrappingIndex(intr.sIndex2, pline2)];
    auto const &t1 = segTangentVector(v1, v2, v1.pos());
    auto const &t2 = segTangentVector(u1, u2, u1.pos());
    // tangent vectors are either going same direction or opposite direction, just test dot product
    // sign to determine if going same direction
    auto dotP = dot(t1, t2);
    bool sameDirection = dotP > Real(0);
    coincidentIsOpposingDirection.push_back(!sameDirection);

    auto split1 = splitAtPoint(v1, v2, intr.point1);
    currCoincidentSlice.addVertex(split1.splitVertex);
    auto split2 = splitAtPoint(v1, v2, intr.point2);
    currCoincidentSlice.addVertex(split2.splitVertex);

    PlineIntersect<Real> sliceStart;
    sliceStart.pos = split1.splitVertex.pos();

    if (fuzzyEqual(v1.pos(), intr.point1, utils::realPrecision<Real>())) {
      // coincidence starts at beginning of segment, report as starting at end of previous index
      sliceStart.sIndex1 = utils::prevWrappingIndex(intr.sIndex1, pline1);
    } else {
      sliceStart.sIndex1 = intr.sIndex1;
    }

    if (fuzzyEqual(u1.pos(), sliceStart.pos, utils::realPrecision<Real>())) {
      sliceStart.sIndex2 = utils::prevWrappingIndex(intr.sIndex2, pline2);
    } else {
      sliceStart.sIndex2 = intr.sIndex2;
    }

    sliceStartPoints.push_back(std::move(sliceStart));
  };

  auto endCoincidentSliceAt = [&](std::size_t intrIndex) {
    const auto &intr = coincidentIntrs[intrIndex];
    const auto &u1 = pline2[intr.sIndex2];

    coincidentSlices.emplace_back();
    using namespace std;
    swap(coincidentSlices.back(), currCoincidentSlice);
    PlineIntersect<Real> sliceEnd;
    sliceEnd.pos = intr.point2;
    sliceEnd.sIndex1 = intr.sIndex1;
    if (fuzzyEqual(u1.pos(), sliceEnd.pos, utils::realPrecision<Real>())) {
      sliceEnd.sIndex2 = utils::prevWrappingIndex(intr.sIndex2, pline2);
    } else {
      sliceEnd.sIndex2 = intr.sIndex2;
    }

    sliceEndPoints.push_back(std::move(sliceEnd));
  };

  startCoincidentSliceAt(0);
  for (std::size_t i = 1; i < coincidentIntrs.size(); ++i) {
    const auto &intr = coincidentIntrs[i];
    const auto &v1 = pline1[intr.sIndex1];
    const auto &v2 = pline1[utils::nextWrappingIndex(intr.sIndex1, pline1)];

    if (fuzzyEqual(intr.point1, currCoincidentSlice.lastVertex().pos(),
                   utils::realPrecision<Real>())) {
      // continue coincident slice
      currCoincidentSlice.vertexes().pop_back();
      auto split1 = splitAtPoint(v1, v2, intr.point1);
      currCoincidentSlice.addVertex(split1.splitVertex);
      auto split2 = splitAtPoint(v1, v2, intr.point2);
      currCoincidentSlice.addVertex(split2.splitVertex);

    } else {
      // end coincident slice and start new
      endCoincidentSliceAt(i - 1);
      startCoincidentSliceAt(i);
    }
  }

  // cap off last slice
  endCoincidentSliceAt(coincidentIntrs.size() - 1);

  if (coincidentSlices.size() > 1) {
    // check if last coincident slice connects with first
    const auto &lastSliceEnd = coincidentSlices.back().lastVertex().pos();
    const auto &firstSliceBegin = coincidentSlices[0][0].pos();
    if (fuzzyEqual(lastSliceEnd, firstSliceBegin, utils::realPrecision<Real>())) {
      // they do connect, join them together
      auto &lastSlice = coincidentSlices.back();
      lastSlice.vertexes().pop_back();
      lastSlice.vertexes().insert(lastSlice.vertexes().end(),
                                  coincidentSlices[0].vertexes().begin(),
                                  coincidentSlices[0].vertexes().end());

      // cleanup
      sliceEndPoints.back() = sliceEndPoints[0];
      sliceEndPoints.erase(sliceEndPoints.begin());
      sliceStartPoints.erase(sliceStartPoints.begin());
      coincidentSlices.erase(coincidentSlices.begin());
      coincidentIsOpposingDirection.erase(coincidentIsOpposingDirection.begin());
    }
  }

  return result;
}

/// Finds all local self intersects of the polyline, local self intersects are defined as between
/// two polyline segments that share a vertex. NOTES:
/// - Singularities (repeating vertexes) are returned as coincident intersects
template <typename Real>
void localSelfIntersects(Polyline<Real> const &pline, std::vector<PlineIntersect<Real>> &output) {
  if (pline.size() < 2) {
    return;
  }

  if (pline.size() == 2) {
    if (pline.isClosed()) {
      // check if overlaps on itself from vertex 1 to vertex 2
      if (utils::fuzzyEqual(pline[0].bulge(), -pline[1].bulge())) {
        // coincident
        output.emplace_back(0, 1, pline[1].pos());
        output.emplace_back(1, 0, pline[0].pos());
      }
    }
    return;
  }

  auto testAndAddIntersect = [&](std::size_t i, std::size_t j, std::size_t k) {
    const PlineVertex<Real> &v1 = pline[i];
    const PlineVertex<Real> &v2 = pline[j];
    const PlineVertex<Real> &v3 = pline[k];
    // testing intersection between v1->v2 and v2->v3 segments

    if (fuzzyEqual(v1.pos(), v2.pos(), utils::realPrecision<Real>())) {
      // singularity
      // coincident
      output.emplace_back(i, j, v1.pos());
    } else {
      IntrPlineSegsResult<Real> intrResult = intrPlineSegs(v1, v2, v2, v3);
      switch (intrResult.intrType) {
      case PlineSegIntrType::NoIntersect:
        break;
      case PlineSegIntrType::TangentIntersect:
      case PlineSegIntrType::OneIntersect:
        if (!fuzzyEqual(intrResult.point1, v2.pos(), utils::realPrecision<Real>())) {
          output.emplace_back(i, j, intrResult.point1);
        }
        break;
      case PlineSegIntrType::TwoIntersects:
        if (!fuzzyEqual(intrResult.point1, v2.pos(), utils::realPrecision<Real>())) {
          output.emplace_back(i, j, intrResult.point1);
        }
        if (!fuzzyEqual(intrResult.point2, v2.pos(), utils::realPrecision<Real>())) {
          output.emplace_back(i, j, intrResult.point2);
        }
        break;
      case PlineSegIntrType::SegmentOverlap:
      case PlineSegIntrType::ArcOverlap:
        // coincident
        output.emplace_back(i, j, intrResult.point1);
        break;
      }
    }
  };

  for (std::size_t i = 2; i < pline.size(); ++i) {
    testAndAddIntersect(i - 2, i - 1, i);
  }

  if (pline.isClosed()) {
    // we tested for intersect between segments at indexes 0->1, 1->2 and everything up to and
    // including (count-3)->(count-2), (count-2)->(count-1), polyline is closed so now test
    // [(count-2)->(count-1), (count-1)->0] and [(count-1)->0, 0->1]
    testAndAddIntersect(pline.size() - 2, pline.size() - 1, 0);
    testAndAddIntersect(pline.size() - 1, 0, 1);
  }
}

/// Finds all global self intersects of the polyline, global self intersects are defined as all
/// intersects between polyline segments that DO NOT share a vertex (use the localSelfIntersects
/// function to find those). A spatial index is used to minimize the intersect comparisons required,
/// the spatial index should hold bounding boxes for all of the polyline's segments.
/// NOTES:
/// - We never include intersects at a segment's start point, the matching intersect from the
/// previous segment's end point is included (no sense in including both)
template <typename Real, std::size_t N>
void globalSelfIntersects(Polyline<Real> const &pline, std::vector<PlineIntersect<Real>> &output,
                          StaticSpatialIndex<Real, N> const &spatialIndex) {
  if (pline.size() < 3) {
    return;
  }

  std::unordered_set<std::pair<std::size_t, std::size_t>, internal::IndexPairHash>
      visitedSegmentPairs;
  visitedSegmentPairs.reserve(pline.size());

  std::vector<std::size_t> queryStack;
  queryStack.reserve(8);

  auto visitor = [&](std::size_t i, Real minX, Real minY, Real maxX, Real maxY) {
    std::size_t j = utils::nextWrappingIndex(i, pline);
    const PlineVertex<Real> &v1 = pline[i];
    const PlineVertex<Real> &v2 = pline[j];
    AABB<Real> envelope{minX, minY, maxX, maxY};
    envelope.expand(utils::realThreshold<Real>());
    auto indexVisitor = [&](std::size_t hitIndexStart) {
      std::size_t hitIndexEnd = utils::nextWrappingIndex(hitIndexStart, pline);
      // skip/filter already visited intersects
      // skip local segments
      if (i == hitIndexStart || i == hitIndexEnd || j == hitIndexStart || j == hitIndexEnd) {
        return true;
      }
      // skip reversed segment order (would end up comparing the same segments)
      if (visitedSegmentPairs.find({hitIndexStart, i}) != visitedSegmentPairs.end()) {
        return true;
      }

      // add the segment pair we're visiting now
      visitedSegmentPairs.emplace(i, hitIndexStart);

      const PlineVertex<Real> &u1 = pline[hitIndexStart];
      const PlineVertex<Real> &u2 = pline[hitIndexEnd];

      auto intrAtStartPt = [&](Vector2<Real> const &intr) {
        return fuzzyEqual(v1.pos(), intr) || fuzzyEqual(u1.pos(), intr);
      };

      IntrPlineSegsResult<Real> intrResult = intrPlineSegs(v1, v2, u1, u2);
      switch (intrResult.intrType) {
      case PlineSegIntrType::NoIntersect:
        break;
      case PlineSegIntrType::TangentIntersect:
      case PlineSegIntrType::OneIntersect:
        if (!intrAtStartPt(intrResult.point1)) {
          output.emplace_back(i, hitIndexStart, intrResult.point1);
        }
        break;
      case PlineSegIntrType::TwoIntersects:
        if (!intrAtStartPt(intrResult.point1)) {
          output.emplace_back(i, hitIndexStart, intrResult.point1);
        }
        if (!intrAtStartPt(intrResult.point2)) {
          output.emplace_back(i, hitIndexStart, intrResult.point2);
        }
        break;
      case PlineSegIntrType::SegmentOverlap:
      case PlineSegIntrType::ArcOverlap:
        if (!intrAtStartPt(intrResult.point1)) {
          output.emplace_back(i, hitIndexStart, intrResult.point1);
        }
        if (!intrAtStartPt(intrResult.point2)) {
          output.emplace_back(i, hitIndexStart, intrResult.point2);
        }
        break;
      }

      // visit the entire query
      return true;
    };

    spatialIndex.visitQuery(envelope.xMin, envelope.yMin, envelope.xMax, envelope.yMax,
                            indexVisitor, queryStack);

    // visit all pline indexes
    return true;
  };

  spatialIndex.visitItemBoxes(visitor);
}

/// Finds all self intersects of the polyline (equivalent to calling localSelfIntersects and
/// globalSelfIntersects).
template <typename Real, std::size_t N>
void allSelfIntersects(Polyline<Real> const &pline, std::vector<PlineIntersect<Real>> &output,
                       StaticSpatialIndex<Real, N> const &spatialIndex) {
  localSelfIntersects(pline, output);
  globalSelfIntersects(pline, output, spatialIndex);
}

/// Finds all intersects between pline1 and pline2.
template <typename Real, std::size_t N>
void findIntersects(Polyline<Real> const &pline1, Polyline<Real> const &pline2,
                    StaticSpatialIndex<Real, N> const &pline1SpatialIndex,
                    PlineIntersectsResult<Real> &output) {
  std::vector<std::size_t> queryResults;
  std::vector<std::size_t> queryStack;
  queryStack.reserve(8);
  std::unordered_set<std::pair<std::size_t, std::size_t>, internal::IndexPairHash>
      possibleDuplicates;

  auto &intrs = output.intersects;
  auto &coincidentIntrs = output.coincidentIntersects;

  auto pline2SegVisitor = [&](std::size_t i2, std::size_t j2) {
    PlineVertex<Real> const &p2v1 = pline2[i2];
    PlineVertex<Real> const &p2v2 = pline2[j2];

    queryResults.clear();

    AABB<Real> bb = createFastApproxBoundingBox(p2v1, p2v2);
    // expand bounding box by threshold amount to ensure finding intersects at segment end points
    Real fuzz = cavc::utils::realPrecision<Real>();
    pline1SpatialIndex.query(bb.xMin - fuzz, bb.yMin - fuzz, bb.xMax + fuzz, bb.yMax + fuzz,
                             queryResults, queryStack);

    for (std::size_t i1 : queryResults) {
      std::size_t j1 = utils::nextWrappingIndex(i1, pline1);
      PlineVertex<Real> const &p1v1 = pline1[i1];
      PlineVertex<Real> const &p1v2 = pline1[j1];

      auto intrAtStartPt = [&](Vector2<Real> const &intr) {
        return fuzzyEqual(p1v1.pos(), intr) || fuzzyEqual(p2v1.pos(), intr);
      };

      auto intrResult = intrPlineSegs(p1v1, p1v2, p2v1, p2v2);
      switch (intrResult.intrType) {
      case PlineSegIntrType::NoIntersect:
        break;
      case PlineSegIntrType::TangentIntersect:
      case PlineSegIntrType::OneIntersect:
        if (!intrAtStartPt(intrResult.point1)) {
          intrs.emplace_back(i1, i2, intrResult.point1);
        }
        break;
      case PlineSegIntrType::TwoIntersects:
        if (!intrAtStartPt(intrResult.point1)) {
          intrs.emplace_back(i1, i2, intrResult.point1);
        }
        if (!intrAtStartPt(intrResult.point2)) {
          intrs.emplace_back(i1, i2, intrResult.point2);
        }
        break;
      case PlineSegIntrType::SegmentOverlap:
      case PlineSegIntrType::ArcOverlap:
        coincidentIntrs.emplace_back(i1, i2, intrResult.point1, intrResult.point2);
        if (fuzzyEqual(p1v1.pos(), intrResult.point1) ||
            fuzzyEqual(p1v1.pos(), intrResult.point2)) {
          possibleDuplicates.insert({utils::prevWrappingIndex(i1, pline1), i2});
        }
        if (fuzzyEqual(p2v1.pos(), intrResult.point1) ||
            fuzzyEqual(p2v1.pos(), intrResult.point2)) {
          possibleDuplicates.insert({i1, utils::prevWrappingIndex(i2, pline2)});
        }
        break;
      }
    }

    // visit all indexes
    return true;
  };

  pline2.visitSegIndices(pline2SegVisitor);

  // remove duplicate points caused by the coincident intersect definition
  intrs.erase(std::remove_if(intrs.begin(), intrs.end(),
                             [&](auto const &intr) {
                               auto found = possibleDuplicates.find({intr.sIndex1, intr.sIndex2});
                               if (found == possibleDuplicates.end()) {
                                 return false;
                               }

                               auto const &endPt1 =
                                   pline1[utils::nextWrappingIndex(intr.sIndex1, pline1)].pos();
                               if (fuzzyEqual(intr.pos, endPt1)) {
                                 return true;
                               }

                               auto const &endPt2 =
                                   pline2[utils::nextWrappingIndex(intr.sIndex2, pline2)].pos();
                               return fuzzyEqual(intr.pos, endPt2);
                             }),
              intrs.end());
}

} // namespace cavc
#endif // CAVC_POLYLINEINTERSECTS_HPP
