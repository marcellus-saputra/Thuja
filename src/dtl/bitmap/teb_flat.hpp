#pragma once
//===----------------------------------------------------------------------===//
#include "teb_types.hpp"
#include "teb_util.hpp"
#include "util/binary_tree_structure.hpp"
#include "util/bitmap_fun.hpp"
#include "util/bitmap_view.hpp"

#include <cassert>
#include <ostream>
#include <string>
#include <algorithm> //std::min
#include <boost/dynamic_bitset.hpp>

#include <bitset>
//===----------------------------------------------------------------------===//
namespace dtl {
//===----------------------------------------------------------------------===//
class teb_iter;
class teb_scan_iter;
class teb_wrapper;
//===----------------------------------------------------------------------===//

/// The TEB logic that is used to access a serialized TEB.
class teb_flat {
public: // TODO remove
  friend class teb_iter;
  friend class teb_scan_iter;
  friend class teb_wrapper;

  using word_type = teb_word_type;
  static constexpr auto word_size = sizeof(word_type);
  static constexpr auto word_bitlength = word_size * 8;

  using rank_type = teb_rank_logic_type;
  using size_type = teb_size_type;
  using bitmap_fn = dtl::bitmap_fun<word_type>;

  word_type* const ptr_; //originally const word_type* const
  const teb_header* const hdr_;

  /// The internal bitmap size. (rounded up to the next power of two)
  const size_type n_;
  /// The actual bitmap size.
  const size_type n_actual_;
  size_type tree_height_;
  size_type implicit_inner_node_cnt_;
  size_type implicit_trailing_leaf_cnt_;
  size_type implicit_leading_label_cnt_;
  size_type implicit_trailing_label_cnt_;
  size_type perfect_level_cnt_;
  size_type encoded_tree_height_;
  size_type free_bits_T_;
  size_type free_bits_L_;

  const word_type* tree_ptr_;
  size_type tree_bit_cnt_;
  const dtl::bitmap_view<const word_type> T_; // TODO cleanup

  const word_type* label_ptr_;
  size_type label_bit_cnt_;
  const dtl::bitmap_view<const word_type> L_; // TODO cleanup

  size_type* rank_lut_ptr_;
  teb_rank_type rank_;

  size_type* level_offsets_tree_lut_ptr_;
  size_type* level_offsets_labels_lut_ptr_;

  size_type update_counter_;
  size_type update_threshold_;

public:
  /// Returns the pointer to the header.
  static constexpr const teb_header* const
  get_header_ptr(const word_type* const ptr) {
    return (const teb_header* const)(void const*)ptr;
  }

  /// Returns the length of the header in number of words.
  static constexpr std::size_t
  get_header_word_cnt(const word_type* const ptr) {
    return sizeof(teb_header) / word_size;
  }

  /// Returns the pointer to the tree structure, or NULL if no tree exists.
  static constexpr const word_type* const
  get_tree_ptr(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    return (hdr->tree_bit_cnt != 0)
        ? ptr + get_header_word_cnt(ptr)
        : nullptr;
  }
  static constexpr word_type*
  get_tree_ptr(word_type* ptr) {
    auto* hdr = get_header_ptr(ptr);
    return (hdr->tree_bit_cnt != 0)
        ? ptr + get_header_word_cnt(ptr)
        : nullptr;
  }

  /// Returns the length of the tree in number of words.
  static constexpr std::size_t
  get_tree_word_cnt(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    return (hdr->tree_bit_cnt + word_bitlength - 1) / word_bitlength;
  }

  /// Returns the pointer to the rank structure, or NULL if it does not exist.
  static constexpr const size_type* const
  get_rank_ptr(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    const auto hdr_word_cnt = get_header_word_cnt(ptr);
    return (hdr->tree_bit_cnt > 0)
        ? reinterpret_cast<const size_type* const>(ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr))
        : nullptr;
    /*
    return (hdr->tree_bit_cnt > 1024)
        ? reinterpret_cast<const size_type* const>(ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr))
        : nullptr;*/
  }
  static constexpr size_type*
  get_rank_ptr(word_type* ptr) {
    auto* hdr = get_header_ptr(ptr);
    auto hdr_word_cnt = get_header_word_cnt(ptr);
    return (hdr->tree_bit_cnt > 0)
        ? reinterpret_cast<size_type*>(ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr))
        : nullptr;
    /*
    return (hdr->tree_bit_cnt > 1024)
        ? reinterpret_cast<size_type*>(ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr))
        : nullptr;*/
  }

  /// Returns the length of the rank helper structure in number of words.
  static constexpr std::size_t
  get_rank_word_cnt(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    const auto rank_size_bytes = rank_type::estimate_size_in_bytes(hdr->tree_bit_cnt);
    return (hdr->tree_bit_cnt > 0)
        ? (rank_size_bytes + word_size - 1) / word_size
        : 0;
    /*
    return (hdr->tree_bit_cnt > 1024)
        ? (rank_size_bytes + word_size - 1) / word_size
        : 0;*/
  }

  /// Returns the pointer to the labels, or NULL if it does not exist.
  static constexpr const word_type* const
  get_label_ptr(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    const auto rank_word_cnt = get_rank_word_cnt(ptr);
    return (hdr->label_bit_cnt != 0)
        ? ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr) + get_rank_word_cnt(ptr)
        : nullptr;
  }
  static constexpr word_type*
  get_label_ptr(word_type* ptr) {
    auto* hdr = get_header_ptr(ptr);
    auto rank_word_cnt = get_rank_word_cnt(ptr);
    return (hdr->label_bit_cnt != 0)
        ? ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr) + get_rank_word_cnt(ptr)
        : nullptr;
  }

  /// Returns the length of the labels in number of words.
  static constexpr std::size_t
  get_label_word_cnt(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    return (hdr->label_bit_cnt + word_bitlength - 1) / word_bitlength;
  }

  /// Returns the pointer to the additional meta data, or NULL if it does not exist.
  static constexpr const word_type* const
  get_metadata_ptr(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    return (hdr->has_level_offsets != 0)
        ? ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr)
            + get_rank_word_cnt(ptr) + get_label_word_cnt(ptr)
        : nullptr;
  }
  static constexpr word_type*
  get_metadata_ptr(word_type* ptr) {
    auto* hdr = get_header_ptr(ptr);
    return (hdr->has_level_offsets != 0)
        ? ptr + get_header_word_cnt(ptr) + get_tree_word_cnt(ptr)
            + get_rank_word_cnt(ptr) + get_label_word_cnt(ptr)
        : nullptr;
  }

  /// Returns the length of the additional meta data in number of words.
  static constexpr std::size_t
  get_metadata_word_cnt(const word_type* const ptr) {
    const auto* hdr = get_header_ptr(ptr);
    const auto entry_cnt = hdr->encoded_tree_height - hdr->perfect_level_cnt;
    const auto size_in_bytes = 2 * sizeof(size_type) * entry_cnt;
    const auto word_cnt = (size_in_bytes + word_size - 1) / word_size;
    return hdr->has_level_offsets ? word_cnt : 0;
  }

  /// Construct a TEB instance that provides all the logic to work with a
  /// serialized TEB.
  explicit teb_flat(word_type* const ptr) //originally const word_type* const ptr
      : ptr_(ptr),
        hdr_(get_header_ptr(ptr)),
        n_(dtl::next_power_of_two(get_header_ptr(ptr)->n)),
        n_actual_(get_header_ptr(ptr)->n),
        tree_height_(dtl::log_2(dtl::next_power_of_two(get_header_ptr(ptr)->n))),
        implicit_inner_node_cnt_(get_header_ptr(ptr)->implicit_inner_node_cnt),
        implicit_trailing_leaf_cnt_(get_header_ptr(ptr)->trailing_inner_node_cnt),
        implicit_trailing_label_cnt_(get_header_ptr(ptr)->implicit_trailing_label_cnt),
        implicit_leading_label_cnt_(get_header_ptr(ptr)->implicit_leading_label_cnt),
        perfect_level_cnt_(get_header_ptr(ptr)->perfect_level_cnt),
        encoded_tree_height_(get_header_ptr(ptr)->encoded_tree_height),
        update_counter_(get_header_ptr(ptr)->update_counter),
        update_threshold_(get_header_ptr(ptr)->update_threshold),
        tree_ptr_(get_tree_ptr(ptr) != nullptr ? get_tree_ptr(ptr) : &teb_null_word),
        T_((get_tree_ptr(ptr) != nullptr)
                ? get_tree_ptr(ptr)
                : &teb_null_word,
            (get_tree_ptr(ptr) != nullptr)
                ? get_tree_ptr(ptr) + teb_flat::get_tree_word_cnt(ptr)
                : &teb_null_word + 1),
        tree_bit_cnt_((get_tree_ptr(ptr) != nullptr)
                ? get_header_ptr(ptr)->tree_bit_cnt
                : 1),
        rank_lut_ptr_(get_rank_ptr(ptr)),
        label_ptr_(get_label_ptr(ptr)),
        L_(get_label_ptr(ptr), get_label_ptr(ptr) + teb_flat::get_label_word_cnt(ptr)),
        label_bit_cnt_(get_header_ptr(ptr)->label_bit_cnt),
        level_offsets_tree_lut_ptr_(
            reinterpret_cast<size_type*>(get_metadata_ptr(ptr))),
        level_offsets_labels_lut_ptr_(
            reinterpret_cast<size_type*>(get_metadata_ptr(ptr))
            + get_header_ptr(ptr)->encoded_tree_height) {
            //- get_header_ptr(ptr)->perfect_level_cnt) {
    // TODO maybe it is not necessary to copy the header data

    // Calculate number of free bits
    free_bits_T_ = (get_tree_word_cnt(ptr_) * word_bitlength) - tree_bit_cnt_;
    free_bits_L_ = (get_label_word_cnt(ptr_) * word_bitlength) - label_bit_cnt_;

    // Initialize rank helper structure.
    if (rank_lut_ptr_ == nullptr) {
      // The TEB instance does not contain a rank LuT. This typically happens
      // when the tree structure is very small. In that case the LuT is
      // initialized on the fly.
      rank_.init(tree_ptr_, tree_ptr_ + get_tree_word_cnt(ptr));
      rank_lut_ptr_ = rank_.lut.data();
    }
  }

  teb_flat(const teb_flat& other) = default;
  teb_flat(teb_flat&& other) = default;
  teb_flat& operator=(const teb_flat& other) = default;
  teb_flat& operator=(teb_flat&& other) = default;
  ~teb_flat() = default;

  /// Return the name of the implementation.
  static std::string
  name() noexcept {
    return "teb_flat";
  }

  /// Returns the length of the original bitmap.
  std::size_t
  size() const noexcept {
    return n_actual_;
  }

  /// Returns the value of the bit at the given position.
  u1 __teb_inline__
  test(const std::size_t pos) const noexcept {
    size_type level = perfect_level_cnt_ - 1;
    const auto foo = pos >> (tree_height_ - level);

    // Determine the top-node idx.
    const auto top_node_idx_begin = (1ull << (perfect_level_cnt_ - 1)) - 1;
    size_type node_idx = top_node_idx_begin + foo;

    auto i = tree_height_ - 1 - level;
    while (!is_leaf_node(node_idx)) {
      u1 direction_bit = dtl::bits::bit_test(pos, i);
      node_idx = 2 * rank_inclusive(node_idx) - 1; // left child
      node_idx += direction_bit; // right child if bit is set, left child otherwise
      --i;
    }
    return get_label(node_idx);
  }

  u64 get_run_length_by_pos(std::size_t pos) {
    size_type level = perfect_level_cnt_ - 1;
    const auto foo = pos >> (tree_height_ - level);

    // Determine the top-node idx.
    const auto top_node_idx_begin = (1ull << (perfect_level_cnt_ - 1)) - 1;
    size_type node_idx = top_node_idx_begin + foo;

    auto i = tree_height_ - 1 - level;
    while (!is_leaf_node(node_idx)) {
      u1 direction_bit = dtl::bits::bit_test(pos, i);
      node_idx = 2 * rank_inclusive(node_idx) - 1; // left child
      node_idx += direction_bit; // right child if bit is set, left child otherwise
      --i;
    }
    return i + 1;
  }

  // Set a new value at the given position
  int update(std::size_t pos, bool val, clock_t *timer = nullptr, u1 *diff_val = nullptr, u1 two_run_opt = false) {
    // I copied the following code from test() right above
    size_type level = perfect_level_cnt_ - 1;
    const auto foo = pos >> (tree_height_ - level);

    // Determine the top-node idx.
    const auto top_node_idx_begin = (1ull << (perfect_level_cnt_ - 1)) - 1;
    size_type node_idx = top_node_idx_begin + foo;

    int i = tree_height_ - 1 - level;
    while (!is_leaf_node(node_idx)) {
      u1 direction_bit = dtl::bits::bit_test(pos, i);
      node_idx = 2 * rank_inclusive(node_idx) - 1; // left child
      node_idx += direction_bit; // right child if bit is set, left child otherwise
      --i;
      level++;
    }

    // Check if the node we stopped at is at the lowest level and whether it actually needs to be changed
    if (diff_val != nullptr) {
      u1 bitmap_val = get_label(node_idx);
      u1 actual_val = *diff_val ^ get_label(node_idx);
      if (actual_val == val) {
        return 0;
      }
      val = !bitmap_val; // change val to !actual_val to avoid changing the rest of the code
    } else {
      if (get_label(node_idx) == val) {
        #ifdef DEBUG
        std::cout << "Update failed: node ID " << node_idx << ", position " << pos << " already has the same value\n";
        #endif
        return 0;
      }
    }

    // I believe determine_tree_height() just calculates log_2(n_), but it has worked so far
    u64 lowest_level_offset = get_level_offset_tree(dtl::teb_util::determine_tree_height(n_));
    if (node_idx < lowest_level_offset) {
      #ifdef DEBUG
      std::cout << "Update failed: node ID " << node_idx << ", position " << pos << " is not at the lowest level\n";
      #endif
      if (diff_val == nullptr) {
        return runbreaking_update(pos,val ,level, node_idx, i, timer);
      } else {
        if (two_run_opt == true) {
          if (level == tree_height_ - 2) return runbreaking_update(pos, val, level, node_idx, i, timer);
        }
        return 2;
      }
      //if (level == tree_height_ - 2) return runbreaking_update(pos, val, level, node_idx, i, timer); // if at second level, in-place update
      //return 2; // run-breaking update, do diff update instead
    }

    u64 node_label_idx = get_label_idx(node_idx);
    u1 node_label_is_leading = node_label_idx < implicit_leading_label_cnt_;
    u1 node_label_is_trailing = node_label_idx >= implicit_leading_label_cnt_ + label_bit_cnt_;
    u1 node_label_is_implicit = node_label_is_leading || node_label_is_trailing;

    // Only 0-labels can be implicit
    if (node_label_is_implicit) {
      std::size_t cost_L = 0;
      if (node_label_is_leading) {
        cost_L += implicit_leading_label_cnt_ - node_label_idx;
        if (free_bits_L_ >= cost_L) {
          if (cost_L > word_bitlength) {
            std::size_t bits_to_insert = cost_L % word_bitlength;
            std::size_t words_to_insert = cost_L / word_bitlength;
            insert_into_labels(0, bits_to_insert, 0);
            label_bit_cnt_ += bits_to_insert;
            implicit_leading_label_cnt_ -= bits_to_insert;
            free_bits_L_ -= bits_to_insert;
            prepend_words_L(words_to_insert);
            label_bit_cnt_ += words_to_insert * word_bitlength;
            implicit_leading_label_cnt_ -= words_to_insert * word_bitlength;
            free_bits_L_ -= words_to_insert * word_bitlength;
          } else {
            insert_into_labels(0, cost_L, 0);
            label_bit_cnt_ += cost_L;
            implicit_leading_label_cnt_ -= cost_L;
            free_bits_L_ -= cost_L; 
          }
        } else {
          #ifdef DEBUG
          std::cout << "Update failed: node ID " << node_idx << ", position " << pos << "; not enough free bits to materialize leading labels\n";
          #endif
          return 2; // do diff update instead
          //return 0;
        }
      } else {
        cost_L += node_label_idx - (implicit_leading_label_cnt_ + label_bit_cnt_ - 1);
        if (free_bits_L_ >= cost_L) {
          label_bit_cnt_ += cost_L;
          implicit_trailing_label_cnt_ -= cost_L;
          free_bits_L_ -= cost_L;
        } else {
          #ifdef DEBUG
          std::cout << "Update failed: node ID " << node_idx << ", position " << pos << "; not enough free bits to materialize trailing labels\n";
          #endif
          return 2; // do diff update instead
          // return 0;
        }
      }
    } 

    // Sets the corresponding location in L, luckily bitmap_fun already has a method for it
    bitmap_fn::set(get_label_ptr(ptr_), node_label_idx - implicit_leading_label_cnt_, val);
    return 1; // In-place update performed
  }

  int runbreaking_update(std::size_t pos, u1 val, std::size_t level, u64 node_idx, int i, clock_t *timer) {
    std::size_t rank_offset = 0;
    std::vector<size_type> T_insert_vec;
    std::vector<size_type> L_insert_vec;
    std::vector<size_type> set_vec;
    size_type n = node_idx;
    size_type remove_label_idx = get_label_idx(node_idx);
    /*
      rank_offset = 1
      set_vec.push_back(node_idx)
      while (i > 0)
        direction bit = ...
        left child = 2 * (rank_inclusive(node_idx) + rank offset)
        -> check cost of insert
        insert_vec.push_back(left_child)
        set_vec.push_back(left child + direction bit)
        node_idx = left child - 2 * rank offset
        rank offset++
        label_idx = left child - rank(node_idx) - rank offset
        -> check cost of insert, if explicit add to label offset
        insert_vec_l.push_back(label idx)
        --i
    */
    std::size_t T_implicit_trailing_begin = implicit_inner_node_cnt_ + tree_bit_cnt_;
    std::size_t L_implicit_trailing_begin = implicit_leading_label_cnt_ + label_bit_cnt_;
    std::size_t L_implicit_leading_end = implicit_leading_label_cnt_;
    std::size_t cost_T = 0;
    int cost_L = 0;

    if (n >= T_implicit_trailing_begin) {
      cost_T += n - T_implicit_trailing_begin + 1;
    }

    if (cost_T > free_bits_T_) {
      //std::cout << "\nRun-breaking update: failed due to not enough bits in T";
      return 2;
    }

    set_vec.push_back(n);
    rank_offset++;

    size_type removed_label = get_label_idx(n);
    if (removed_label < L_implicit_leading_end) {
      L_implicit_leading_end--;
    } else if (n < L_implicit_trailing_begin) {
      cost_L -= 1;
    }

    while (i >= 0) {
      u1 direction_bit = dtl::bits::bit_test(pos, i); // left if 0, right if 1
      u64 left_child = 2 * (rank_inclusive(n) + rank_offset) - 1;

      if (i > 0) {
        // Calculate cost to T
        if (direction_bit) {
          // 01
          if (left_child <= T_implicit_trailing_begin) {
            cost_T += 2;
            T_implicit_trailing_begin += 2;
          } else {
            cost_T += left_child - T_implicit_trailing_begin + 2;
            T_implicit_trailing_begin += left_child - T_implicit_trailing_begin + 2;
          }
        } else {
          // 10
          if (left_child < T_implicit_trailing_begin) {
            cost_T += 2;
            T_implicit_trailing_begin += 2;
          } else {
            cost_T += left_child - T_implicit_trailing_begin + 1;
            T_implicit_trailing_begin += left_child - T_implicit_trailing_begin + 1;
          }
        }

        if (cost_T > free_bits_T_) {
          //std::cout << "Run-breaking update: failed due to not enough bits in T, cost: " << cost_T << " " << T_implicit_trailing_begin << " " << left_child;
          return 2;
        }

        set_vec.push_back(left_child + direction_bit);
      }
      T_insert_vec.push_back(left_child);

      n = left_child - 1 - (2 * (rank_offset - 1));

      // Calculate cost to L
      u64 label = left_child - rank_inclusive(n) - rank_offset;
      u1 label_leading = label < L_implicit_leading_end;
      u1 label_trailing = label >= L_implicit_trailing_begin;
      u1 label_explicit = !label_leading && !label_trailing;

      // Not lowest level -> insert labels opposite to val
      if (i > 0) {
        // Updating to 1 -> insert 0 
        if (val) {
          if (label_leading) L_implicit_leading_end++;
          // No changes in inserting trailing 0 label
          else if (!label_trailing) {
            cost_L += 1;
            L_implicit_trailing_begin += 1;
          }
        } else {
          if (label_leading) {
            cost_L += L_implicit_leading_end - label + 1;
            L_implicit_leading_end = label;
          } else if (label_trailing) {
            cost_L += label - L_implicit_trailing_begin + 1;
            L_implicit_trailing_begin = label + 1;
          } else {
            cost_L++;
            L_implicit_trailing_begin += 1;
          }
        }
      }
      //std::cout << "L insert cost: " << cost_L << "\n";

      L_insert_vec.push_back(left_child - rank_inclusive(n) - rank_offset);
      if (i == 0) {
        u64 right_label = label + 1;
        u1 right_label_leading = right_label < L_implicit_leading_end;
        u1 right_label_trailing = right_label >= L_implicit_trailing_begin;
        u1 right_explicit = !right_label_leading && !right_label_trailing;
        // 01 : updating right child to 1 or updating left child to 0
        if (val && direction_bit || !val && !direction_bit) {
          if (label < L_implicit_leading_end) {
            cost_L += L_implicit_leading_end - label + 1;
          } else if (label == L_implicit_leading_end) {
            cost_L += 1;
          } else if (label >= L_implicit_trailing_begin) {
            cost_L += L_implicit_trailing_begin - label + 2;
          } else {
            // both explicit
            cost_L += 2;
          }
        } else {
          // 10
          if (label <= L_implicit_leading_end) {
            cost_L += L_implicit_leading_end - label + 2;
          } else if (label < L_implicit_trailing_begin) {
            cost_L += 2;
          } else if (label == L_implicit_trailing_begin) {
            cost_L += 1;
          } else if (label > L_implicit_trailing_begin) {
            cost_L += label - L_implicit_trailing_begin + 1;
          }
        }

        if (cost_L > free_bits_L_) {
          // std::cout << "Run-breaking update: failed due to not enough bits in L.\n";
          return 2;
        }

        L_insert_vec.push_back(right_label);
      }
      rank_offset++;
      --i;
    }

    // timer for test
    if (timer) *timer = clock() - *timer;

    std::size_t teb_max_height = dtl::teb_util::determine_tree_height(n_);
    std::vector<int> ofs_tree_update_vec(teb_max_height, 0);
    std::vector<int> ofs_labels_update_vec(teb_max_height, 0);
    
    // Insert leaf nodes marked in insert vector
    std::size_t level_t = level + 1; // copy level variable, since we'll be using it again later
    for (auto i = 0; i < T_insert_vec.size(); i++) {
      size_type left = T_insert_vec[i];
      if (left < implicit_inner_node_cnt_ + tree_bit_cnt_) {
        insert_into_tree(left - implicit_inner_node_cnt_, 2, 0);
        free_bits_T_ -= 2;
        tree_bit_cnt_ += 2;
      } else {
        implicit_trailing_leaf_cnt_ += 2;
      }
      if (i < T_insert_vec.size() - 1) {
        ofs_tree_update_vec[level_t] += 2;
      }
      level_t++;
    }
    

    // Set inserted leaf nodes to inner nodes according to set vector, update rank LuT
    const std::size_t rank_block_granularity = rank_.get_block_bitlength();
    std::size_t all_bits = get_tree_word_cnt(ptr_) * word_bitlength;
    std::size_t rank_lut_block_cnt = (all_bits / rank_block_granularity) + 1; // badly worded; it's actually the highest index in the rank lut
    if (all_bits % rank_block_granularity) rank_lut_block_cnt += 1;
    std::size_t lut_begin_block_idx = ((set_vec[0] - implicit_inner_node_cnt_) / rank_block_granularity) + 1;
    std::vector<int> lut_update_vec(rank_lut_block_cnt - lut_begin_block_idx, 0);

    for (auto i = 0; i < set_vec.size(); i++) {
      size_type set_idx = set_vec[i];
      std::size_t trailing_begin = implicit_inner_node_cnt_ + tree_bit_cnt_;
      if (set_idx >= trailing_begin) {
        free_bits_T_ -= set_idx - trailing_begin + 1;
        implicit_trailing_leaf_cnt_ -= set_idx - trailing_begin + 1;
        tree_bit_cnt_ += set_idx - trailing_begin + 1;
      }
      bitmap_fn::set(get_tree_ptr(ptr_), set_idx - implicit_inner_node_cnt_);
      lut_update_vec[((set_idx - implicit_inner_node_cnt_) / rank_block_granularity + 1) - lut_begin_block_idx] += 1;
    }

    int lut_update_sum = 0;
    for (int i = 0; i < lut_update_vec.size(); i++) {
      lut_update_sum += lut_update_vec[i];
      rank_lut_ptr_[i + lut_begin_block_idx] += lut_update_sum;    
    }

    // Remove node_idx label bit
    if (remove_label_idx < implicit_leading_label_cnt_) {
      implicit_leading_label_cnt_--;
    } else if (remove_label_idx >= implicit_leading_label_cnt_ + label_bit_cnt_) {
      implicit_trailing_label_cnt_--;
    } else {
      remove_from_labels(remove_label_idx - implicit_leading_label_cnt_, 1);
      label_bit_cnt_--;
      free_bits_L_++;
    }
    ofs_labels_update_vec[level]--;

    // Insert label bits according to insert vector
    u1 insert_label = !val;
    std::size_t level_l = level + 1;
    for (auto i = 0; i < L_insert_vec.size() - 2; i++) {
      size_type insert_idx = L_insert_vec[i];
      std::size_t trailing_begin = implicit_leading_label_cnt_ + label_bit_cnt_;
      if (insert_idx < implicit_leading_label_cnt_) {
        if (insert_label) {
          std::cout << "Implicit label insert cost: " << implicit_leading_label_cnt_ - insert_idx + 1;
          insert_into_labels(0, implicit_leading_label_cnt_ - insert_idx + 1, 0);
          bitmap_fn::set(get_label_ptr(ptr_), 0);
          label_bit_cnt_ += implicit_leading_label_cnt_ - insert_idx + 1;
          free_bits_L_ -= implicit_leading_label_cnt_ - insert_idx + 1;
          implicit_leading_label_cnt_ -= implicit_leading_label_cnt_ - insert_idx;
        } else {
          implicit_leading_label_cnt_++;
        }
      } else if (insert_idx >= trailing_begin) {
        if (insert_label) {
          label_bit_cnt_ += insert_idx - trailing_begin + 1;
          free_bits_L_ -= insert_idx - trailing_begin + 1;
          implicit_trailing_label_cnt_ -= insert_idx - trailing_begin;
          bitmap_fn::set(get_label_ptr(ptr_), insert_idx);
        } else {
          implicit_trailing_label_cnt_++;
        }
      } else {
        insert_into_labels(insert_idx - implicit_leading_label_cnt_, 1, insert_label);
        label_bit_cnt_++;
        free_bits_L_--;
      }
      ofs_labels_update_vec[level_l]++;
      level_l++;
    }

    // Insert last 2 label bits at the lowest level
    u1 direction_bit = dtl::bits::bit_test(pos, 0); // left if 0, right if 1
    size_type left_label = L_insert_vec[L_insert_vec.size() - 2];
    size_type right_label = L_insert_vec[L_insert_vec.size() - 1];

    L_implicit_trailing_begin = implicit_leading_label_cnt_ + label_bit_cnt_;
    L_implicit_leading_end = implicit_leading_label_cnt_;

    // 01 : updating right child to 1 or updating left child to 0
    if (val && direction_bit || !val && !direction_bit) {
      if (left_label < L_implicit_leading_end) {
        // both leading
        insert_into_labels(0, L_implicit_leading_end - left_label + 1, 0);
        bitmap_fn::set(get_label_ptr(ptr_), 0);
        free_bits_L_ -= L_implicit_leading_end - left_label + 1;
        label_bit_cnt_ += L_implicit_leading_end - left_label + 1;
        implicit_inner_node_cnt_ -= L_implicit_leading_end - left_label;
      } else if (left_label == L_implicit_leading_end) {
        // only left leading
        insert_into_labels(0, 1, 1);
        free_bits_L_--;
        label_bit_cnt_++;
        implicit_leading_label_cnt_++;
      } else if (left_label > L_implicit_trailing_begin) {
        // both trailing
        free_bits_L_ -= left_label - L_implicit_trailing_begin + 2;
        label_bit_cnt_ += left_label - L_implicit_trailing_begin + 2;
        implicit_trailing_label_cnt_ -= left_label - L_implicit_trailing_begin;
        bitmap_fn::set(get_label_ptr(ptr_), right_label - implicit_leading_label_cnt_);
      } else {
        // both explicit
        insert_into_labels(left_label - implicit_leading_label_cnt_, 2, 0);
        free_bits_L_ -= 2;
        label_bit_cnt_ += 2;
        bitmap_fn::set(get_label_ptr(ptr_), right_label - implicit_leading_label_cnt_);
      }
    } else {
      // 10
      if (left_label <= L_implicit_leading_end) {
        insert_into_labels(0, L_implicit_trailing_begin - left_label + 2, 0);
        bitmap_fn::set(get_label_ptr(ptr_), 0);
        label_bit_cnt_ += L_implicit_leading_end - left_label + 2;
        free_bits_L_ -= L_implicit_leading_end - left_label + 2;
        implicit_leading_label_cnt_ -= L_implicit_leading_end - left_label;
      } else if (left_label < L_implicit_trailing_begin) {
        insert_into_labels(left_label - implicit_leading_label_cnt_, 2, 0);
        free_bits_L_ -= 2;
        label_bit_cnt_ += 2;
        bitmap_fn::set(get_label_ptr(ptr_), left_label - implicit_leading_label_cnt_);
      } else if (left_label == L_implicit_trailing_begin) {
        label_bit_cnt_++;
        free_bits_L_--;
        implicit_trailing_label_cnt_++;
        bitmap_fn::set(get_label_ptr(ptr_), left_label - implicit_leading_label_cnt_);
      } else if (left_label > L_implicit_trailing_begin) {
        label_bit_cnt_ += left_label - L_implicit_trailing_begin + 1;
        free_bits_L_ -= left_label - L_implicit_trailing_begin + 1;
        implicit_trailing_label_cnt_ -= left_label - L_implicit_trailing_begin - 1;
        bitmap_fn::set(get_label_ptr(ptr_), left_label - implicit_leading_label_cnt_);
      }
    }

    // Update level offset tables
    int T_update_sum = 0;
    int L_update_sum = 0;
  
    for (std::size_t i = 0; i < ofs_tree_update_vec.size(); i++) {
      T_update_sum += ofs_tree_update_vec[i];
      L_update_sum += ofs_labels_update_vec[i];

      level_offsets_tree_lut_ptr_[i + 1] += T_update_sum;
      level_offsets_labels_lut_ptr_[i + 1] += L_update_sum;
    }

    encoded_tree_height_ = teb_util::determine_tree_height(n_) + 1;
    return 1;
  }

  void
  remove_from_labels(u32 begin_idx, std::size_t bits_to_remove) {
    if (bits_to_remove == 0) return;
    const auto word_idx = begin_idx / word_bitlength;
    const auto bit_idx = begin_idx % word_bitlength;

    word_type* bitmap = get_label_ptr(ptr_);
    std::size_t word_count = get_label_word_cnt(ptr_);

    // Clear the bits
    bitmap_fn::clear(bitmap, begin_idx, begin_idx + bits_to_remove);

    if (bit_idx + bits_to_remove > word_bitlength) {
      std::size_t first_word_bits_to_remove = word_bitlength - bit_idx;
      std::size_t next_word_bits_to_shift = bits_to_remove - first_word_bits_to_remove;
      
      // Shift second word to the right
      bitmap[word_idx + 1] >>= next_word_bits_to_shift;

      // Copy bits from second word to fill in missing bits in the first word, shift second word again
      bitmap[word_idx] |= (bitmap[word_idx +1] & ((1ull << first_word_bits_to_remove + 1) - 1)) << (word_bitlength - first_word_bits_to_remove);
      bitmap[word_idx + 1] >>= first_word_bits_to_remove;

      // Get bits from next word and shift next word, and so on...
      for (std::size_t i = word_idx + 1; i < word_count - 1; i++) {
        bitmap[i] |= (bitmap[i+1] & ((1ull << bits_to_remove + 1) - 1)) << (word_bitlength - bits_to_remove);
        bitmap[i+1] >>= bits_to_remove; 
      }
    } else {
      // Remove cleared bits from first word, shift lower half of bitmap to the left
      if (!(bit_idx + bits_to_remove == word_bitlength)) {
        const word_type mask = (1ull << (bit_idx + bits_to_remove)) - 1;
        bitmap[word_idx] = ((bitmap[word_idx] & ~mask) >> bits_to_remove) | (bitmap[word_idx] & mask);
      }
      
      // Get bits from next word and shift next word, and so on...
      for (std::size_t i = word_idx; i < word_count - 1; i++) {
        bitmap[i] |= (bitmap[i+1] & ((1ull << bits_to_remove + 1) - 1)) << (word_bitlength - bits_to_remove);
        bitmap[i+1] >>= bits_to_remove; 
      } 
    }
  }

  void
  insert_into_labels(u32 begin_idx, std::size_t bits_to_insert, u1 val) {
    if (bits_to_insert == 0) return;
    const auto word_idx = begin_idx / word_bitlength;
    const auto bit_idx = begin_idx % word_bitlength;

    word_type* bitmap = get_label_ptr(ptr_);
    std::size_t word_count = get_label_word_cnt(ptr_);

    // For now, assume there is always enough space
    if (bit_idx + bits_to_insert >= word_bitlength) {
      std::size_t first_word_bits_to_insert = word_bitlength - bit_idx;
      std::size_t next_word_bits_to_shift = bits_to_insert - first_word_bits_to_insert;

      for (std::size_t i = word_count - 1; i > word_idx + 1; i--) {
        bitmap[i] <<= bits_to_insert;
        bitmap[i] |= bitmap[i - 1] >> (word_bitlength - bits_to_insert);
      }

      bitmap[word_idx + 1] <<= first_word_bits_to_insert;
      bitmap[word_idx + 1] |= (bitmap[word_idx] & (~(1ull << (word_bitlength - first_word_bits_to_insert - 2) - 1))) >> (word_bitlength - first_word_bits_to_insert);
      bitmap[word_idx + 1] <<= next_word_bits_to_shift;

      if (val) {
        bitmap_fn::set(bitmap, begin_idx, begin_idx + bits_to_insert);
      } else {
        bitmap_fn::clear(bitmap, begin_idx, begin_idx + bits_to_insert);
      }
    } else {
      // Shift the bitmap
      for (std::size_t i = word_count - 1; i > word_idx; i--) {
        //bitmap[i] <<= bits_to_insert;
        //bitmap[i] |= (bitmap[i - 1] & (~(1ull << (word_bitlength - bits_to_insert - 2) - 1))) >> (word_bitlength - bits_to_insert);
        word_type imported_bits = bitmap[i - 1] >> (word_bitlength - bits_to_insert);
        bitmap[i] <<= bits_to_insert;
        bitmap[i] |= imported_bits;
      }
      
      // Make space in the first word
      const word_type mask = (1ull << bit_idx) - 1;
      bitmap[word_idx] = ((bitmap[word_idx] & ~mask) << bits_to_insert) | (bitmap[word_idx] & mask);

      // Set the bits
      if (val) {
        bitmap_fn::set(bitmap, begin_idx, begin_idx + bits_to_insert);
      } else {
        bitmap_fn::clear(bitmap, begin_idx, begin_idx + bits_to_insert);
      }
    }
  }

  void
  remove_from_tree(u32 begin_idx, std::size_t bits_to_remove) {
    if (bits_to_remove == 0) return;
    const auto word_idx = begin_idx / word_bitlength;
    const auto bit_idx = begin_idx % word_bitlength;

    word_type* bitmap = get_tree_ptr(ptr_);
    std::size_t word_count = get_tree_word_cnt(ptr_);

    const std::size_t rank_block_granularity = rank_.get_block_bitlength();
    std::size_t all_bits = word_count * word_bitlength;
    std::size_t rank_lut_block_cnt = (all_bits / rank_block_granularity) + 1;
    if (all_bits % rank_block_granularity) rank_lut_block_cnt += 1;
    //std::size_t rank_lut_block_cnt = ((word_bitlength * word_count) + rank_block_granularity - 1) / rank_block_granularity + 1;
    std::size_t lut_begin_block_idx = (begin_idx / rank_block_granularity) + 1;
    assert(rank_lut_block_cnt > lut_begin_block_idx);
    std::vector<int> lut_update_vec(rank_lut_block_cnt - lut_begin_block_idx, 0);

    if (bit_idx + bits_to_remove > word_bitlength) {
      // Remove affects 2 words
      std::size_t first_word_bits_to_remove = word_bitlength - bit_idx;
      std::size_t next_word_bits_to_shift = bits_to_remove - first_word_bits_to_remove;

      // Manually handle LuT updates for first and second words
      std::size_t first_word_lut_block = ((word_idx * word_bitlength) / rank_block_granularity) + 1;
      std::size_t second_word_lut_block = (((word_idx + 1) * word_bitlength) / rank_block_granularity) + 1;

      lut_update_vec[first_word_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(bitmap[word_idx] >> (word_bitlength - first_word_bits_to_remove));
      lut_update_vec[second_word_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(bitmap[word_idx + 1] << (word_bitlength - next_word_bits_to_shift));

      // Clear the bits, so existing 1-bits don't interfere with the bitmask
      bitmap_fn::clear(bitmap, begin_idx, begin_idx + bits_to_remove);
      
      // Shift second word to the right
      bitmap[word_idx + 1] >>= next_word_bits_to_shift;

      // Copy bits from second word to fill in missing bits in the first word, shift second word again
      word_type first_word_imported_bits  = (bitmap[word_idx +1] & ((1ull << first_word_bits_to_remove + 1) - 1)) << (word_bitlength - first_word_bits_to_remove);
      bitmap[word_idx] |= first_word_imported_bits;
      bitmap[word_idx + 1] >>= first_word_bits_to_remove;

      if (second_word_lut_block > first_word_lut_block) {
        std::size_t bit_count = bitmap_fn::count_set_bits(first_word_imported_bits);
        lut_update_vec[0] += bit_count;
        lut_update_vec[1] -= bit_count;
      }

      // Get bits from next word and shift next word, and so on...
      for (std::size_t i = word_idx + 1; i < word_count - 1; i++) {
        word_type next_word_exported_bits = bitmap[i + 1] << (word_bitlength - bits_to_remove);
        word_type imported_bits = (bitmap[i+1] & ((1ull << bits_to_remove + 1) - 1)) << (word_bitlength - bits_to_remove);
        bitmap[i] |= imported_bits;
        bitmap[i+1] >>= bits_to_remove;

        std::size_t current_lut_block = ((i * word_bitlength) / rank_block_granularity) + 1;
        std::size_t next_word_lut_block = (((i + 1) * word_bitlength) / rank_block_granularity) + 1;

        if (next_word_lut_block > current_lut_block) { 
          lut_update_vec[current_lut_block - lut_begin_block_idx] += bitmap_fn::count_set_bits(imported_bits);
          lut_update_vec[next_word_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(next_word_exported_bits);
        }
      }
    } else {
      // Clear the bits, so existing 1-bits don't interfere with the bitmask
      lut_update_vec[0] -= bitmap_fn::count(bitmap, begin_idx, begin_idx + bits_to_remove);
      bitmap_fn::clear(bitmap, begin_idx, begin_idx + bits_to_remove);

      // Remove cleared bits from first word, shift lower half of bitmap to the left
      // Corner case where the last bit of the word is removed; in this case, no need to "close the hole"
      if (!(bit_idx + bits_to_remove == word_bitlength)) {
        const word_type mask = (1ull << (bit_idx + bits_to_remove)) - 1;
        bitmap[word_idx] = ((bitmap[word_idx] & ~mask) >> bits_to_remove) | (bitmap[word_idx] & mask);
      }

      // Get bits from next word and shift next word, and so on...
      for (std::size_t i = word_idx; i < word_count - 1; i++) {
        word_type next_word_exported_bits = bitmap[i + 1] << (word_bitlength - bits_to_remove);
        word_type imported_bits = (bitmap[i+1] & ((1ull << bits_to_remove + 1) - 1)) << (word_bitlength - bits_to_remove);
        bitmap[i] |= imported_bits;
        bitmap[i + 1] >>= bits_to_remove; 

        std::size_t current_lut_block = ((i * word_bitlength) / rank_block_granularity) + 1;
        std::size_t next_word_lut_block = (((i + 1) * word_bitlength) / rank_block_granularity) + 1;

        if (next_word_lut_block > current_lut_block) { 
          lut_update_vec[current_lut_block - lut_begin_block_idx] += bitmap_fn::count_set_bits(imported_bits);
          lut_update_vec[next_word_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(next_word_exported_bits);
        }
      } 
    }

    int lut_update_sum = 0;
    for (int i = 0; i < lut_update_vec.size(); i++) {
      lut_update_sum += lut_update_vec[i];
      rank_lut_ptr_[i + lut_begin_block_idx] += lut_update_sum;    
    }
  }

  void
  insert_into_tree(u32 begin_idx, std::size_t bits_to_insert, u1 val) {
    if (bits_to_insert == 0) return;
    const auto word_idx = begin_idx / word_bitlength;
    const auto bit_idx = begin_idx % word_bitlength;

    word_type* bitmap = get_tree_ptr(ptr_);
    std::size_t word_count = get_tree_word_cnt(ptr_);

    const std::size_t rank_block_granularity = rank_.get_block_bitlength();
    std::size_t all_bits = word_count * word_bitlength;
    std::size_t rank_lut_block_cnt = (all_bits / rank_block_granularity) + 1;
    if (all_bits % rank_block_granularity) rank_lut_block_cnt += 1;
    std::size_t lut_begin_block_idx = (begin_idx / rank_block_granularity) + 1;
    std::vector<int> lut_update_vec(rank_lut_block_cnt - lut_begin_block_idx, 0);

    // For now, assume there is always enough space
    // For LuT updates, assume block granularity is equal to or a multiple of word size

    if (bit_idx + bits_to_insert >= word_bitlength) {
      // Bits are inserted between two words
      std::size_t first_word_bits_to_insert = word_bitlength - bit_idx;
      std::size_t next_word_bits_to_shift = bits_to_insert - first_word_bits_to_insert;

      // Make space for insert starting from the last word, calculate updates for every affected LuT block while at it
      for (std::size_t i = word_count - 1; i > word_idx + 1; i--) {
        word_type exported_bits = bitmap[i] >> (word_bitlength - bits_to_insert);
        word_type imported_bits = bitmap[i - 1] >> (word_bitlength - bits_to_insert);
        bitmap[i] <<= bits_to_insert;
        bitmap[i] |= imported_bits;

        std::size_t current_lut_block = ((i * word_bitlength) / rank_block_granularity) + 1;
        lut_update_vec[current_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(exported_bits);
        lut_update_vec[current_lut_block - lut_begin_block_idx] += bitmap_fn::count_set_bits(imported_bits);
      }

      // Do the LuT updates manually for first word and the next, but they can be in the same block or be in different blocks
      std::size_t first_word_lut_block = ((word_idx * word_bitlength) / rank_block_granularity) + 1;
      std::size_t next_word_lut_block = (((word_idx + 1) * word_bitlength) / rank_block_granularity) + 1;

      word_type imported_bits = bitmap[word_idx]  >> (word_bitlength - first_word_bits_to_insert);
      word_type second_word_exported_bits = bitmap[word_idx + 1] >> (word_bitlength - bits_to_insert);

      // Update LuT if first and second affected words are in different blocks
      std::size_t bits_moved = bitmap_fn::count_set_bits(imported_bits);
      lut_update_vec[first_word_lut_block - lut_begin_block_idx] -= bits_moved;
      lut_update_vec[next_word_lut_block - lut_begin_block_idx] += bits_moved;
      lut_update_vec[next_word_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(second_word_exported_bits);

      bitmap[word_idx + 1] <<= first_word_bits_to_insert;
      bitmap[word_idx + 1] |= imported_bits;
      bitmap[word_idx + 1] <<= next_word_bits_to_shift;

      if (val) {
        bitmap_fn::set(bitmap, begin_idx, begin_idx + bits_to_insert);
        lut_update_vec[first_word_lut_block - lut_begin_block_idx] += first_word_bits_to_insert;
        lut_update_vec[next_word_lut_block - lut_begin_block_idx] += next_word_bits_to_shift;
      } else {
        bitmap_fn::clear(bitmap, begin_idx, begin_idx + bits_to_insert);
      }
    } else {
      // Shift the bitmap
      for (std::size_t i = word_count - 1; i > word_idx; i--) {
        word_type exported_bits = bitmap[i] >> (word_bitlength - bits_to_insert);
        word_type imported_bits = bitmap[i - 1] >> (word_bitlength - bits_to_insert);
        bitmap[i] <<= bits_to_insert;
        bitmap[i] |= imported_bits;

        std::size_t current_lut_block = ((i * word_bitlength) / rank_block_granularity) + 1;
        lut_update_vec[current_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(exported_bits);
        lut_update_vec[current_lut_block - lut_begin_block_idx] += bitmap_fn::count_set_bits(imported_bits);
      }
      
      // Count exported bits from first word if the next word is in a different LuT block
      std::size_t current_lut_block = ((word_idx * word_bitlength) / rank_block_granularity) + 1;
      //std::size_t prev_word_lut_block = (((word_idx + 1) * word_bitlength) / rank_block_granularity) + 1;
      lut_update_vec[current_lut_block - lut_begin_block_idx] -= bitmap_fn::count_set_bits(bitmap[word_idx] >> word_bitlength - bits_to_insert);
      //lut_update_vec[prev_word_lut_block - lut_begin_block_idx] += bitmap_fn::count_set_bits(bitmap[word_idx] >> word_bitlength - bits_to_insert);

      // Make space in the first word
      const word_type mask = (1ull << bit_idx) - 1;
      bitmap[word_idx] = ((bitmap[word_idx] & ~mask) << bits_to_insert) | (bitmap[word_idx] & mask);

      // Set the bits
      if (val) {
          bitmap_fn::set(bitmap, begin_idx, begin_idx + bits_to_insert);
          lut_update_vec[0] += bits_to_insert;
        } else {
          bitmap_fn::clear(bitmap, begin_idx, begin_idx + bits_to_insert);
        }
      } 

    int lut_update_sum = 0;
    #ifdef DEBUG_RANK_LUT
    std::cout << "LuT update vec:\n";
    for (int i = 0; i < lut_update_vec.size(); i++) {
      std::cout << rank_lut_ptr_[i + lut_begin_block_idx] << " + " << lut_update_vec[i] << " ";
      lut_update_sum += lut_update_vec[i];
      rank_lut_ptr_[i + lut_begin_block_idx] += lut_update_sum; 
      std::cout << "-> " << rank_lut_ptr_[i + lut_begin_block_idx] << "\n";
    }
    std::cout << std::endl;
    #else
    for (int i = 0; i < lut_update_vec.size(); i++) {
      lut_update_sum += lut_update_vec[i];
      rank_lut_ptr_[i + lut_begin_block_idx] += lut_update_sum;
    }
    #endif

    /*
    std::cout << "Rank LuT: ";
    for (std::size_t i = 0; i < get_rank_word_cnt(ptr_) * 2; i++) {
      std::cout << get_rank_ptr(ptr_)[i] << " ";
    }
    std::cout << "\n\n";
    
    display_T();
    check_LuT();
    */
  }

  void prepend_words_L(std::size_t words) {
    word_type* bitmap = get_label_ptr(ptr_);
    std::size_t explicit_word_count = label_bit_cnt_ / word_bitlength;
    if (label_bit_cnt_ % word_bitlength) explicit_word_count += 1;

    assert(get_label_word_cnt(ptr_) >= explicit_word_count + words);

    for (int i = explicit_word_count - 1; i >= 0; i--) {
      bitmap[i + words] = bitmap[i];
      bitmap[i] = 0;
    }
  }

  void prepend_words_T(std::size_t words) {
    word_type* bitmap = get_tree_ptr(ptr_);
    std::size_t explicit_word_count = tree_bit_cnt_ / word_bitlength;
    if (tree_bit_cnt_ % word_bitlength) explicit_word_count += 1;

    std::size_t rank_block_granularity = rank_.get_block_bitlength();
    std::size_t all_bits = get_tree_word_cnt(ptr_) * word_bitlength;
    std::size_t rank_lut_block_cnt = (all_bits / rank_block_granularity) + 1;
    if (all_bits % rank_block_granularity) rank_lut_block_cnt += 1;
    std::vector<int> lut_update_vec(rank_lut_block_cnt, 0);

    assert(get_tree_word_cnt(ptr_) >= explicit_word_count + words);

    for (int i = explicit_word_count - 1; i >= 0; i--) {
      //assert(bitmap[i + words] == 0);
      std::size_t current_lut_block = ((i * word_bitlength) / rank_block_granularity) + 1;
      std::size_t target_lut_block = (((i + words) * word_bitlength) / rank_block_granularity) + 1;

      std::size_t current_set_bits = bitmap_fn::count_set_bits(bitmap[i]);

      lut_update_vec[target_lut_block] += current_set_bits;
      lut_update_vec[current_lut_block] -= current_set_bits;

      bitmap[i + words] = bitmap[i];
      bitmap[i] = 0;
    }

    for (int i = 0; i < words; i++) {
      assert(bitmap[i] == 0);
      std::size_t current_lut_block = ((i * word_bitlength) / rank_block_granularity) + 1;
      lut_update_vec[current_lut_block] += word_bitlength;
      bitmap[i] = 0xFFFFFFFFFFFFFFFF;
    }

    int lut_update_sum = 0;
    for (int i = 0; i < rank_lut_block_cnt; i++) {
      lut_update_sum += lut_update_vec[i];
      rank_lut_ptr_[i] += lut_update_sum;    
    }
  }

  void
  display_T() {
    std::cout << "Tree:\n";
    word_type* tree_ptr = get_tree_ptr(ptr_);
    for (int i = 0; i < get_tree_word_cnt(ptr_); i++) {
        std::cout << std::bitset<64>(tree_ptr[i]) << " " << bitmap_fn::count_set_bits(tree_ptr[i]) << std::endl;
        if (i >= 9 && i % 10 == 0) std::cout << std::endl;
    }
  }

  void
  display_L() {
    std::cout << "Labels:\n";
    word_type* tree_ptr = get_label_ptr(ptr_);
    for (int i = 0; i < get_label_word_cnt(ptr_); i++) {
        std::cout << std::bitset<64>(tree_ptr[i]) << " " << bitmap_fn::count_set_bits(tree_ptr[i]) << std::endl;
        if (i >= 9 && i % 10 == 0) std::cout << std::endl;
    }
  }

  struct prune_log_entry {
    u64 node_idx;
    u64 left_child;
    u64 right_child;
    u1 label;
    u64 parent_label_idx;
    u1 T_children_implicit;
    u1 T_parent_implicit;
    const std::size_t T_materialize_cost;
    u1 L_children_explicit;
    u1 L_children_implicit_leading;
    u1 L_only_right_child_explicit;
    u1 L_children_implicit_trailing;
    u1 L_only_left_child_explicit;
    u1 L_parent_implicit_leading;
    u1 L_parent_implicit_trailing;
    const std::size_t L_materialize_cost;
    const std::size_t tree_bits;
    const std::size_t tree_free_bits;
    const std::size_t implicit_inner_node_count;
    const std::size_t implicit_leaf_count;
    const std::size_t label_bits;
    const std::size_t label_free_bits;
    const std::size_t implicit_leading_label_count;
    const std::size_t implicit_trailing_label_count;
    u1 T_materialize_failed;
    u1 L_materialize_failed;
  };

  void prune_report(std::vector<prune_log_entry> log) {
    std::cout << "Detected " << log.size() << " entries\n";

    std::size_t inner_nodes_visited = 0;

    std::size_t parent_is_explicit = 0;
    std::size_t children_are_explicit = 0;
    std::size_t parent_is_implicit = 0;
    std::size_t children_are_implicit = 0;

    std::size_t implicit_leading_children_labels = 0;
    std::size_t implicit_trailing_children_labels = 0;
    std::size_t explicit_children_labels = 0;
    std::size_t left_label_implicit = 0;
    std::size_t right_label_implicit = 0;

    std::size_t leading_parent_label = 0;
    std::size_t explicit_parent_label = 0;
    std::size_t trailing_parent_label = 0;

    std::size_t T_word_inserts = 0;
    std::size_t L_word_inserts = 0;
    std::size_t T_prune_fail = 0;
    std::size_t L_prune_fail = 0;

    for (prune_log_entry entry : log) {
      inner_nodes_visited++;
      if (!entry.T_materialize_failed && !entry.L_materialize_failed) {
        (entry.T_parent_implicit) ? parent_is_implicit++ : parent_is_explicit++;
        (entry.T_children_implicit) ? children_are_implicit++ : children_are_explicit++;
        implicit_leading_children_labels += entry.L_children_implicit_leading;
        implicit_trailing_children_labels += entry.L_children_implicit_trailing;
        explicit_children_labels += entry.L_children_explicit;
        left_label_implicit += entry.L_only_left_child_explicit;
        right_label_implicit += entry.L_only_right_child_explicit;
        leading_parent_label += entry.L_parent_implicit_leading;
        trailing_parent_label += entry.L_parent_implicit_trailing;
        explicit_parent_label += (!entry.L_parent_implicit_leading && !entry.L_parent_implicit_trailing);
        T_word_inserts += (entry.T_materialize_cost > word_bitlength) ? 1 : 0;
        L_word_inserts += (entry.L_materialize_cost > word_bitlength) ? 1 : 0;
      } else {
        T_prune_fail += (entry.T_materialize_failed) ? 1 : 0;
        L_prune_fail += (entry.L_materialize_failed) ? 1 : 0;
      }
      
    }
    std::cout << "Visited " << inner_nodes_visited << " inner nodes\n";
    std::cout << "Explicit nodes set to leaf: " << parent_is_explicit << std::endl;
    std::cout << "Implicit nodes set to leaf: " << parent_is_implicit << std::endl;
    std::cout << "Explicit leaf pairs pruned: " << children_are_explicit << std::endl;
    std::cout << "Implicit leaf pairs pruned: " << children_are_implicit << std::endl;
    std::cout << "Implicit leading children label pairs pruned: " << implicit_leading_children_labels << std::endl;
    std::cout << "Implicit trailing children label pairs pruned: " << implicit_trailing_children_labels << std::endl;
    std::cout << "Explicit children label pairs pruned: " << explicit_children_labels << std::endl;
    std::cout << "Cases where only left child label is implicit: " << left_label_implicit << std::endl;
    std::cout << "Cases where only right child label is implicit: " << right_label_implicit << std::endl;
    std::cout << "Inserted " << leading_parent_label << " leading label bits\n";
    std::cout << "Inserted " << explicit_parent_label << " explicit label bits\n";
    std::cout << "Inserted " << trailing_parent_label << " trailing label bits\n";
    std::cout << "No. of times prepend_words_T() was called: " << T_word_inserts << std::endl;
    std::cout << "No. of times prepend_words_L() was called: " << L_word_inserts << std::endl;
    std::cout << "Failed prunes due to not enough free bits in T: " << T_prune_fail << std::endl;
    std::cout << "Failed prunes due to not enough free bits in L: " << L_prune_fail << std::endl;
  }

  void
  prune(bool debug = false) {
    std::size_t teb_max_height = encoded_tree_height_;
    std::vector<int> ofs_tree_update_vec(teb_max_height, 0);
    std::vector<int> ofs_labels_update_vec(teb_max_height, 0);
    std::size_t max_height_observed = 0;

    std::vector<prune_log_entry> prune_log;

    prune_traverse(0, 0, ofs_tree_update_vec, ofs_labels_update_vec, max_height_observed, &prune_log);

    // Update level offset tables
    int T_update_sum = 0;
    int L_update_sum = 0;
    for (std::size_t i = 0; i < teb_max_height - 1; i++) {
      T_update_sum += ofs_tree_update_vec[i];
      L_update_sum += ofs_labels_update_vec[i];

      level_offsets_tree_lut_ptr_[i + 1] += T_update_sum;
      level_offsets_labels_lut_ptr_[i + 1] += L_update_sum;
    }
    /*
    if (max_height_observed + 1 != encoded_tree_height_) {
      encoded_tree_height_ = max_height_observed + 1;
    }
    */

    #ifdef DEBUG
    std::cout << "Max height observed: " << max_height_observed << std::endl;
    if (max_height_observed + 1 != encoded_tree_height_) {
      std::cout << "Detected change in encoded tree height.\n"; 
    }

    std::cout << "Tree offset update vec: ";
    for (std::size_t i = 0; i < teb_max_height; i++) {
      std::cout << ofs_tree_update_vec[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "Label offset update vec: ";
    for (std::size_t i = 0; i < teb_max_height; i++) {
      std::cout << ofs_labels_update_vec[i] << " ";
    }
    std::cout << std::endl;
    prune_report(prune_log);
    std::cout << std::endl;
    #endif

    // Trim T
    word_type* T_bitmap = get_tree_ptr(ptr_);
    /*
    std::uint64_t T_first_zero_idx = bitmap_fn::find_next_zero(T_bitmap, 0, tree_bit_cnt_);
    //For some reason find_next_zero can't detect a 0 at index 0
    if (T_bitmap[0] % 2 == 0) T_first_zero_idx = 0;
    if (T_first_zero_idx > 0 && T_first_zero_idx < word_bitlength) {
      std::cout << "Trimming " << T_first_zero_idx << " bits from T.\n";
      remove_from_tree(0, T_first_zero_idx);
      tree_bit_cnt_ -= T_first_zero_idx;
      free_bits_T_ += T_first_zero_idx;
      implicit_inner_node_cnt_ += T_first_zero_idx;
    }
    */
    u64 T_last_one_idx = bitmap_fn::find_last(T_bitmap, &T_bitmap[get_tree_word_cnt(ptr_)]);
    u64 T_max_idx = tree_bit_cnt_ - 1;
    if (T_last_one_idx < T_max_idx) {
      //display_T();
      //std::cout << "\nTree bits: " << tree_bit_cnt_ << " Last one idx: " << T_last_one_idx << "\n";
      std::size_t bits_to_trim = T_max_idx - T_last_one_idx;
      tree_bit_cnt_ -= bits_to_trim;
      free_bits_T_ += bits_to_trim;
      implicit_trailing_leaf_cnt_ += bits_to_trim;
      //std::cout << "Trimmed " << bits_to_trim << " bits from T.\n";
      //display_T();
    }

    // Trim L
    word_type* L_bitmap = get_label_ptr(ptr_);
    /*
    */
    u64 L_first_one_idx = bitmap_fn::find_first(L_bitmap, &L_bitmap[get_label_word_cnt(ptr_)]);
    if (L_first_one_idx > 0 && L_first_one_idx < word_bitlength) {
      display_L();
      std::cout << "Trimming " << L_first_one_idx << " leading bits from L.\n";
      remove_from_labels(0, L_first_one_idx);
      display_L();
      label_bit_cnt_ -= L_first_one_idx;
      free_bits_L_ += L_first_one_idx;
      implicit_leading_label_cnt_ += L_first_one_idx;
    }

    u64 L_max_idx = label_bit_cnt_ - 1;
    u64 L_last_one_idx = bitmap_fn::find_last(L_bitmap, &L_bitmap[get_label_word_cnt(ptr_)]);
    if (L_last_one_idx < L_max_idx) {
      std::size_t bits_to_trim = L_max_idx - L_last_one_idx;
      label_bit_cnt_ -= bits_to_trim;
      free_bits_L_ += bits_to_trim;
      implicit_trailing_label_cnt_ += bits_to_trim;
    }
  }
  
  void
  prune_traverse(
      u64 node_idx, 
      std::size_t level, 
      std::vector<int> &ofs_T_update_vec, 
      std::vector<int> &ofs_L_update_vec, 
      std::size_t &max_height_observed,
      std::vector<prune_log_entry> *prune_log = nullptr) {
    std::size_t max_node_idx = implicit_inner_node_cnt_ + tree_bit_cnt_ + implicit_trailing_leaf_cnt_ - 1;
    if (node_idx >= max_node_idx) {
      return;
    }

    if (is_inner_node(node_idx)) {

      // Traverse right subtree
      u64 right_child = 2 * rank_inclusive(node_idx);
      assert(right_child < implicit_inner_node_cnt_ + tree_bit_cnt_ + implicit_trailing_leaf_cnt_);
      bool right_child_is_inner = is_inner_node(right_child);
      if (right_child_is_inner) {
        prune_traverse(right_child, level + 1, ofs_T_update_vec, ofs_L_update_vec, max_height_observed, prune_log);
      }
      right_child_is_inner = is_inner_node(right_child);
      
      // Traverse left subtree
      u64 left_child = 2 * rank_inclusive(node_idx) - 1;
      assert(left_child < implicit_inner_node_cnt_ + tree_bit_cnt_ + implicit_trailing_leaf_cnt_);
      bool left_child_is_inner = is_inner_node(left_child);
      if (left_child_is_inner) {
        prune_traverse(left_child, level + 1, ofs_T_update_vec, ofs_L_update_vec, max_height_observed, prune_log);
      }
      left_child_is_inner = is_inner_node(left_child);

      if (!right_child_is_inner && !left_child_is_inner) {
        u1 right_label = get_label(right_child);
        u1 left_label = get_label(left_child);

        if (right_label == left_label) {
          u1 label = right_label;

          uint64_t implicit_trailing_T_begin = implicit_inner_node_cnt_ + tree_bit_cnt_;
          uint64_t implicit_trailing_L_begin = implicit_leading_label_cnt_ + label_bit_cnt_;

          // Check if current node is implicit; only leafs can be trailing implicit
          u1 current_is_implicit = node_idx < implicit_inner_node_cnt_;

          // Check whether children are implicit; leafs can only be trailing implicit
          u1 left_child_is_implicit = left_child >= implicit_trailing_T_begin;
          u1 right_child_is_implicit = right_child >= implicit_trailing_T_begin;
          u1 children_are_implicit = left_child_is_implicit && right_child_is_implicit;

          // Check whether children label bits are implicit
          u64 left_label_idx = get_label_idx(left_child);
          u64 right_label_idx = left_label_idx + 1;
          assert(left_label_idx < implicit_leading_label_cnt_ + label_bit_cnt_ + implicit_trailing_label_cnt_);
          assert(right_label_idx < implicit_leading_label_cnt_ + label_bit_cnt_ + implicit_trailing_label_cnt_);

          bool left_label_is_leading = left_label_idx < implicit_leading_label_cnt_;
          bool right_label_is_leading = right_label_idx < implicit_leading_label_cnt_;

          bool left_label_is_trailing = left_label_idx >= implicit_trailing_L_begin;
          bool right_label_is_trailing = right_label_idx >= implicit_trailing_L_begin;

          bool children_labels_are_leading = left_label_is_leading && right_label_is_leading;
          bool children_labels_are_trailing = left_label_is_trailing && right_label_is_trailing;
          bool children_labels_are_implicit = children_labels_are_leading || children_labels_are_trailing;

          // Check whether the would be label of the parent would end up in the "implicit zone"
          u64 current_label_idx = get_future_label_idx(node_idx);
          bool current_label_is_leading = current_label_idx < implicit_leading_label_cnt_;
          bool current_label_is_trailing = current_label_idx >= implicit_trailing_L_begin;
          bool current_label_is_implicit = current_label_is_leading || current_label_is_trailing;

          // Cost to update, relevant if parent is implicit (in T or L or both)
          std::size_t cost_T = 0;
          std::size_t cost_L = 0;

          if (current_is_implicit) {
            cost_T += implicit_inner_node_cnt_ - node_idx;
          }

          if (current_label_is_implicit && label) {
            if (current_label_is_leading) {
              cost_L += implicit_leading_label_cnt_ - current_label_idx + 1;
            }
            if (current_label_is_trailing) {
              cost_L += current_label_idx - implicit_trailing_L_begin + 1;
            }
          }

          // Heuristic: check if prune reduces number of perfect levels
          bool perfect_levels_reduced = level + 1 < perfect_level_cnt_;
          perfect_levels_reduced = false;

          #ifdef DEBUG
          std::cout << "Visiting node " << node_idx << std::endl;
          std::cout << "Right Child id: " << right_child << " " << right_child_is_inner << " " << get_label(right_child) <<  std::endl;
          std::cout << "Left Child id: " << left_child << " " << left_child_is_inner << " " << get_label(left_child) << std::endl;
          std::cout << "Prunable leafs found at " << left_child << ", level " << level + 1 << std::endl;

          if (current_is_implicit) {
            std::cout << "Setting parent to leaf requires " << cost_T << " new bits\n";
            std::cout << "There are " << free_bits_T_ << " free bits in T\n";
            if (cost_T > free_bits_T_) {
              std::cout << "There are not enough bits in T\n";
            } else {
              std::cout << "There are enough bits in T\n";
            }
          }

          if (current_label_is_implicit && label) {
            std::cout << "Inserting parent label requires " << cost_L << " new bits\n";
            std::cout << "There are " << free_bits_L_ << " free bits in L\n";
            if (cost_L > free_bits_L_) {
              std::cout << "There are not enough bits in L\n";
            } else {
              std::cout << "There are enough bits in L\n";
            } 
          }
          #endif
          
          //if (cost_T <= free_bits_T_ && cost_L <= free_bits_L_ && cost_L < 64 && cost_T < 64)
          if (cost_T <= free_bits_T_ && cost_L <= free_bits_L_ && !perfect_levels_reduced) {
            #ifdef DEBUG
            /*
            boost::dynamic_bitset<u_int32_t> test_bitset(n_);
            for (std::size_t i = 0; i < n_; i++) {
              test_bitset[i] = test(i);
            }*/
            #endif
            std::size_t T_bits_before = implicit_inner_node_cnt_ + tree_bit_cnt_ + implicit_trailing_leaf_cnt_;
            std::size_t L_bits_before = implicit_leading_label_cnt_ + label_bit_cnt_ + implicit_trailing_label_cnt_;

            // Remove children label bits from L
            if (!children_labels_are_implicit) {
              if (left_label_is_leading && !right_label_is_leading) {
                remove_from_labels(0, 1);
                free_bits_L_ += 1;
                label_bit_cnt_ -= 1;
                implicit_leading_label_cnt_ -= 1;
              } else if (!left_label_is_trailing && right_label_is_trailing) {
                bitmap_fn::clear(get_label_ptr(ptr_), label_bit_cnt_ - 1);
                free_bits_L_ += 1;
                label_bit_cnt_ -= 1;
                implicit_trailing_label_cnt_ -= 1;
              } else {
                remove_from_labels(left_label_idx - implicit_leading_label_cnt_, 2);
                label_bit_cnt_ -= 2;
                free_bits_L_ += 2;
              }
            } else {
              if (children_labels_are_leading) {
                implicit_leading_label_cnt_ -= 2;
              }
              if (children_labels_are_trailing) {
                implicit_trailing_label_cnt_ -= 2;
              }
            }

            // Update label level offset LuT
            ofs_L_update_vec[level + 1] -= 2;
            
            // Remove children tree bits from T
            if (children_are_implicit) {
              // Both leaves are implicit
              implicit_trailing_leaf_cnt_ -= 2;
            } else {
              // Left child is explicit while right child is trailing
              if (!left_child_is_implicit && right_child_is_implicit) {
                implicit_trailing_leaf_cnt_ -= 1;
                tree_bit_cnt_ -= 1;
                free_bits_T_ += 1;
              } else {
                // Both are explicit
                remove_from_tree(left_child - implicit_inner_node_cnt_, 2);
                tree_bit_cnt_ -= 2;
                free_bits_T_ += 2;
              }
            }

            // Update tree level offset LuT
            ofs_T_update_vec[level + 1] -= 2;

            // If pruning happens at a perfect level, decrease number of perfect levels
            if (level + 1 < perfect_level_cnt_) {
              perfect_level_cnt_ -= 1;
            }

            std::size_t rank_block_granularity = rank_.get_block_bitlength();
            std::size_t all_bits = get_tree_word_cnt(ptr_) * word_bitlength;
            std::size_t rank_lut_block_cnt = (all_bits / rank_block_granularity) + 1;
            if (all_bits % rank_block_granularity) rank_lut_block_cnt += 1;
            std::size_t lut_begin = ((node_idx - implicit_inner_node_cnt_) / rank_block_granularity) + 1;
            if (!current_is_implicit) {
              // Set current (inner) node to a leaf node, update LuT
              bitmap_fn::clear(get_tree_ptr(ptr_), node_idx - implicit_inner_node_cnt_);
              for (std::size_t i = lut_begin; i < rank_lut_block_cnt; i++) {
                rank_lut_ptr_[i] -= 1;
              }
            } else {
              // Insert new bits into T
              if (cost_T >= 64) {
                std::size_t words_to_insert = cost_T / word_bitlength;
                std::size_t bits_to_insert = cost_T % word_bitlength;
                insert_into_tree(0, bits_to_insert, 1);
                free_bits_T_ -= bits_to_insert;
                implicit_inner_node_cnt_ -= bits_to_insert;
                tree_bit_cnt_ += bits_to_insert;
                prepend_words_T(words_to_insert);
                free_bits_T_ -= words_to_insert * word_bitlength;
                implicit_inner_node_cnt_ -= words_to_insert * word_bitlength;
                tree_bit_cnt_ += words_to_insert * word_bitlength;
              } else {
                insert_into_tree(0, cost_T, 1);
                free_bits_T_ -= cost_T;
                implicit_inner_node_cnt_ -= cost_T;
                tree_bit_cnt_ += cost_T;
              }
              
              // Set current (inner) node to a leaf node, update LuT
              bitmap_fn::clear(get_tree_ptr(ptr_), 0);
              for (std::size_t i = 1; i < rank_lut_block_cnt; i++) {
                rank_lut_ptr_[i] -= 1;
              }
            }

            // Insert label bit of parent into
            if (!current_label_is_implicit) {
              insert_into_labels(current_label_idx - implicit_leading_label_cnt_, 1, label);
              label_bit_cnt_ += 1;
              free_bits_L_ -= 1;
            } else {
              if (label) {
                if (current_label_is_leading) {
                  if (cost_L >= 64) {
                    std::size_t words_to_insert = cost_L / word_bitlength;
                    std::size_t bits_to_insert = cost_L % word_bitlength;
                    insert_into_labels(0, bits_to_insert, 0);
                    free_bits_L_ -= bits_to_insert;
                    implicit_leading_label_cnt_ -= bits_to_insert - 1;
                    label_bit_cnt_ += bits_to_insert;
                    prepend_words_L(words_to_insert);
                    free_bits_L_ -= words_to_insert * word_bitlength;
                    implicit_leading_label_cnt_ -= words_to_insert * word_bitlength;
                    label_bit_cnt_ += words_to_insert * word_bitlength;
                  } else {
                    insert_into_labels(0, cost_L, 0);
                    free_bits_L_ -= cost_L;
                    implicit_leading_label_cnt_ -= cost_L - 1;
                    label_bit_cnt_ += cost_L;
                  }
                  bitmap_fn::set(get_label_ptr(ptr_), 0);
                } else {
                  //insert_into_labels(label_bit_cnt_, cost_L, 0);
                  free_bits_L_ -= cost_L;
                  implicit_trailing_label_cnt_ -= cost_L - 1;
                  label_bit_cnt_ += cost_L;
                  bitmap_fn::set(get_label_ptr(ptr_), label_bit_cnt_ + cost_L - 1);
                }
              } else {
                if (current_label_is_leading) {
                  implicit_leading_label_cnt_ += 1;
                } else {
                  implicit_trailing_label_cnt_ += 1;
                }
              }
            }
            // Update label level offset table
            ofs_L_update_vec[level] += 1;

            // Update height
            if (level > max_height_observed) max_height_observed = level;

            #ifdef DEBUG
            // Debugging & Logging
            if (!children_labels_are_implicit) {
              std::cout << "Label idx of left child: " << left_label_idx << "\n";
              if (left_label_is_leading && !right_label_is_leading) {
                std::cout << "Left child label is implicit, but right child is explicit\n";
              } else if (!left_label_is_trailing && right_label_is_trailing) {
                std::cout <<  "Right child label is implicit, but left child is explicit\n";
              } else {
                std::cout << "Children labels are explicit\n";
              }
            } else {
              if (children_labels_are_leading) {
                std::cout << "Children labels are leading implicit\n";
              }
              if (children_labels_are_trailing) {
                std::cout << "Children labels are trailing implicit\n";
              }
            }
            
            // Remove children tree bits from T
            if (children_are_implicit) {
              // Both leaves are implicit
              std::cout << "Both children are implicit\n";
            } else {
              // Both are explicit
              if (!left_child_is_implicit && right_child_is_implicit) {
                std::cout << "Only left child is explicit\n";
              } else {
                std::cout << "Both children are explicit\n";
              }
            }
            // Insert label bit of parent into
            std::cout << "Calculated parent label idx: " << current_label_idx << std::endl;
            if (!current_label_is_implicit) {

            } else {
              if (label) {
                if (current_label_is_leading) {
                  std::cout << "Parent 1 label is leading implicit\n";
                } else {
                  std::cout << "Parent 1 label is trailing implicit\n";
                }
              } else {
                if (current_label_is_leading) {
                  std::cout << "Parent 0 label is leading implicit\n";
                } else {
                  std::cout << "Parent 0 label is trailing implicit\n";
                }
              }
            }

            std::cout << "Tree bits: " << tree_bit_cnt_ << std::endl;
            std::cout << "Total bits: " << get_tree_word_cnt(ptr_) * word_bitlength << std::endl;
            std::cout << "Label bits: " << label_bit_cnt_ << std::endl;
            std::cout << "Leading inner nodes: " << implicit_inner_node_cnt_ << std::endl;
            std::cout << "Trailing leafs: " << implicit_trailing_leaf_cnt_ << std::endl;
            std::cout << "Leading 0 labels: " << implicit_leading_label_cnt_ << std::endl;
            std::cout << "Trailing 0 labels: " << implicit_trailing_label_cnt_ << std::endl;
            std::cout << "Label ID after setting parent to leaf: " << get_label_idx(node_idx) << std::endl;
            //display_T();
            //display_L();
            /*
            std::cout << "Rank LuT: ";
            for (std::size_t i = 0; i < get_rank_word_cnt(ptr_) * 2; i++) {
              std::cout << get_rank_ptr(ptr_)[i] << " ";
            }
            std::cout << "\n"; */
            std::cout << "Tree offset update vec: ";
            for (std::size_t i = 0; i < encoded_tree_height_; i++) {
              std::cout << ofs_T_update_vec[i] << " ";
            }
            std::cout << std::endl;

            std::cout << "Label offset update vec: ";
            for (std::size_t i = 0; i < encoded_tree_height_; i++) {
              std::cout << ofs_L_update_vec[i] << " ";
            }
            std::cout << std::endl;

            std::cout << "Tree offset table: ";
            for (std::size_t i = 0; i < encoded_tree_height_; i++) {
              std::cout << level_offsets_tree_lut_ptr_[i] << " ";
            }
            std::cout << std::endl;

            std::cout << "Label offset table: ";
            for (std::size_t i = 0; i < encoded_tree_height_; i++) {
              std::cout << level_offsets_labels_lut_ptr_[i] << " ";
            }
            std::cout << "\n\n";

            prune_log_entry log_entry{
              node_idx,
              left_child,
              right_child,
              left_label,
              current_label_idx,
              children_are_implicit,
              current_is_implicit,
              cost_T,
              !children_labels_are_implicit,
              children_labels_are_leading,
              right_label_is_trailing && !left_label_is_trailing,
              children_labels_are_trailing,
              left_label_is_leading && !right_label_is_leading,
              current_label_is_leading,
              current_label_is_trailing,
              cost_L,
              tree_bit_cnt_,
              free_bits_T_,
              implicit_inner_node_cnt_,
              implicit_trailing_leaf_cnt_,
              label_bit_cnt_,
              free_bits_L_,
              implicit_leading_label_cnt_,
              implicit_trailing_label_cnt_,
              0,
              0
            };

            assert(prune_log != nullptr);
            prune_log->push_back(log_entry);
            /*
            for (std::size_t i = 0; i < n_; i++) {
              assert(test_bitset[i] == test(i));
            }*/

            #endif

            assert(implicit_inner_node_cnt_ + tree_bit_cnt_ + implicit_trailing_leaf_cnt_ == T_bits_before - 2);
            assert(implicit_leading_label_cnt_ + label_bit_cnt_ + implicit_trailing_label_cnt_ == L_bits_before - 1);
            assert(free_bits_L_ == (get_label_word_cnt(ptr_) * word_bitlength) - label_bit_cnt_);
            assert(free_bits_T_ == (get_tree_word_cnt(ptr_) * word_bitlength) - tree_bit_cnt_);
            std::cout.flush();
            check_LuT();
          } else {
            // Failed pruning
            if (level + 1 > max_height_observed) max_height_observed = level + 1;
            #ifdef DEBUG
            std::cout << std::endl;
            prune_log_entry log_entry{
              node_idx,
              left_child,
              right_child,
              left_label,
              current_label_idx,
              children_are_implicit,
              current_is_implicit,
              cost_T,
              !children_are_implicit,
              children_labels_are_leading,
              right_label_is_trailing && !left_label_is_trailing,
              children_labels_are_trailing,
              left_label_is_leading && !right_label_is_leading,
              current_label_is_leading,
              current_label_is_trailing,
              cost_L,
              tree_bit_cnt_,
              free_bits_T_,
              implicit_inner_node_cnt_,
              implicit_trailing_leaf_cnt_,
              label_bit_cnt_,
              free_bits_L_,
              implicit_leading_label_cnt_,
              implicit_trailing_label_cnt_,
              cost_T > free_bits_T_,
              cost_L > free_bits_L_
            };
            prune_log->push_back(log_entry);
            #endif
          }
        } else {
          // Children nodes have different labels, update height
          if (level + 1 > max_height_observed) max_height_observed = level + 1;
        }
      } else {
        // Children are an inner node and leaf pair
        if (level + 1 > max_height_observed) max_height_observed = level + 1;
      }
    }
  }

  // Checks consistency between 1-bits in T and Rank LuT
  void check_LuT() {
    std::size_t set_bits_count = 0;
    std::size_t prev_lut_block = 1;
    std::size_t rank_block_granularity = rank_.get_block_bitlength();
    for (std::size_t i = 0; i < get_tree_word_cnt(ptr_); i++) {
      std::size_t current_lut_block = ((i * word_bitlength) / rank_block_granularity) + 1;
      if (current_lut_block > prev_lut_block) {
        #ifdef DEBUG_RANK_LUT
        //std::cout << "Set: " << set_bits_count << ", Rank: " << rank_lut_ptr_[prev_lut_block] << "\n";
        #endif
        std::cout.flush();
        assert(set_bits_count == rank_lut_ptr_[prev_lut_block]);
        prev_lut_block += 1;
      }
      set_bits_count += bitmap_fn::count_set_bits(tree_ptr_[i]);
    }
  }

  // Checks whether last 1-bit in T is considered explicit
  void check_T() {
    assert(bitmap_fn::find_last(get_tree_ptr(ptr_), get_tree_ptr(ptr_) + get_tree_word_cnt(ptr_) - 1) < tree_bit_cnt_);
  }

  /// Return the size in bytes.
  std::size_t __teb_inline__
  size_in_bytes() const noexcept {
    std::size_t word_cnt = 0;
    word_cnt += get_header_word_cnt(ptr_);
    word_cnt += get_tree_word_cnt(ptr_);
    word_cnt += get_rank_word_cnt(ptr_);
    word_cnt += get_label_word_cnt(ptr_);
    word_cnt += get_metadata_word_cnt(ptr_);
    return word_cnt * word_size;
  }

  /// For debugging purposes.
  void __forceinline__
  print(std::ostream& os) const noexcept {
    os << "implicit inner nodes = "
       << implicit_inner_node_cnt_
       << ", implicit trailing leafs = "
       << implicit_trailing_leaf_cnt_
       << ", implicit leading labels = "
       << implicit_leading_label_cnt_
       << ", implicit trailing labels = "
       << implicit_trailing_label_cnt_
       << ", perfect levels = "
       << perfect_level_cnt_
       << ", tree bits = " << tree_bit_cnt_
       << ", label bits = " << label_bit_cnt_
       << ", n = " << n_
       << ", n_actual = " << n_actual_
       << ", encoded tree height = " << encoded_tree_height_
       << ", rank size = " << (get_rank_word_cnt(ptr_) * word_size)
       << ", size = " << size_in_bytes()
       << "\n | ";

    if (implicit_inner_node_cnt_ > 0) {
      os << "'";
    }
    for ($i64 i = 0; i < tree_bit_cnt_; i++) {
      os << (T_[i] ? "1" : "0");
    }
    os << "\n | ";
    if (implicit_leading_label_cnt_ > 0) {
      os << "'";
    }
    for ($i64 i = 0; i < label_bit_cnt_; i++) {
      os << (L_[i] ? "1" : "0");
    }
    os << "\n";
    if (has_level_offsets()) {
      os << "Level offsets (tree):   ";
      for (size_type l = 0; l < encoded_tree_height_; ++l) {
        os << l << "=" << get_level_offset_tree(l) << " ";
      }
      os << "\n";
      os << "Level offsets (labels): ";
      for (size_type l = 0; l < encoded_tree_height_; ++l) {
        os << l << "=" << (l < perfect_level_cnt_ ? 0 : get_level_offset_labels(l)) << " ";
      }
    }
  }

//private:
public: // TODO revert
  /// Computes the (inclusive) rank of the given tree node.
  size_type __teb_inline__
  rank_inclusive(size_type node_idx) const noexcept {
    const auto implicit_1bit_cnt = implicit_inner_node_cnt_;
    if (node_idx < implicit_1bit_cnt) {
      return node_idx + 1;
    }
    assert(tree_bit_cnt_ > 0);
    const auto i = std::min(node_idx - implicit_1bit_cnt, tree_bit_cnt_ - 1);
    const auto r = rank_type::get(rank_lut_ptr_, i, tree_ptr_);
    const auto ret_val = implicit_1bit_cnt + r;
    return ret_val;
  }

  /// Returns true if the given node is an inner node, false otherwise.
  u1 __teb_inline__
  is_inner_node(size_type node_idx) const noexcept {
    assert(node_idx < 2 * n_);
    const auto implicit_1bit_cnt = implicit_inner_node_cnt_;
    const auto implicit_leaf_begin = tree_bit_cnt_ + implicit_inner_node_cnt_;
    if (node_idx < implicit_1bit_cnt) {
      // Implicit inner node.
      return true;
    }
    if (node_idx >= implicit_leaf_begin) {
      // Implicit leaf node.
      return false;
    }
    return bitmap_fn::test(tree_ptr_, node_idx - implicit_1bit_cnt);
  }

  /// Returns true if the given node is a leaf node, false otherwise.
  u1 __teb_inline__
  is_leaf_node(size_type node_idx) const noexcept {
    return !is_inner_node(node_idx);
  }

  /// Returns the label index for the given node.
  size_type __teb_inline__
  get_label_idx(size_type node_idx) const noexcept {
    return node_idx - rank_inclusive(node_idx);
  }

  /// Used in prune_traverse: returns would be label index for the given node (which is not an inner node)
  size_type __teb_inline__
  get_future_label_idx(size_type node_idx) const noexcept {
    return node_idx - (rank_inclusive(node_idx) - 1);
  }

  /// Returns the label at index.
  u1 __teb_inline__
  get_label_by_idx(size_type label_idx) const noexcept {
    const auto implicit_leading_label_cnt = implicit_leading_label_cnt_;
    const auto implicit_trailing_labels_begin =
        label_bit_cnt_ + implicit_leading_label_cnt;
    if (label_idx < implicit_leading_label_cnt) {
      // An implicit leading 0-label.
      return false;
    }
    if (label_idx >= implicit_trailing_labels_begin) {
      // An implicit trailing 0-label.
      return false;
    }
    return bitmap_fn::test(label_ptr_, label_idx - implicit_leading_label_cnt);
  }

  /// Returns the label of the given node.
  u1 __teb_inline__
  get_label(size_type node_idx) const noexcept {
    const auto label_idx = get_label_idx(node_idx);
    return get_label_by_idx(label_idx);
  }

  /// Returns true if this instance contains the offsets of the individual
  /// tree levels. This is required by the tree scan iterator. Typically, the
  /// offsets aren't present when the tree structure is very small.
  u1 __teb_inline__
  has_level_offsets() const noexcept {
    return hdr_->has_level_offsets;
  }

  /// Returns the position in T where the first tree node of level is stored.
  size_type __teb_inline__
  get_level_offset_tree(size_type level) const noexcept {
    assert(has_level_offsets());
    assert(level < encoded_tree_height_);
    return level_offsets_tree_lut_ptr_[level];
    /*
    if (level < perfect_level_cnt_) {
      return dtl::binary_tree_structure::first_node_idx_at_level(level);
    }
    else {
      return level_offsets_tree_lut_ptr_[level - perfect_level_cnt_];
    } */
  }

  /// Returns the position in L where the first label of level is stored.
  size_type __teb_inline__
  get_level_offset_labels(size_type level) const noexcept {
    assert(has_level_offsets());
    assert(level < encoded_tree_height_);
    return level_offsets_labels_lut_ptr_[level];
    /*
    if (level < perfect_level_cnt_) {
      return 0;
    }
    else {
      return level_offsets_labels_lut_ptr_[level - perfect_level_cnt_];
    } */
  }
};
//===----------------------------------------------------------------------===//
} // namespace dtl
