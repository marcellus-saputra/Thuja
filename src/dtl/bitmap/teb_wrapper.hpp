#pragma once
//===----------------------------------------------------------------------===//
#include "teb_builder.hpp"
#include "teb_flat.hpp"
#include "teb_iter.hpp"
#include "teb_scan_iter.hpp"
#include "teb_types.hpp"

#include <dtl/dtl.hpp>

#include <boost/dynamic_bitset.hpp>

#include <memory>
#include <string>
#include <vector>
//===----------------------------------------------------------------------===//
namespace dtl {
//===----------------------------------------------------------------------===//
class teb_wrapper {
public: // TODO remove
  /// The serialized TEB.
  std::vector<teb_word_type> data_; // TODO use dtl::buffer
  /// The TEB logic.
  std::unique_ptr<teb_flat> teb_;
  /// For testing: whether this TEB uses in-place pruning or reconstruction
  bool reconstruct = true;

public:
  /// C'tor
  explicit teb_wrapper(const boost::dynamic_bitset<$u32>& bitmap)
      : data_(0), teb_(nullptr) {
    dtl::teb_builder builder(bitmap);
    const auto word_cnt = builder.serialized_size_in_words();
    data_.resize(word_cnt);
    builder.serialize(data_.data());
    teb_ = std::make_unique<teb_flat>(data_.data());
  }

  /// C'tor
  explicit teb_wrapper(const bitmap_tree<>&& bitmap_tree, f64 fpr = 0.0)
      : data_(0), teb_(nullptr) {
    dtl::teb_builder builder(std::move(bitmap_tree));
    const auto word_cnt = builder.serialized_size_in_words();
    data_.resize(word_cnt);
    builder.serialize(data_.data());
    teb_ = std::make_unique<teb_flat>(data_.data());
  }

  void display_L() {
    unsigned long int L_word_count = dtl::teb_flat::get_label_word_cnt(teb_->ptr_);

    std::cout << "Labels:\n";
    teb_word_type*  label_ptr = teb_->get_label_ptr(teb_->ptr_);
    for (int i = 0; i < L_word_count; i++) {
        std::cout << std::bitset<64>(label_ptr[i]) << std::endl;
    }
    std::cout << std::endl;
}

void display_T() {
    unsigned long int T_word_count = dtl::teb_flat::get_tree_word_cnt(teb_->ptr_);

    if (reconstruct) {
      std::cout << "Reconstruct-TEB\n";
    }
    std::cout << "Tree:\n";
    teb_word_type* tree_ptr = teb_->get_tree_ptr(teb_->ptr_);
    for (int i = 0; i < T_word_count; i++) {
        std::cout << std::bitset<64>(tree_ptr[i]) << std::endl;
    }
    std::cout << std::endl;
}

  teb_wrapper(const teb_wrapper& other) = delete;
  teb_wrapper(teb_wrapper&& other) noexcept = default;
  teb_wrapper& operator=(const teb_wrapper& other) = delete;
  teb_wrapper& operator=(teb_wrapper&& other) noexcept = default;

  /// Return the name of the implementation.
  static std::string
  name() noexcept {
    return "teb_wrapper";
  }

  /// Returns a 1-fill iterator, with efficient skip support.
  teb_iter __teb_inline__
  it() const noexcept {
    return std::move(teb_iter(*teb_));
  }

  /// Returns a 1-fill iterator, with WITHOUT efficient skip support.
  teb_scan_iter __teb_inline__
  scan_it() const noexcept {
    return std::move(teb_scan_iter(*teb_));
  }

  using skip_iter_type = teb_iter;
  using scan_iter_type = teb_scan_iter;

  /// Returns the length of the original bitmap.
  std::size_t __teb_inline__
  size() const noexcept {
    return teb_->size();
  }

  /// Returns the value of the bit at the given position.
  u1 __teb_inline__
  test(const std::size_t pos) const noexcept {
    return teb_->test(pos);
  }

  /// Updates the TEB with a new value for the specified bit in the uncompressed bitmap
  int
  update(std::size_t pos, u1 val, u1 *diff_val, u1 two_run_opt = false) {
    return teb_->update(pos, val, nullptr, diff_val, two_run_opt);
      /*
    if (teb_->update(pos, val)) {
      unsigned long int T_word_count = dtl::teb_flat::get_tree_word_cnt(teb_->ptr_);
      teb_->update_counter_++;
    } else {
      return;
    }
    
    if (teb_->update_counter_ >= teb_->update_threshold_) {
      if (reconstruct) {
        reconstruct_teb();
      } else {
        teb_->prune_traverse(0, 0);
    } 
      } */
  }

  void
  reconstruct_teb() {
    // Decompress
    boost::dynamic_bitset<$u32> new_bm = to_bitmap_using_iterator();

    // Construct new TEB
    teb_builder builder(new_bm);
    const auto word_cnt = builder.serialized_size_in_words();
    data_.clear();
    data_.resize(word_cnt);
    builder.serialize(data_.data());
    teb_ = std::make_unique<teb_flat>(data_.data());

    std::cout << "Rank LuT: ";
    for (std::size_t i = 0; i < teb_->rank_.lut.size(); i++) {
      std::cout << teb_->rank_.lut[i] << " ";
    }
    std::cout << std::endl;
  }

  /// Reconstruct a plain bitmap using the run iterator, copied from util/convert.hpp
  boost::dynamic_bitset<$u32>
  to_bitmap_using_iterator() {
    boost::dynamic_bitset<$u32> bm(teb_->size());
    auto it = scan_it();
    while (!it.end()) {
  #ifndef BOOST_DYNAMIC_BITSET_DONT_USE_FRIENDS
      for (std::size_t i = it.pos(); i < it.pos() + it.length(); ++i) {
        assert(bm[i] == false);
        bm[i] = true;
      }
  #else
      // HACK: This gives access to the private members of the boost::dynamic_bitset.
      const std::size_t b = it.pos();
      const std::size_t e = it.length() + b;
      dtl::bitmap_fun<$u32>::set(bm.m_bits.data(), b, e);
  #endif
      it.next();
    }
    return bm;
  }

  /// Return the size in bytes.
  std::size_t __teb_inline__
  size_in_bytes() const noexcept {
    return teb_->size_in_bytes();
  }

  /// Returns the name of the instance including the most important parameters
  /// in JSON.
  std::string
  info() const noexcept {
    auto determine_compressed_tree_depth = [&]() {
      auto i = it();
      $u64 height = 0;
      while (!i.end()) {
        const auto h = dtl::teb_util::determine_level_of(i.path());
        height = std::max(height, h);
        i.next();
      }
      return height;
    };
    return "{\"name\":\"" + name() + "\""
        + ",\"n\":" + std::to_string(teb_->n_)
        + ",\"size\":" + std::to_string(size_in_bytes())
        + ",\"tree_bits\":" + std::to_string(teb_->tree_bit_cnt_)
        + ",\"label_bits\":" + std::to_string(teb_->label_bit_cnt_)
        + ",\"implicit_inner_nodes\":"
        + std::to_string(teb_->implicit_inner_node_cnt_)
        + ",\"logical_tree_depth\":"
        + std::to_string(dtl::teb_util::determine_tree_height(teb_->n_))
        + ",\"encoded_tree_depth\":"
        + std::to_string(determine_compressed_tree_depth())
        + ",\"perfect_levels\":"
        + std::to_string(dtl::teb_util::determine_perfect_tree_levels(
            teb_->implicit_inner_node_cnt_))
        + ",\"opt_level\":" + std::to_string(3) // default
        + ",\"rank\":" + teb_->rank_.info(teb_->tree_bit_cnt_)
        + ",\"leading_zero_labels\":" + std::to_string(
            teb_->implicit_leading_label_cnt_)
        + "}";
  }

  /// For debugging purposes.
  void
  print(std::ostream& os) const noexcept {
    teb_->print(os);
  }
};
//===----------------------------------------------------------------------===//
} // namespace dtl