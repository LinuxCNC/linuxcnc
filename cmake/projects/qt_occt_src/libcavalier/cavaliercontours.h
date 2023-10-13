#ifndef CAVALIERCONTOURS_HPP
#define CAVALIERCONTOURS_HPP

#include <stdint.h>

#ifdef CAVC_STATIC_LIB
#define CAVC_API
#else
#if defined _WIN32 || defined __CYGWIN__
#ifdef CAVC_EXPORTS
#ifdef __GNUC__
#define CAVC_API __attribute__((dllexport))
#else
#define CAVC_API __declspec(dllexport)
#endif
#else
#ifdef __GNUC__
#define CAVC_API __attribute__((dllimport))
#else
#define CAVC_API __declspec(dllimport)
#endif
#endif
#else
#if __GNUC__ >= 4
#define CAVC_API __attribute__((visibility("default")))
#else
#define CAVC_API
#endif
#endif
#endif

#ifndef CAVC_REAL
#define CAVC_REAL double
#endif

typedef CAVC_REAL cavc_real;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cavc_pline cavc_pline;

typedef struct cavc_pline_list cavc_pline_list;

typedef struct cavc_vertex {
  cavc_real x;
  cavc_real y;
  cavc_real bulge;
} cavc_vertex;

typedef struct cavc_point {
  cavc_real x;
  cavc_real y;
} cavc_point;

// Functions for working with cavc_pline

// Create/alloc a new cavc_pline initialized with data given. vertex_data may be null (vertex_count
// will still be used to set capacity), if is_closed is 0 then polyline will be open, otherwise it
// will be closed.
CAVC_API cavc_pline *cavc_pline_new(cavc_vertex const *vertex_data, uint32_t vertex_count,
                                    int is_closed);

// Delete/free a cavc_pline that was created with cavc_pline_new or released from a cavc_pline_list.
// NOTE: Do not call this function on cavc_pline's that are owned by a cavc_pline_list!
CAVC_API void cavc_pline_delete(cavc_pline *pline);

// Returns the current capacity of pline.
CAVC_API uint32_t cavc_pline_capacity(cavc_pline const *pline);

// Reserves memory for size vertexes to be stored in the pline if size is greater than current
// capacity, otherwise does nothing.
CAVC_API void cavc_pline_set_capacity(cavc_pline *pline, uint32_t size);

// Returns the current vertex count of a cavc_pline.
CAVC_API uint32_t cavc_pline_vertex_count(cavc_pline const *pline);

// Returns the vertex data of a cavc_pline by filling vertex_data as an array. NOTE: vertex_data
// must be large enough to hold the total vertex count! Call cavc_pline_vertex_count first to
// determine size required.
CAVC_API void cavc_pline_vertex_data(cavc_pline const *pline, cavc_vertex *vertex_data);

// Returns whether the cavc_pline is closed or not.
CAVC_API int cavc_pline_is_closed(cavc_pline const *pline);

// Sets the vertex data of a cavc_pline. Must pass in vertex_count to indicate how many vertexes to
// copy from the vertex_data array.
CAVC_API void cavc_pline_set_vertex_data(cavc_pline *pline, cavc_vertex const *vertex_data,
                                         uint32_t vertex_count);

// Adds a vertex to the cavc_pline.
CAVC_API void cavc_pline_add_vertex(cavc_pline *pline, cavc_vertex vertex);

// Removes a range of vertexes from the cavc_pline, starting at start_index and removing count
// number of vertexes. No bounds checking is performed (ensure start_index + count <=
// pline_vertex_count).
CAVC_API void cavc_pline_remove_range(cavc_pline *pline, uint32_t start_index, uint32_t count);

// Clears the cavc_pline (capacity is left unchanged).
CAVC_API void cavc_pline_clear(cavc_pline *pline);

// Sets the polyline to be closed or open, if is_closed is 0 then polyline will be open, otherwise
// it will be closed.
CAVC_API void cavc_pline_set_is_closed(cavc_pline *pline, int is_closed);

// Functions for working with cavc_pline_list

// Delete/free a cavc_pline_list, this will also delete all elements in the list.
CAVC_API void cavc_pline_list_delete(cavc_pline_list *pline_list);

// Get the element count of the cavc_pline_list.
CAVC_API uint32_t cavc_pline_list_count(cavc_pline_list const *pline_list);

// Get a cavc_pline from the cavc_pline_list. No bounds checking is performed (ensure index <
// cavc_pline_list count). NOTE: The cavc_pline is still owned by the list!
CAVC_API cavc_pline *cavc_pline_list_get(cavc_pline_list const *pline_list, uint32_t index);

// Release a cavc_pline from the cavc_pline_list's ownership, returning the cavc_pline that was
// released and removing it from the list. NOTE: cavc_pline_delete must now be called on the
// released cavc_pline!
CAVC_API cavc_pline *cavc_pline_list_release(cavc_pline_list *pline_list, uint32_t index);

// Algorithm functions

// Generates the parallel offset of a polyline. delta is the offset delta, output is filled with the
// result, option_flags are bit flags that allow for indicating information about the polyline or
// forcing certain behaviors in the offset generation, more flags may be added in future versions.
// If no flags are set then the polyline is assumed to have no self intersects.
// 0x1 = Indicates the polyline may have self intersects.
CAVC_API void cavc_parallel_offset(cavc_pline const *pline, cavc_real delta,
                                   cavc_pline_list **output, int option_flags);

// Combines two non-self intersecting closed polylines, pline_a and pline_b.
// For union combine_mode = 0
// For exclude combine_mode = 1
// For intersect combine_mode = 2
// For XOR combine_mode = 3
// If combine_mode is any other value then no output parameters are filled.
// remaining is filled with the closed polylines that remain after combining, subtracted is filled
// with closed polylines that represent subtracted space (in the case of islands after a union or
// exclude).
// If pline_a or pline_b is an open polyline or has self intersects then the result is undefined.
CAVC_API void cavc_combine_plines(cavc_pline const *pline_a, cavc_pline const *pline_b,
                                  int combine_mode, cavc_pline_list **remaining,
                                  cavc_pline_list **subtracted);

// Returns the path length of the cavc_pline given. If pline vertex count is less than 2 then 0 is
// returned.
CAVC_API cavc_real cavc_get_path_length(cavc_pline const *pline);

// Returns the signed area of the cavc_pline given. If pline is open or vertex count is less than 2
// then 0 is returned. If pline goes clockwise then a negative signed area is returned, otherwise a
// positive signed area is returned.
CAVC_API cavc_real cavc_get_area(cavc_pline const *pline);

// Compute the winding number of the 2d point relative to the pline. If pline is open or vertex
// count is less than 2 then 0 is returned. For more on winding number see:
// https://en.wikipedia.org/wiki/Winding_number
CAVC_API int cavc_get_winding_number(cavc_pline const *pline, cavc_point point);

// Compute the axis aligned extents of the pline, results are written to min_x, min_y, max_x, and
// max_y output parameters. If pline is empty then min_x and min_y are filled with positive infinity
// and max_x and max_y are filled with negative infinity.
CAVC_API void cavc_get_extents(cavc_pline const *pline, cavc_real *min_x, cavc_real *min_y,
                               cavc_real *max_x, cavc_real *max_y);

// Finds the closest point on the polyline to the point given. closest_start_index is filled with
// the starting vertex index of the segment that the closest point is on. closest_point is filled
// with the closest point on the polyline. distance is filled with the distance from the point given
// to the closest point. pline must not be empty.
CAVC_API void cavc_get_closest_point(cavc_pline const *pline, cavc_point input_point,
                                     uint32_t *closest_start_index, cavc_point *closest_point,
                                     cavc_real *distance);

#ifdef __cplusplus
}
#endif

#endif // CAVALIERCONTOURS_HPP
