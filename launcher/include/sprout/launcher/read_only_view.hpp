#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace sprout::launcher {

template <typename T>
class ReadOnlyView {
 public:
  constexpr ReadOnlyView() noexcept = default;

  constexpr ReadOnlyView(const T* data, std::size_t size) noexcept
      : data_(data), size_(size) {}

  template <std::size_t Size>
  constexpr ReadOnlyView(const std::array<T, Size>& values) noexcept
      : data_(values.data()), size_(values.size()) {}

  template <typename Allocator>
  ReadOnlyView(const std::vector<T, Allocator>& values) noexcept
      : data_(values.data()), size_(values.size()) {}

  [[nodiscard]] constexpr const T* begin() const noexcept { return data_; }
  [[nodiscard]] constexpr const T* end() const noexcept {
    return size_ == 0 ? data_ : data_ + size_;
  }
  [[nodiscard]] constexpr const T* data() const noexcept { return data_; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept {
    return data_[index];
  }

 private:
  const T* data_{nullptr};
  std::size_t size_{0};
};

}  // namespace sprout::launcher
