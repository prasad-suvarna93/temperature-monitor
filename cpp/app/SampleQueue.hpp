#ifndef SAMPLE_QUEUE_HPP
#define SAMPLE_QUEUE_HPP

// single producer single consumer lock-free

#include <array>
#include <atomic>
#include <cstddef>

#include "Types.hpp"

namespace tempmon {

template <std::size_t Depth>
class SampleQueue {
  static_assert((Depth & (Depth - 1)) == 0, "depth must be a power of two for the index masking");
  static_assert(Depth >= 2, "a queue of one cannot decouple anything");

 public:
  // interrupt context bounded and drops on backpressure
  bool push(std::uint8_t blockIndex) noexcept {
    const std::uint32_t head = head_.load(std::memory_order_relaxed);

    // unsigned so it stays correct across the wrap
    if (head - tail_.load(std::memory_order_acquire) >= Depth) {
      overruns_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    slot_[head & (Depth - 1)] = blockIndex;

    // write the slot before publishing the head
    head_.store(head + 1, std::memory_order_release);
    return true;
  }

  bool pop(std::uint8_t& out) noexcept {
    const std::uint32_t tail = tail_.load(std::memory_order_relaxed);

    // pairs with the push barrier
    if (head_.load(std::memory_order_acquire) == tail) return false;

    out = slot_[tail & (Depth - 1)];

    // free the slot only after reading it
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::uint32_t overruns() const noexcept { return overruns_.load(std::memory_order_relaxed); }

 private:
  std::atomic<std::uint32_t> head_ {0};
  std::atomic<std::uint32_t> tail_ {0};
  std::atomic<std::uint32_t> overruns_ {0};
  std::array<std::uint8_t, Depth> slot_ {};

  // fail the build if these are not lock-free
  static_assert(std::atomic<std::uint32_t>::is_always_lock_free, "32-bit atomics must be lock-free on this target");
};

}  // namespace tempmon

#endif  // SAMPLE_QUEUE_HPP
