#ifndef CAVC_STATICSPATIALINDEX_HPP
#define CAVC_STATICSPATIALINDEX_HPP
#include "internal/common.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stack>
#include <vector>

namespace cavc {
template <typename Real, std::size_t NodeSize = 16> class StaticSpatialIndex {
public:
  StaticSpatialIndex(std::size_t numItems) {
    CAVC_ASSERT(numItems > 0, "number of items must be greater than 0");
    static_assert(NodeSize >= 2 && NodeSize <= 65535, "node size must be between 2 and 65535");
    // calculate the total number of nodes in the R-tree to allocate space for
    // and the index of each tree level (used in search later)
    m_numItems = numItems;
    std::size_t n = numItems;
    std::size_t numNodes = numItems;

    m_numLevels = computeNumLevels(numItems);
    m_levelBounds = std::unique_ptr<std::size_t[]>(new std::size_t[m_numLevels]);
    m_levelBounds[0] = n * 4;
    // now populate level bounds and numNodes
    std::size_t i = 1;
    do {
      n = static_cast<std::size_t>(std::ceil(static_cast<float>(n) / NodeSize));
      numNodes += n;
      m_levelBounds[i] = numNodes * 4;
      i += 1;
    } while (n != 1);

    m_numNodes = numNodes;
    m_boxes = std::unique_ptr<Real[]>(new Real[numNodes * 4]);
    m_indices = std::unique_ptr<std::size_t[]>(new std::size_t[numNodes]);
    m_pos = 0;
    m_minX = std::numeric_limits<Real>::infinity();
    m_minY = std::numeric_limits<Real>::infinity();
    m_maxX = -std::numeric_limits<Real>::infinity();
    m_maxY = -std::numeric_limits<Real>::infinity();
  }

  Real minX() const { return m_minX; }
  Real minY() const { return m_minY; }
  Real maxX() const { return m_maxX; }
  Real maxY() const { return m_maxY; }

  void add(Real minX, Real minY, Real maxX, Real maxY) {
    std::size_t index = m_pos >> 2;
    m_indices[index] = index;
    m_boxes[m_pos++] = minX;
    m_boxes[m_pos++] = minY;
    m_boxes[m_pos++] = maxX;
    m_boxes[m_pos++] = maxY;

    if (minX < m_minX)
      m_minX = minX;
    if (minY < m_minY)
      m_minY = minY;
    if (maxX > m_maxX)
      m_maxX = maxX;
    if (maxY > m_maxY)
      m_maxY = maxY;
  }

  void finish() {
    CAVC_ASSERT(m_pos >> 2 == m_numItems, "added item count should equal static size given");

    // if number of items is less than node size then skip sorting since each node of boxes must be
    // fully scanned regardless and there is only one node
    if (m_numItems <= NodeSize) {
      m_indices[m_pos >> 2] = 0;
      // fill root box with total extents
      m_boxes[m_pos++] = m_minX;
      m_boxes[m_pos++] = m_minY;
      m_boxes[m_pos++] = m_maxX;
      m_boxes[m_pos++] = m_maxY;
      return;
    }

    Real width = m_maxX - m_minX;
    Real height = m_maxY - m_minY;
    std::unique_ptr<std::uint32_t[]> hilbertValues(new std::uint32_t[m_numItems]);

    std::size_t pos = 0;

    for (std::size_t i = 0; i < m_numItems; ++i) {
      pos = 4 * i;
      Real minX = m_boxes[pos++];
      Real minY = m_boxes[pos++];
      Real maxX = m_boxes[pos++];
      Real maxY = m_boxes[pos++];

      // hilbert max input value for x and y
      const Real hilbertMax = static_cast<Real>((1 << 16) - 1);
      // mapping the x and y coordinates of the center of the box to values in the range
      // [0 -> n - 1] such that the min of the entire set of bounding boxes maps to 0 and the max of
      // the entire set of bounding boxes maps to n - 1 our 2d space is x: [0 -> n-1] and
      // y: [0 -> n-1], our 1d hilbert curve value space is d: [0 -> n^2 - 1]
      Real x = std::floor(hilbertMax * ((minX + maxX) / 2 - m_minX) / width);
      std::uint32_t hx = static_cast<std::uint32_t>(x);
      Real y = std::floor(hilbertMax * ((minY + maxY) / 2 - m_minY) / height);
      std::uint32_t hy = static_cast<std::uint32_t>(y);
      hilbertValues[i] = hilbertXYToIndex(hx, hy);
    }

    // sort items by their Hilbert value (for packing later)
    sort(&hilbertValues[0], &m_boxes[0], &m_indices[0], 0, m_numItems - 1);

    // generate nodes at each tree level, bottom-up
    pos = 0;
    for (std::size_t i = 0; i < m_numLevels - 1; i++) {
      auto end = m_levelBounds[i];

      // generate a parent node for each block of consecutive <nodeSize> nodes
      while (pos < end) {
        auto nodeMinX = std::numeric_limits<Real>::infinity();
        auto nodeMinY = std::numeric_limits<Real>::infinity();
        auto nodeMaxX = -1 * std::numeric_limits<Real>::infinity();
        auto nodeMaxY = -1 * std::numeric_limits<Real>::infinity();
        auto nodeIndex = pos;

        // calculate bbox for the new node
        for (std::size_t j = 0; j < NodeSize && pos < end; j++) {
          auto minX = m_boxes[pos++];
          auto minY = m_boxes[pos++];
          auto maxX = m_boxes[pos++];
          auto maxY = m_boxes[pos++];
          if (minX < nodeMinX)
            nodeMinX = minX;
          if (minY < nodeMinY)
            nodeMinY = minY;
          if (maxX > nodeMaxX)
            nodeMaxX = maxX;
          if (maxY > nodeMaxY)
            nodeMaxY = maxY;
        }

        // add the new node to the tree data
        m_indices[m_pos >> 2] = nodeIndex;
        m_boxes[m_pos++] = nodeMinX;
        m_boxes[m_pos++] = nodeMinY;
        m_boxes[m_pos++] = nodeMaxX;
        m_boxes[m_pos++] = nodeMaxY;
      }
    }
  }

  // Visit all the bounding boxes in the spatial index. Visitor function has the signature
  // bool(std::size_t level, Real xmin, Real ymin, Real xmax, Real ymax, std::size_t level).
  // Visiting stops early if false is returned.
  template <typename F> void visitBoundingBoxes(F &&visitor) const {
    std::size_t nodeIndex = 4 * m_numNodes - 4;
    std::size_t level = m_numLevels - 1;

    std::vector<std::size_t> stack;
    stack.reserve(16);

    bool done = false;
    while (!done) {
      auto end = std::min(nodeIndex + NodeSize * 4, m_levelBounds[level]);
      for (std::size_t pos = nodeIndex; pos < end; pos += 4) {
        auto index = m_indices[pos >> 2];
        if (!visitor(level, m_boxes[pos], m_boxes[pos + 1], m_boxes[pos + 2], m_boxes[pos + 3])) {
          return;
        }

        if (nodeIndex >= m_numItems * 4) {
          stack.push_back(index);
          stack.push_back(level - 1);
        }
      }

      if (stack.size() > 1) {
        level = stack.back();
        stack.pop_back();
        nodeIndex = stack.back();
        stack.pop_back();
      } else {
        done = true;
      }
    }
  }

  // Visit only the item bounding boxes in the spatial index. Visitor function has the signature
  // bool(std::size_t index, Real xmin, Real ymin, Real xmax, Real ymax). Visiting stops early if
  // false is returned.
  template <typename F> void visitItemBoxes(F &&visitor) const {
    for (std::size_t i = 0; i < m_levelBounds[0]; i += 4) {
      if (!visitor(m_indices[i >> 2], m_boxes[i], m_boxes[i + 1], m_boxes[i + 2], m_boxes[i + 3])) {
        return;
      }
    }
  }

  // See other overloads for details.
  void query(Real minX, Real minY, Real maxX, Real maxY, std::vector<std::size_t> &results) const {
    auto visitor = [&](std::size_t index) {
      results.push_back(index);
      return true;
    };

    visitQuery(minX, minY, maxX, maxY, visitor);
  }

  // Query the spatial index adding indexes to the results vector given. This overload accepts an
  // existing vector to use as a stack and takes care of clearing the stack before use.
  void query(Real minX, Real minY, Real maxX, Real maxY, std::vector<std::size_t> &results,
             std::vector<std::size_t> &stack) const {
    auto visitor = [&](std::size_t index) {
      results.push_back(index);
      return true;
    };

    visitQuery(minX, minY, maxX, maxY, visitor, stack);
  }

  // See other overloads for details.
  template <typename F>
  void visitQuery(Real minX, Real minY, Real maxX, Real maxY, F &&visitor) const {
    std::vector<std::size_t> stack;
    stack.reserve(16);
    visitQuery(minX, minY, maxX, maxY, std::forward<F>(visitor), stack);
  }

  // Query the spatial index, invoking a visitor function for each index that overlaps the bounding
  // box given. Visitor function has the signature bool(std::size_t index), if visitor returns false
  // the query stops early, otherwise the query continues. This overload accepts an existing vector
  // to use as a stack and takes care of clearing the stack before use.
  template <typename F>
  void visitQuery(Real minX, Real minY, Real maxX, Real maxY, F &&visitor,
                  std::vector<std::size_t> &stack) const {
    CAVC_ASSERT(m_pos == 4 * m_numNodes, "data not yet indexed - call Finish() before querying");

    auto nodeIndex = 4 * m_numNodes - 4;
    auto level = m_numLevels - 1;

    stack.clear();

    auto done = false;

    while (!done) {
      // find the end index of the node
      auto end = std::min(nodeIndex + NodeSize * 4, m_levelBounds[level]);

      // search through child nodes
      for (std::size_t pos = nodeIndex; pos < end; pos += 4) {
        auto index = m_indices[pos >> 2];
        // check if node bbox intersects with query bbox
        if (maxX < m_boxes[pos] || maxY < m_boxes[pos + 1] || minX > m_boxes[pos + 2] ||
            minY > m_boxes[pos + 3]) {
          // no intersect
          continue;
        }

        if (nodeIndex < m_numItems * 4) {
          done = !visitor(index);
          if (done) {
            break;
          }
        } else {
          // push node index and level for further traversal
          stack.push_back(index);
          stack.push_back(level - 1);
        }
      }

      if (stack.size() > 1) {
        level = stack.back();
        stack.pop_back();
        nodeIndex = stack.back();
        stack.pop_back();
      } else {
        done = true;
      }
    }
  }

  static std::uint32_t hilbertXYToIndex(std::uint32_t x, std::uint32_t y) {
    std::uint32_t a = x ^ y;
    std::uint32_t b = 0xFFFF ^ a;
    std::uint32_t c = 0xFFFF ^ (x | y);
    std::uint32_t d = x & (y ^ 0xFFFF);

    std::uint32_t A = a | (b >> 1);
    std::uint32_t B = (a >> 1) ^ a;
    std::uint32_t C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
    std::uint32_t D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;

    a = A;
    b = B;
    c = C;
    d = D;
    A = (a & (a >> 2)) ^ (b & (b >> 2));
    B = (a & (b >> 2)) ^ (b & ((a ^ b) >> 2));
    C ^= (a & (c >> 2)) ^ (b & (d >> 2));
    D ^= (b & (c >> 2)) ^ ((a ^ b) & (d >> 2));

    a = A;
    b = B;
    c = C;
    d = D;
    A = (a & (a >> 4)) ^ (b & (b >> 4));
    B = (a & (b >> 4)) ^ (b & ((a ^ b) >> 4));
    C ^= (a & (c >> 4)) ^ (b & (d >> 4));
    D ^= (b & (c >> 4)) ^ ((a ^ b) & (d >> 4));

    a = A;
    b = B;
    c = C;
    d = D;
    C ^= ((a & (c >> 8)) ^ (b & (d >> 8)));
    D ^= ((b & (c >> 8)) ^ ((a ^ b) & (d >> 8)));

    a = C ^ (C >> 1);
    b = D ^ (D >> 1);

    std::uint32_t i0 = x ^ y;
    std::uint32_t i1 = b | (0xFFFF ^ (i0 | a));

    i0 = (i0 | (i0 << 8)) & 0x00FF00FF;
    i0 = (i0 | (i0 << 4)) & 0x0F0F0F0F;
    i0 = (i0 | (i0 << 2)) & 0x33333333;
    i0 = (i0 | (i0 << 1)) & 0x55555555;

    i1 = (i1 | (i1 << 8)) & 0x00FF00FF;
    i1 = (i1 | (i1 << 4)) & 0x0F0F0F0F;
    i1 = (i1 | (i1 << 2)) & 0x33333333;
    i1 = (i1 | (i1 << 1)) & 0x55555555;

    return (i1 << 1) | i0;
  }

private:
  Real m_minX;
  Real m_minY;
  Real m_maxX;
  Real m_maxY;
  std::size_t m_numItems;
  std::size_t m_numLevels;
  // using std::unique_ptr arrays for uninitialized memory optimization
  std::unique_ptr<std::size_t[]> m_levelBounds;
  std::size_t m_numNodes;
  std::unique_ptr<Real[]> m_boxes;
  std::unique_ptr<std::size_t[]> m_indices;
  std::size_t m_pos;

  static std::size_t computeNumLevels(std::size_t numItems) {
    std::size_t n = numItems;
    std::size_t levelBoundsSize = 1;
    do {
      n = static_cast<std::size_t>(std::ceil(static_cast<float>(n) / NodeSize));
      levelBoundsSize += 1;
    } while (n != 1);

    return levelBoundsSize;
  }

  // quicksort that partially sorts the bounding box data alongside the Hilbert values
  static void sort(std::uint32_t *values, Real *boxes, std::size_t *indices, std::size_t left,
                   std::size_t right) {
    CAVC_ASSERT(left <= right, "left index should never be past right index");

    // check against NodeSize (only need to sort down to NodeSize buckets)
    if (left / NodeSize >= right / NodeSize) {
      return;
    }

    auto pivot = values[(left + right) >> 1];
    auto i = left - 1;
    auto j = right + 1;

    while (true) {
      do
        i++;
      while (values[i] < pivot);
      do
        j--;
      while (values[j] > pivot);
      if (i >= j)
        break;
      swap(values, boxes, indices, i, j);
    }

    sort(values, boxes, indices, left, j);
    sort(values, boxes, indices, j + 1, right);
  }

  static void swap(std::uint32_t *values, Real *boxes, std::size_t *indices, std::size_t i,
                   std::size_t j) {
    auto temp = values[i];
    values[i] = values[j];
    values[j] = temp;

    auto k = 4 * i;
    auto m = 4 * j;

    auto a = boxes[k];
    auto b = boxes[k + 1];
    auto c = boxes[k + 2];
    auto d = boxes[k + 3];
    boxes[k] = boxes[m];
    boxes[k + 1] = boxes[m + 1];
    boxes[k + 2] = boxes[m + 2];
    boxes[k + 3] = boxes[m + 3];
    boxes[m] = a;
    boxes[m + 1] = b;
    boxes[m + 2] = c;
    boxes[m + 3] = d;

    auto e = indices[i];
    indices[i] = indices[j];
    indices[j] = e;
  }
};
} // namespace cavc

#endif // CAVC_STATICSPATIALINDEX_HPP
