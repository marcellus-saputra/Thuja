#pragma once
//===----------------------------------------------------------------------===//
#include <dtl/bitmap/util/plain_bitmap.hpp>
#include <dtl/bitmap/util/plain_bitmap_iter.hpp>
#include <dtl/dtl.hpp>

#include <boost/dynamic_bitset.hpp>

#include <cstddef>
#include <type_traits>
#include <vector>
//===----------------------------------------------------------------------===//
namespace dtl {
//===----------------------------------------------------------------------===//
/// X-Aligned Hybrid: An RLE compressed representation of a bitmap of length N.
template<typename _word_type = u32>
class xah {

protected:
  using word_type = typename std::remove_cv<_word_type>::type;
  static_assert(std::is_integral<word_type>::value,
      "The word type must be an integral type.");
  static_assert(!std::is_same<word_type, bool>::value,
      "The word type must not be a boolean.");
  
  static constexpr std::size_t word_bitlength = sizeof(word_type) * 8;
  static constexpr std::size_t max_fill_repetitions =
      (1ull << (word_bitlength - 2)) - 1;
  /// The number of bits that can be stored in a literal word.
  static constexpr std::size_t payload_bit_cnt = word_bitlength - 1;
  static constexpr word_type all_ones = word_type(~word_type(0));

  /// The encoded bitmap.
  std::vector<word_type> data_;
  /// The length of the encoded bitmap.
  std::size_t encoded_bitmap_length_ = 0;

  std::size_t remaining_bit_cnt_in_last_literal_word_ = 0;

  /// Returns true if the given word is a literal word.
  static inline constexpr u1
  is_literal_word(const word_type w) noexcept {
    return (w & word_type(1)) == 0;
  }

  /// Returns true if the given word is a fill word.
  static inline constexpr u1
  is_fill_word(const word_type w) noexcept {
    return (w & word_type(1)) == 1;
  }

  /// Extract the fill value from a fill word.
  static inline constexpr u1
  extract_fill_value(const word_type w) noexcept {
    assert(is_fill_word(w));
    return (w & word_type(2)) != 0;
  }

  /// Extract the fill length from a fill word.
  static inline constexpr std::size_t
  extract_fill_repetitions(const word_type w) noexcept {
    assert(is_fill_word(w));
    return w >> 2;
  }

  /// Creates a fill word.
  static inline constexpr word_type
  make_fill_word(u1 val, const std::size_t len) noexcept {
    assert(len <= max_fill_repetitions);
    word_type w = val ? word_type(3) : word_type(1);
    w |= len << 2;
    return w;
  }

  /// Append a 0- or 1-run of the given length to the compressed bitmap.
  inline void
  append_run(u1 val, std::size_t len) {
    assert(len > 0);
    if (data_.size() == 0) {
      if (len >= payload_bit_cnt) {
        auto rep = std::min(std::size_t(max_fill_repetitions), len / payload_bit_cnt);
        data_.emplace_back(make_fill_word(val, rep));
        encoded_bitmap_length_ += rep * payload_bit_cnt;
        remaining_bit_cnt_in_last_literal_word_ = 0;
        len -= rep * payload_bit_cnt;
      }
      else {
        word_type lit = val == true
            ? (word_type(~0ull) >> (word_bitlength - len)) << 1
            : 0;
        data_.emplace_back(lit);
        encoded_bitmap_length_ += len;
        remaining_bit_cnt_in_last_literal_word_ = payload_bit_cnt - len;
        len = 0;
      }
      if (len == 0) return; // Done
    }
    assert(!data_.empty());

    auto& w = data_.back();
    if (len >= payload_bit_cnt
        && is_fill_word(w)) {
      const auto last_fill_rep = extract_fill_repetitions(w);
      const auto last_fill_val = extract_fill_value(w);
      if (last_fill_val == val) {
        //===--------------------------------------------------------------===//
        // The run that is to be appended is of a SAME KIND as the
        // previous one.
        //===--------------------------------------------------------------===//

        // Extend the previous fill word.
        auto extend_by = std::min(max_fill_repetitions - last_fill_rep,
            len / payload_bit_cnt); // a multiple of the payload size
        w = make_fill_word(val, last_fill_rep + extend_by);
        encoded_bitmap_length_ += extend_by * payload_bit_cnt;
        if (extend_by * payload_bit_cnt == len) {
          return; // Done.
        }
        len -= extend_by * payload_bit_cnt;
      }
    }
    if (is_literal_word(w)) {
      // Append to the last literal word.
      if (remaining_bit_cnt_in_last_literal_word_ > 0) {
        auto extend_by =
            std::min(remaining_bit_cnt_in_last_literal_word_, len); // # of bits
        if (val == true) {
          auto bits_to_append = all_ones >> (word_bitlength - extend_by);
          w |= bits_to_append
              << (word_bitlength - remaining_bit_cnt_in_last_literal_word_);
        }
        remaining_bit_cnt_in_last_literal_word_ -= extend_by;
        encoded_bitmap_length_ += extend_by;

        // Check whether we can turn the last word into a fill word.
        if (remaining_bit_cnt_in_last_literal_word_ == 0) {
          if (w == (all_ones << 1)) {
            w = make_fill_word(true, payload_bit_cnt);
          }
          else if (w == 0) {
            w = make_fill_word(false, payload_bit_cnt);
          }
        }

        if (extend_by == len) {
          return; // Done.
        }
        len -= extend_by;
        // Could not fit the entire run into the last literal word.
      }
    }

    // Append remaining bits of the current run.
    while (len > 0) {
      if (len >= payload_bit_cnt) {
        // Append a fill word.
        auto rep =
            std::min(std::size_t(max_fill_repetitions), len / payload_bit_cnt);
        data_.emplace_back(make_fill_word(val, rep));
        encoded_bitmap_length_ += rep * payload_bit_cnt;
        len -= rep * payload_bit_cnt;
      }
      else {
        // Append a literal word.
        if (val == true) {
          data_.emplace_back((all_ones >> (word_bitlength - len)) << 1);
        }
        else {
          data_.emplace_back(0);
        }
        encoded_bitmap_length_ += len;
        remaining_bit_cnt_in_last_literal_word_ = payload_bit_cnt - len;
        len = 0;
      }
    }
  }

  /// Append a 0-run of the given length to the compressed bitmap.
  inline void
  append_zero_run(std::size_t len) {
    append_run(false, len);
  }

  /// Append a 1-run of the given length to the compressed bitmap.
  inline void
  append_one_run(const std::size_t len) {
    append_run(true, len);
  }

public:
  xah() = default;

  explicit xah(const boost::dynamic_bitset<$u32>& in) {
    // Obtain a 1-run iterator for the input bitmap.
    dtl::plain_bitmap_iter<boost::dynamic_bitset<$u32>> it(in);
    // Append runs to the compressed bitmap.
    std::size_t i = 0;
    if (it.pos() > 0) {
      // The bitmap starts with a 0-run.
      auto len = it.pos() - i;
      append_zero_run(len);
      i += len;
    }
    while (!it.end()) {
      auto len = it.length();
      append_one_run(len);
      i += len;
      it.next();
      if (!it.end()) {
        len = it.pos() - i;
        append_zero_run(len);
        i += len;
      }
    }
    if (i < in.size()) {
      append_zero_run(in.size() - i);
    }
    // Try to reduce the memory consumption.
    shrink();
  }

  ~xah() = default;
  xah(const xah& other) = default;
  xah(xah&& other) noexcept = default;
  xah& operator=(const xah& other) = default;
  xah& operator=(xah&& other) noexcept = default;

  /// Return the size in bytes.
  std::size_t __forceinline__
  size_in_bytes() const {
    return data_.size() * sizeof(word_type) /* size of the compressed bitmap */
        + sizeof(encoded_bitmap_length_); /* bit-length of the original bitmap */
  }

  /// Returns the size of the bitmap.
  std::size_t __forceinline__
  size() const {
    return encoded_bitmap_length_;
  }

  /// Conversion to an plain bitmap. // TODO remove
  dtl::plain_bitmap<$u64>
  to_plain_bitmap() {
    dtl::plain_bitmap<$u64> ret(encoded_bitmap_length_, false);
    std::size_t i = 0;
    for (std::size_t word_idx = 0; word_idx < data_.size(); ++word_idx) {
      auto& w = data_[word_idx];
      if (is_fill_word(w)) {
        auto val = extract_fill_value(w);
        auto len = extract_fill_repetitions(w) * payload_bit_cnt;
        ret.set(i, i + len, val);
        i += len;
      }
      else {
        const std::size_t cnt = word_idx == data_.size() - 1
            ? payload_bit_cnt - remaining_bit_cnt_in_last_literal_word_
            : payload_bit_cnt;
        for (std::size_t k = 0; k < cnt; ++k) {
          ret.set(i + k, dtl::bits::bit_test(w, k + 1));
        }
        i += cnt;
      }
    }
    if (i < encoded_bitmap_length_) {
      ret.clear(i, encoded_bitmap_length_);
    }
    return std::move(ret);
  }

  static std::string
  name() {
    return "xah" + std::to_string(word_bitlength);
  }

  /// Returns the value of the bit at the position pos.
  u1 __forceinline__
  test(const std::size_t pos) const {
    std::size_t i = 0;
    std::size_t word_idx = 0;
    // Find the corresponding word.
    for (; word_idx < data_.size(); ++word_idx) {
      auto& w = data_[word_idx];
      if (is_literal_word(w)) {
        if (pos >= i + payload_bit_cnt) {
          i += payload_bit_cnt;
          continue;
        }
        else {
          return dtl::bits::bit_test(w, pos - i + 1); // TODO optimize
        }
      }
      else {
        auto fill_len = extract_fill_repetitions(w) * payload_bit_cnt;
        if (pos >= i + fill_len) {
          i += fill_len;
          continue;
        }
        else {
          return extract_fill_value(w);
        }
      }
    }
    return false;
  }

  /// Try to reduce the memory consumption. This function is supposed to be
  /// called after the bitmap has been modified.
  __forceinline__ void
  shrink() {
    data_.shrink_to_fit();
  }

  //===--------------------------------------------------------------------===//
  /// 1-run iterator
  class iter {
    const xah& outer_;

    std::size_t word_idx_;
    std::size_t in_word_idx_;

    //===------------------------------------------------------------------===//
    // Iterator state
    //===------------------------------------------------------------------===//
    /// Points to the beginning of a 1-run.
    $u64 pos_;
    /// The length of the current 1-run.
    $u64 length_;
    //===------------------------------------------------------------------===//

  public:
    explicit __forceinline__
    iter(const xah& outer)
        : outer_(outer),
          word_idx_(0),
          in_word_idx_(0),
          pos_(0), length_(0) {
      const auto word_cnt = outer_.data_.size();
      word_type w;
      while (word_idx_ < word_cnt) {
        w = outer_.data_[word_idx_];
        if (is_fill_word(w)) {
          if (extract_fill_value(w) == false) {
            pos_ += extract_fill_repetitions(w) * payload_bit_cnt;
          }
          else {
            length_ = extract_fill_repetitions(w) * payload_bit_cnt;
            in_word_idx_ = 0;
            break;
          }
        }
        else {
          const word_type payload = w >> 1;
          if (payload == 0) {
            pos_ += payload_bit_cnt;
          }
          else {
            const std::size_t b = dtl::bits::tz_count(payload);
            std::size_t e = b + 1;
            for (; e < payload_bit_cnt; ++e) {
              u1 is_set = dtl::bits::bit_test(payload, e);
              if (!is_set) break;
            }
            pos_ += b;
            length_ = e - b;
            in_word_idx_ = e;
            break;
          }
        }
        ++word_idx_;
      }
      if (word_idx_ == word_cnt) {
        pos_ = outer_.encoded_bitmap_length_;
        length_ = 0;
      }
      else {
        if (is_fill_word(w)) {
          ++word_idx_;
          in_word_idx_ = 0;
        }
      }
    }

    /// Forward the iterator to the next 1-run.
    void __forceinline__
    next() {
      pos_ += length_;
      length_ = 0;
      const auto word_cnt = outer_.data_.size();
      word_type w;
      while (word_idx_ < word_cnt) {
        w = outer_.data_[word_idx_];
        if (is_fill_word(w)) {
          if (extract_fill_value(w) == false) {
            pos_ += extract_fill_repetitions(w) * payload_bit_cnt;
          }
          else {
            length_ = extract_fill_repetitions(w) * payload_bit_cnt;
            break;
          }
        }
        else {
          if (in_word_idx_ < payload_bit_cnt) { // TODO decode the entire literal word at once.
            const word_type payload = w >> (1 + in_word_idx_);
            if (payload == 0) {
              pos_ += payload_bit_cnt - in_word_idx_;
            }
            else {
              const std::size_t b = dtl::bits::tz_count(payload);
              std::size_t e = b + 1;
              for (; e < (payload_bit_cnt - in_word_idx_); ++e) {
                u1 is_set = dtl::bits::bit_test(payload, e);
                if (!is_set) break;
              }
              pos_ += b;
              length_ = e - b;
              in_word_idx_ += e;
              break;
            }
          }
        }
        ++word_idx_;
        in_word_idx_ = 0;
      }
      if (word_idx_ == word_cnt) {
        pos_ = outer_.encoded_bitmap_length_;
        length_ = 0;
      }
      else {
        if (is_fill_word(w)) {
          ++word_idx_;
          in_word_idx_ = 0;
        }
      }
    }

    /// Forward the iterator to the desired position.
    void __forceinline__
    skip_to(const std::size_t to_pos) {
      assert(pos_ <= to_pos);
      if (to_pos >= outer_.encoded_bitmap_length_) {
        pos_ = outer_.encoded_bitmap_length_;
        length_ = 0;
        return;
      }
      // Call next until the desired position has been reached.
      while (!end() && pos() + length() <= to_pos) {
        next();
      }
      // Adjust the current position and run length.
      if (!end() && pos() < to_pos) {
        length_ -= to_pos - pos_;
        pos_ = to_pos;
      }
    }

    u1 __forceinline__
    end() const noexcept {
      return length_ == 0;
    }

    u64 __forceinline__
    pos() const noexcept {
      return pos_;
    }

    u64 __forceinline__
    length() const noexcept {
      return length_;
    }
  };
  //===--------------------------------------------------------------------===//

  using skip_iter_type = iter;
  using scan_iter_type = iter;

  /// Returns a 1-run iterator.
  skip_iter_type __forceinline__
  it() const {
    return skip_iter_type(*this);
  }

  /// Returns a 1-run iterator.
  scan_iter_type __forceinline__
  scan_it() const {
    return scan_iter_type(*this);
  }

  /// Returns the name of the instance including the most important parameters
  /// in JSON.
  std::string
  info() const {
    return "{\"name\":\"" + name() + "\""
        + ",\"n\":" + std::to_string(encoded_bitmap_length_)
        + ",\"size\":" + std::to_string(size_in_bytes())
        + ",\"word_size\":" + std::to_string(sizeof(word_type))
        + "}";
  }

  // For debugging purposes.
  void
  print(std::ostream& os) const {
    os << "word idx | word type | content" << std::endl;
    for (std::size_t i = 0; i < data_.size(); ++i) {
      os << std::setw(8) << i << " | ";
      auto& w = data_[i];
      if (is_fill_word(w)) {
        os << "fill      | ";
        os << extract_fill_repetitions(w) << " x ";
        os << (extract_fill_value(w) ? "'1'" : "'0'");
        os << std::endl;
      }
      else {
        os << "literal   | '";
        for (std::size_t k = 1; k < word_bitlength; ++k) {
          os << (dtl::bits::bit_test(w, k) ? "1" : "0");
        }
        os << "'";
        os << std::endl;
      }
    }
  }
};
//===----------------------------------------------------------------------===//
/// XAH compressed representation of a bitmap of length N using 8-bit words.
using xah8 = xah<u8>;
/// XAH compressed representation of a bitmap of length N using 16-bit words.
using xah16 = xah<u16>;
/// XAH compressed representation of a bitmap of length N using 32-bit words.
using xah32 = xah<u32>;
/// XAH compressed representation of a bitmap of length N using 64-bit words.
using xah64 = xah<u64>;
//===----------------------------------------------------------------------===//
} // namespace dtl
