#ifndef MEDIAN_FILTER_HPP
#define MEDIAN_FILTER_HPP

// median ignores a single stuck sample

#include <array>
#include <cstddef>

#include "Types.hpp"

namespace tempmon {

template <std::size_t N>
class MedianFilter {
  static_assert(N > 0, "a median of nothing is not defined");

 public:
  // scratch is a member to keep it off the stack
  [[nodiscard]] AdcRaw apply(const AdcRaw* block) noexcept {
    for (std::size_t i = 0; i < N; ++i) scratch_[i] = block[i];

    // insertion sort bounded no recursion
    for (std::size_t i = 1; i < N; ++i) {
      const AdcRaw key = scratch_[i];
      std::size_t j    = i;
      while (j > 0 && scratch_[j - 1] > key) {
        scratch_[j] = scratch_[j - 1];
        --j;
      }
      scratch_[j] = key;
    }

    // even N takes the upper of the two middle values
    return scratch_[N / 2];
  }

  // pinned at an end stop means an open or shorted input
  [[nodiscard]] static constexpr bool atRail(AdcRaw v, AdcRaw rawMax) noexcept { return (v == 0) || (v >= rawMax); }

 private:
  std::array<AdcRaw, N> scratch_ {};
};

}  // namespace tempmon

#endif  // MEDIAN_FILTER_HPP
