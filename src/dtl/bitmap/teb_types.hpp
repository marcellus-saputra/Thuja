#pragma once
//===----------------------------------------------------------------------===//
#include "util/rank1.hpp"
#include "util/rank1_logic_surf.hpp"

#include <dtl/dtl.hpp>
//===----------------------------------------------------------------------===//
#if defined(TEB_NO_INLINE) && !defined(__teb_inline__)
#define __teb_inline __attribute__((noinline))
#warning Inlining disabled.
#endif

#if !defined(__teb_inline__)
#if defined(NDEBUG)
// Release build.
#define __teb_inline__ inline __attribute__((always_inline))
#else
#define __teb_inline__
#endif
#endif
//===----------------------------------------------------------------------===//
namespace dtl {
//===----------------------------------------------------------------------===//
/// The size type.
using teb_size_type = $u32;
/// The storage type. The size of a TEB is a multiple of sizeof(teb_word_type).
using teb_word_type = $u64;
/// The rank1 logic.
using teb_rank_logic_type = dtl::rank1_logic_surf<teb_word_type, true>;
/// Support data structure for rank1 operations on the tree structure.
using teb_rank_type = dtl::rank1<teb_rank_logic_type>;
//===----------------------------------------------------------------------===//
#pragma pack(push, 1)
/// The header of a TEB.
struct teb_header {
  /// The number of bits in the bitmap.
  teb_size_type n = 0;
  /// The length of the encoded tree.
  teb_size_type tree_bit_cnt = 0;
  /// The number of implicit inner nodes in the tree structure.
  teb_size_type implicit_inner_node_cnt = 0;
  /// THe number of trailing inner nodes in the tree structure
  teb_size_type trailing_inner_node_cnt = 0;
  /// The number of label bits.
  teb_size_type label_bit_cnt = 0;
  /// The number of implicit leading 0-labels.
  teb_size_type implicit_leading_label_cnt = 0;
  /// The number of implicit trailing 0-labels.
  teb_size_type implicit_trailing_label_cnt = 0;
  /// The number of perfect levels.
  $u8 perfect_level_cnt = 0; // FIXME redundant, as is can be computed from the number of implicit inner nodes
  /// The height of the encoded (pruned) tree.
  $u8 encoded_tree_height = 0;
  /// True if the TEB contains level offsets at the very end.
  $u1 has_level_offsets = false;
  /// The number of updates since last pruning
  teb_size_type update_counter = 0;
  /// The number of updates to trigger pruning
  teb_size_type update_threshold = 0;
  /// The number of free bits available for updates
  teb_size_type free_bits_T = 0;
  teb_size_type free_bits_L = 0;
  /// Padding.
  $u8 padding = 0;
};
#pragma pack(pop)
//===----------------------------------------------------------------------===//
static_assert(sizeof(teb_header) == 48,
    "A TEB header is supposed to be 48 bytes in size.");
static_assert(sizeof(teb_header) % sizeof(teb_word_type) == 0,
    "A TEB header is supposed to be a multiple of the word size.");
//===----------------------------------------------------------------------===//
/// Used to represent empty trees and labels.
static const teb_word_type teb_null_word = 0;
//===----------------------------------------------------------------------===//
}; // namespace dtl
