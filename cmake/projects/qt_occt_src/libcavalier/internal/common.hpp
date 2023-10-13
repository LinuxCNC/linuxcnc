#ifndef CAVC_INTERNAL_COMMON_HPP
#define CAVC_INTERNAL_COMMON_HPP
#include <cassert>
#include <functional>
#include <utility>

namespace cavc {
namespace internal {
#define CAVC_ASSERT(cond, msg) assert(cond &&msg)

template <typename T> inline void hashCombine(std::size_t &seed, const T &val) {
  // copied from boost hash_combine, it's not the best hash combine but it's very simple
  // https://stackoverflow.com/questions/35985960/c-why-is-boosthash-combine-the-best-way-to-combine-hash-values
  seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct IndexPairHash {
  inline std::size_t operator()(std::pair<std::size_t, std::size_t> const &pair) const {
    std::size_t seed = 0;
    hashCombine(seed, pair.first);
    hashCombine(seed, pair.second);
    return seed;
  }
};
} // namespace internal
} // namespace cavc

#endif // CAVC_INTERNAL_COMMON_HPP
