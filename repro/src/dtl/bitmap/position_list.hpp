#pragma once
//===----------------------------------------------------------------------===//
#include <dtl/dtl.hpp>
#include <dtl/math.hpp>

#include <boost/dynamic_bitset.hpp>

#include <cstddef>
#include <string>
#include <vector>
//===----------------------------------------------------------------------===//
namespace dtl {
//===----------------------------------------------------------------------===//
/// Position list.
template<typename _position_t = $u32>
struct position_list {
  using position_t = _position_t;

  /// The ordered list of positions.
  std::vector<position_t> positions_;

  /// The universe [0, n_).
  $u64 n_;

  // TODO make private
  position_list() = default;

  explicit position_list(const boost::dynamic_bitset<$u32>& in)
      : positions_(), n_(in.size()) {
    std::size_t current_pos = in.find_first();
    while (current_pos < in.size()) {
      positions_.push_back(static_cast<position_t>(current_pos));
      current_pos = in.find_next(current_pos);
    }
  }

  ~position_list() = default;
  position_list(const position_list& other) = default;
  position_list(position_list&& other) noexcept = default;

  __forceinline__ position_list&
  operator=(const position_list& other) = default;

  __forceinline__ position_list&
  operator=(position_list&& other) noexcept = default;

  /// Return the size in bytes.
  std::size_t
  size_in_bytes() const {
    return positions_.size() * sizeof(position_t) /* positions */
        + sizeof(position_t) /* number of positions */
        + sizeof(n_) /* bit-length of the original bitmap */;
  }

  /// Return the length of the bitmap.
  std::size_t
  size() const {
    return n_;
  }

  /// Conversion to an boost::dynamic_bitset.
  boost::dynamic_bitset<$u32>
  to_bitset() {
    boost::dynamic_bitset<$u32> ret(n_);
    for (position_t p : positions_) {
      ret[p] = true;
    }
    return ret;
  }

  /// Bitwise AND
  position_list __forceinline__
  operator&(const position_list& other) const {
    position_list ret;
    ret.n_ = n_;
    auto this_it = positions_.begin();
    const auto this_it_end = positions_.end();
    auto other_it = other.positions_.begin();
    const auto other_it_end = other.positions_.end();
    while (!(this_it == this_it_end || other_it == other_it_end)) {
      if (*this_it == *other_it) {
        ret.positions_.push_back(*this_it);
        ++this_it;
        ++other_it;
      }
      else {
        if (*this_it < *other_it) {
          ++this_it;
        }
        else {
          ++other_it;
        }
      }
    }
    return ret;
  }

  /// Bitwise AND (range encoding)
  position_list __forceinline__
  and_re(const position_list& other) const {
    return *this & other; // nothing special here. fall back to AND
  }

  /// Bitwise XOR
  position_list __forceinline__
  operator^(const position_list& other) const {
    position_list ret;
    ret.n_ = n_;

    auto this_it = positions_.begin();
    const auto this_it_end = positions_.end();
    auto other_it = other.positions_.begin();
    const auto other_it_end = other.positions_.end();
    while (!(this_it == this_it_end || other_it == other_it_end)) {
      if (*this_it < *other_it) {
        ret.positions_.push_back(*this_it);
        ++this_it;
      }
      else if (*other_it < *this_it) {
        ret.positions_.push_back(*other_it);
        ++other_it;
      }
      else {
        ++this_it;
        ++other_it;
      }
    }
    if (this_it != this_it_end) {
      while (this_it != this_it_end) {
        ret.positions_.push_back(*this_it);
        ++this_it;
      }
    }
    if (other_it != other_it_end) {
      while (other_it != other_it_end) {
        ret.positions_.push_back(*other_it);
        ++other_it;
      }
    }
    return ret;
  }

  /// Bitwise XOR (range encoding)
  position_list __forceinline__
  xor_re(const position_list& other) const {
    return *this ^ other; // nothing special here. fall back to XOR
  }

  /// Computes (a XOR b) & this
  /// Note: this, a and b must be different instances. Otherwise, the behavior
  /// is undefined.
  __forceinline__ position_list&
  fused_xor_and(const position_list& a, const position_list& b) {
    const auto x = a ^ b;
    auto y = *this & x;
    std::swap(positions_, y.positions_);
    return *this;
  }

  static std::string
  name() {
    return "position_list_"
        + std::to_string(sizeof(position_t) * 8);
    ;
  }

  /// Returns the value of the bit at the position pos.
  u1
  test(const std::size_t pos) const {
    auto search = std::lower_bound(positions_.begin(), positions_.end(), pos);
    if (search == positions_.end()) {
      return false;
    }
    return *search == pos;
  }

  /// Set the i-th bit to the given value.
  void __forceinline__
  set(std::size_t i, u1 val) noexcept {
    auto search = std::lower_bound(positions_.begin(), positions_.end(), i);
    if (search == positions_.end()) {
      if (val) {
        // Append the value to the list.
        positions_.push_back(i);
      }
      return;
    }

    auto found_pos = *search;
    if (found_pos == i) {
      if (!val) {
        // Remove the value from the list.
        positions_.erase(search);
      }
      return;
    }
    else {
      if (val) {
        positions_.insert(search, i);
      }
    }
  }

  //===--------------------------------------------------------------------===//
  /// Iterator, with skip support.
  class iter {
    const position_list& outer_;

    //===------------------------------------------------------------------===//
    // Iterator state
    //===------------------------------------------------------------------===//
    /// Read position within the list.
    $u64 read_pos_;
    /// Points to the beginning of the current range.
    $u64 range_begin_;
    /// The length of the current range.
    $u64 range_length_;
    //===------------------------------------------------------------------===//

  public:
    explicit __forceinline__
    iter(const position_list& outer)
        : outer_(outer),
          read_pos_(outer.positions_.empty() ? outer_.positions_.size() : 0),
          range_begin_(read_pos_ < outer.positions_.size()
                  ? outer_.positions_[0]
                  : outer_.n_),
          range_length_(read_pos_ < outer.positions_.size() ? 1 : 0) {
      // Determine the length of the current range.
      if (read_pos_ == 0) {
        ++read_pos_;
      }
      while (read_pos_ < outer_.positions_.size()
          && outer_.positions_[read_pos_] == range_begin_ + range_length_) {
        ++read_pos_;
        ++range_length_;
      }
    }

    void __forceinline__
    next() {
      if (read_pos_ < outer_.positions_.size()) {
        range_begin_ = outer_.positions_[read_pos_];
        range_length_ = 1;
        ++read_pos_;
        while (read_pos_ < outer_.positions_.size()
            && outer_.positions_[read_pos_] == range_begin_ + range_length_) {
          ++read_pos_;
          ++range_length_;
        }
      }
      else {
        range_begin_ = outer_.n_;
        range_length_ = 0;
      }
    }

    void __forceinline__
    skip_to(const std::size_t to_pos) {
      if (to_pos < (range_begin_ + range_length_)) {
        range_length_ -= to_pos - range_begin_;
        range_begin_ = to_pos;
        return;
      }
      auto search = std::lower_bound(
          outer_.positions_.begin(), outer_.positions_.end(), to_pos);
      if (search != outer_.positions_.end()) {
        range_begin_ = *search;
        read_pos_ = std::distance(outer_.positions_.begin(), search) + 1ull;
        range_length_ = 1;
        while (read_pos_ < outer_.positions_.size()
            && outer_.positions_[read_pos_] == range_begin_ + range_length_) {
          ++read_pos_;
          ++range_length_;
        }
      }
      else {
        range_begin_ = outer_.n_;
        range_length_ = 0;
      }
    }

    u1 __forceinline__
    end() const noexcept {
      return range_length_ == 0;
    }

    u64 __forceinline__
    pos() const noexcept {
      return range_begin_;
    }

    u64 __forceinline__
    length() const noexcept {
      return range_length_;
    }
  };
  //===--------------------------------------------------------------------===//

  using skip_iter_type = iter;
  using scan_iter_type = iter;

  skip_iter_type __forceinline__
  it() const {
    return skip_iter_type(*this);
  }

  scan_iter_type __forceinline__
  scan_it() const {
    return scan_iter_type(*this);
  }

  /// Returns the name of the instance including the most important parameters
  /// in JSON.
  std::string
  info() const {
    return "{\"name\":\"" + name() + "\""
        + ",\"n\":" + std::to_string(n_)
        + ",\"size\":" + std::to_string(size_in_bytes())
        + ",\"positions\":" + std::to_string(positions_.size())
        + "}";
  }

  // For debugging purposes.
  void
  print(std::ostream& os) const {
    os << "[";
    if (!positions_.empty()) {
      os << positions_[0];
      for (std::size_t i = 1; i < positions_.size(); ++i) {
        os << "," << positions_[i];
      }
    }
    os << "]";
  }
};
//===----------------------------------------------------------------------===//
} // namespace dtl
