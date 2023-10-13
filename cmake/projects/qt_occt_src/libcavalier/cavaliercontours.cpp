#include "cavaliercontours.h"
#include "polylinecombine.hpp"
#include "polylineoffset.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

#define CAVC_BEGIN_TRY_CATCH try {

#define CAVC_END_TRY_CATCH                                                                         \
  }                                                                                                \
  catch (std::exception const &ex) {                                                               \
    std::cerr << "Unexpected exception thrown in " << __FUNCTION__ << ": " << ex.what();           \
    std::terminate();                                                                              \
  }

struct cavc_pline {
  cavc::Polyline<cavc_real> data;
  cavc_pline() = default;
  cavc_pline(cavc::Polyline<cavc_real> &&data) noexcept : data(data) {}
};

struct cavc_pline_list {
  std::vector<std::unique_ptr<cavc_pline>> data;
};

// helper to move vector of plines to cavc_pline_list
static void move_to_list(std::vector<cavc::Polyline<cavc_real>> &&plines, cavc_pline_list *list) {
  list->data.reserve(plines.size());

  for (std::size_t i = 0; i < plines.size(); ++i) {
    list->data.push_back(std::make_unique<cavc_pline>(std::move(plines[i])));
  }
}

// helper to copy vertex data to cavc_pline
static void copy_to_pline(cavc_pline *api_pline, cavc_vertex const *vertex_data,
                          uint32_t vertex_count) {
  api_pline->data.vertexes().clear();
  api_pline->data.vertexes().reserve(vertex_count);

  for (uint32_t i = 0; i < vertex_count; ++i) {
    api_pline->data.addVertex(vertex_data[i].x, vertex_data[i].y, vertex_data[i].bulge);
  }
}

// helper to copy to vertex_data from cavc_pline
static void copy_to_vertex_data(cavc_pline const *api_pline, cavc_vertex *vertex_data) {
  auto const &pline = api_pline->data;
  uint32_t vertex_count = static_cast<uint32_t>(pline.size());
  for (uint32_t i = 0; i < vertex_count; ++i) {
    auto const &v = pline[i];
    vertex_data[i] = cavc_vertex{v.x(), v.y(), v.bulge()};
  }
}

cavc_pline *cavc_pline_new(const cavc_vertex *vertex_data, uint32_t vertex_count, int is_closed) {
  CAVC_BEGIN_TRY_CATCH
  cavc_pline *result = new cavc_pline();
  if (vertex_data) {
    copy_to_pline(result, vertex_data, vertex_count);
  } else {
    result->data.vertexes().reserve(vertex_count);
  }
  result->data.isClosed() = is_closed;
  return result;
  CAVC_END_TRY_CATCH
}

void cavc_pline_delete(cavc_pline *polyline) {
  CAVC_BEGIN_TRY_CATCH
  delete polyline;
  CAVC_END_TRY_CATCH
}

uint32_t cavc_pline_capacity(cavc_pline const *pline) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  return static_cast<uint32_t>(pline->data.vertexes().capacity());
  CAVC_END_TRY_CATCH
}

void cavc_pline_set_capacity(cavc_pline *pline, uint32_t size) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  pline->data.vertexes().reserve(size);
  CAVC_END_TRY_CATCH
}

uint32_t cavc_pline_vertex_count(cavc_pline const *pline) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  return static_cast<uint32_t>(pline->data.size());
  CAVC_END_TRY_CATCH
}

void cavc_pline_vertex_data(cavc_pline const *pline, cavc_vertex *vertex_data) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  copy_to_vertex_data(pline, vertex_data);
  CAVC_END_TRY_CATCH
}

int cavc_pline_is_closed(cavc_pline const *pline) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  return pline->data.isClosed();
  CAVC_END_TRY_CATCH
}

void cavc_pline_set_vertex_data(cavc_pline *pline, const cavc_vertex *vertex_data,
                                uint32_t vertex_count) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  copy_to_pline(pline, vertex_data, vertex_count);
  CAVC_END_TRY_CATCH
}

void cavc_pline_add_vertex(cavc_pline *pline, cavc_vertex vertex) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  pline->data.addVertex(vertex.x, vertex.y, vertex.bulge);
  CAVC_END_TRY_CATCH
}

void cavc_pline_remove_range(cavc_pline *pline, uint32_t start_index, uint32_t count) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_ASSERT(start_index < pline->data.size(), "start_index is out of vertexes range");
  CAVC_ASSERT(start_index + count <= pline->data.size(), "count is out of vertexes range");
  CAVC_BEGIN_TRY_CATCH
  auto &vertexes = pline->data.vertexes();
  auto start_it = vertexes.begin() + static_cast<std::ptrdiff_t>(start_index);
  vertexes.erase(start_it, start_it + static_cast<std::ptrdiff_t>(count));
  CAVC_END_TRY_CATCH
}

void cavc_pline_clear(cavc_pline *pline) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  pline->data.vertexes().clear();
  CAVC_END_TRY_CATCH
}

void cavc_pline_set_is_closed(cavc_pline *pline, int is_closed) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  pline->data.isClosed() = is_closed;
  CAVC_END_TRY_CATCH
}

void cavc_pline_list_delete(cavc_pline_list *pline_list) {
  CAVC_BEGIN_TRY_CATCH
  delete pline_list;
  CAVC_END_TRY_CATCH
}

uint32_t cavc_pline_list_count(cavc_pline_list const *pline_list) {
  CAVC_ASSERT(pline_list, "null pline_list not allowed");
  CAVC_BEGIN_TRY_CATCH
  return static_cast<uint32_t>(pline_list->data.size());
  CAVC_END_TRY_CATCH
}

cavc_pline *cavc_pline_list_get(cavc_pline_list const *pline_list, uint32_t index) {
  CAVC_ASSERT(pline_list, "null pline_list not allowed");
  CAVC_ASSERT(index < pline_list->data.size(), "index is out of vertexes range");
  CAVC_BEGIN_TRY_CATCH
  return pline_list->data[index].get();
  CAVC_END_TRY_CATCH
}

cavc_pline *cavc_pline_list_release(cavc_pline_list *pline_list, uint32_t index) {
  CAVC_ASSERT(pline_list, "null pline_list not allowed");
  CAVC_ASSERT(index < pline_list->data.size(), "index is out of vertexes range");
  CAVC_BEGIN_TRY_CATCH
  cavc_pline *target = pline_list->data[index].release();
  pline_list->data.erase(pline_list->data.begin() + static_cast<std::ptrdiff_t>(index));
  return target;
  CAVC_END_TRY_CATCH
}

void cavc_parallel_offset(cavc_pline const *pline, cavc_real delta, cavc_pline_list **output,
                          int option_flags) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_ASSERT(output, "null output not allowed");
  CAVC_BEGIN_TRY_CATCH
  bool mayHaveSelfIntersects = (option_flags & 0x1) != 0;
  auto results = cavc::parallelOffset(pline->data, delta, mayHaveSelfIntersects);
  *output = new cavc_pline_list();
  move_to_list(std::move(results), *output);
  CAVC_END_TRY_CATCH
}

void cavc_combine_plines(cavc_pline const *pline_a, cavc_pline const *pline_b, int combine_mode,
                         cavc_pline_list **remaining, cavc_pline_list **subtracted) {
  CAVC_ASSERT(pline_a, "null pline_a not allowed");
  CAVC_ASSERT(pline_b, "null pline_b not allowed");
  CAVC_ASSERT(combine_mode >= 0 && combine_mode <= 3, "combine_mode must be 0, 1, 2, or 3");
  CAVC_BEGIN_TRY_CATCH
  cavc::PlineCombineMode mode;
  switch (combine_mode) {
  case 0:
    mode = cavc::PlineCombineMode::Union;
    break;
  case 1:
    mode = cavc::PlineCombineMode::Exclude;
    break;
  case 2:
    mode = cavc::PlineCombineMode::Intersect;
    break;
  case 3:
    mode = cavc::PlineCombineMode::XOR;
    break;
  default:
    mode = cavc::PlineCombineMode::Union;
    break;
  }
  auto results = cavc::combinePolylines(pline_a->data, pline_b->data, mode);

  *remaining = new cavc_pline_list();
  *subtracted = new cavc_pline_list();
  move_to_list(std::move(results.remaining), *remaining);
  move_to_list(std::move(results.subtracted), *subtracted);
  CAVC_END_TRY_CATCH
}

cavc_real cavc_get_path_length(cavc_pline const *pline) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  return cavc::getPathLength(pline->data);
  CAVC_END_TRY_CATCH
}

cavc_real cavc_get_area(cavc_pline const *pline) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  return cavc::getArea(pline->data);
  CAVC_END_TRY_CATCH
}

int cavc_get_winding_number(cavc_pline const *pline, cavc_point point) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  return cavc::getWindingNumber(pline->data, cavc::Vector2<cavc_real>(point.x, point.y));
  CAVC_END_TRY_CATCH
}

void cavc_get_extents(cavc_pline const *pline, cavc_real *min_x, cavc_real *min_y, cavc_real *max_x,
                      cavc_real *max_y) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  auto result = cavc::getExtents(pline->data);
  *min_x = result.xMin;
  *min_y = result.yMin;
  *max_x = result.xMax;
  *max_y = result.yMax;
  CAVC_END_TRY_CATCH
}

void cavc_get_closest_point(cavc_pline const *pline, cavc_point input_point,
                            uint32_t *closest_start_index, cavc_point *closest_point,
                            cavc_real *distance) {
  CAVC_ASSERT(pline, "null pline not allowed");
  CAVC_ASSERT(pline->data.size() != 0, "empty pline not allowed");
  CAVC_BEGIN_TRY_CATCH
  cavc::ClosestPoint<cavc_real> closestPoint(
      pline->data, cavc::Vector2<cavc_real>(input_point.x, input_point.y));
  *closest_start_index = static_cast<uint32_t>(closestPoint.index());
  *closest_point = cavc_point{closestPoint.point().x(), closestPoint.point().y()};
  *distance = closestPoint.distance();
  CAVC_END_TRY_CATCH
}
