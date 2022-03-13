#include <iostream>
#include <fstream>
#include <boost/dynamic_bitset.hpp>
#include <bitset>
#include "teb_wrapper.hpp"
#include "teb_flat.hpp"
#include "util/convert.hpp"

#include "teb_types.hpp"
#include "util/bitmap_fun.hpp"

dtl::teb_wrapper* teb_reconstruct;
dtl::teb_wrapper* teb_traverse;

using word_type = dtl::teb_word_type;
using bitmap_funcs = dtl::bitmap_fun<word_type>;
const size_t word_size = sizeof(word_type);

boost::dynamic_bitset<u_int32_t> generate_random_bitset(unsigned int bitset_size = 32, unsigned int inverse_bit_density = 4) {
    boost::dynamic_bitset<u_int32_t> initial_bitset(bitset_size);
    for (unsigned int i = 0; i < bitset_size; i++) {
        if (!(rand() % inverse_bit_density)) {
            initial_bitset.set(i);
        }
    }
    return initial_bitset;
}

void display_L(dtl::teb_wrapper* teb) {
    unsigned long int L_word_count = dtl::teb_flat::get_label_word_cnt(teb->teb_->ptr_);

    std::cout << "Labels:\n";
    word_type* label_ptr = dtl::teb_flat::get_label_ptr(teb->teb_->ptr_);
    for (int i = 0; i < L_word_count; i++) {
        std::cout << std::bitset<64>(label_ptr[i]) << " " << label_ptr[i] << std::endl;
    }
    std::cout << std::endl;
}

void display_T(dtl::teb_wrapper* teb) {
    unsigned long int T_word_count = dtl::teb_flat::get_tree_word_cnt(teb->teb_->ptr_);

    std::cout << "Tree:\n";
    word_type* tree_ptr = dtl::teb_flat::get_tree_ptr(teb->teb_->ptr_);
    for (int i = 0; i < T_word_count; i++) {
        std::cout << std::bitset<64>(tree_ptr[i]) << " " << bitmap_funcs::count_set_bits(tree_ptr[i]) << std::endl;
    }
    std::cout << std::endl;
}

bool bitset_equals(boost::dynamic_bitset<u_int32_t> bitset1, boost::dynamic_bitset<u_int32_t> bitset2) {
    assert(bitset1.size() == bitset2.size());
    for (std::size_t i = 0; i < bitset1.size(); i++) {
        if (bitset1[i] != bitset2[i]) {
            std::cout << "Unequal bit at position " << i << "\n\n";
            return false;
        }
    }
    return true;
}

bool bitmap_equals_teb(boost::dynamic_bitset<u_int32_t> bitset, dtl::teb_wrapper *teb) {
    assert(bitset.size() == teb->teb_->n_actual_);
    for (std::size_t i = 0; i < bitset.size(); i++) {
        if (bitset[i] != teb->test(i)) {
            std::cout << "BeT: Unequal bit at position " << i << "\n\n";
            std::cout << "Bitset: " << bitset[i] << ", TEB: " << teb->test(i) << "\n";
            return false;
        }
    }
    return true;
}

int main() {
    std::cout << "This is a test. " << __cplusplus << "\n";
    std::ofstream reconstruct_file;
    reconstruct_file.open("reconstruct_log.txt");

    std::size_t bitset_size = 10000;
    boost::dynamic_bitset<u_int32_t> initial_bitset = generate_random_bitset(bitset_size);

    teb_reconstruct = new dtl::teb_wrapper(initial_bitset);
    teb_traverse = new dtl::teb_wrapper(initial_bitset);

    teb_reconstruct->print(std::cout);
    std::cout << "\n\n";
    teb_traverse->print(std::cout);

    teb_traverse = new dtl::teb_wrapper(initial_bitset);
    teb_traverse->reconstruct = false;

    boost::dynamic_bitset<$u32> decompressed_prune = teb_traverse->to_bitmap_using_iterator();
    std::cout << initial_bitset << "\n\n" << decompressed_prune;
    assert(bitset_equals(initial_bitset, teb_traverse->to_bitmap_using_iterator()));
    std::cout << "\nInitial bitmap decompress check complete\n";
    assert(bitmap_equals_teb(initial_bitset, teb_traverse));
    std::cout << "bitmap equals teb check complete\n";
    teb_traverse->test(0);

    int update_count = 0;
    int update_break = 1;
    u1 update_label = 1;
    int prune_count = 0;

    // Set every lowest-level leaf to update label;
    for (int i = 0; i < bitset_size; i++) {
        if (teb_traverse->teb_->update(i, update_label)) {
            teb_reconstruct->update(i, update_label);
            std::cout << "Updating pos " << i << " to " << update_label << "\n\n";
            initial_bitset.set(i, update_label);
            update_count += 1;
            teb_traverse->teb_->prune();
            decompressed_prune = teb_traverse->to_bitmap_using_iterator();
            if (initial_bitset != decompressed_prune) {
                reconstruct_file << initial_bitset << "\n\n" << decompressed_prune << "\n\n";
                teb_traverse->print(reconstruct_file);
                reconstruct_file.close();
            }
            assert(bitset_equals(decompressed_prune, initial_bitset));
            std::cout << "Update successful\n";
            /*
            if (update_count == update_break) {
                teb_traverse->teb_->prune();
                update_count = 0;
                prune_count += 1;
            }*/
        }
    }

    teb_reconstruct->print(reconstruct_file);
    reconstruct_file << "\n\n";
    teb_reconstruct->reconstruct_teb();
    teb_reconstruct->print(reconstruct_file);
    reconstruct_file << "\n\n";

    teb_traverse->teb_->prune();
    prune_count += 1;

    reconstruct_file << "Result of traverse prune:\n\n";
    teb_traverse->print(reconstruct_file);

    boost::dynamic_bitset<$u32> decompressed_reconstruct = teb_reconstruct->to_bitmap_using_iterator();
    decompressed_prune = teb_traverse->to_bitmap_using_iterator();
    reconstruct_file << "\n\nDecompressed bitmap (reconstructed):\n" << decompressed_reconstruct;
    reconstruct_file << "\n\nDecompressed bitmap (pruned):\n" << decompressed_prune;
    reconstruct_file << "\n\nTimes pruned: " << prune_count << std::endl;
    if (decompressed_prune != decompressed_reconstruct) {
        std::cout << "\nBoth TEBs do not decompress into the same bitmap";
        reconstruct_file << "\nThey are not equal";
    } else {
        std::cout << "\nBoth TEBs decompress into the same bitmap";
        reconstruct_file << "\nThey are equal";
    }
    
    std::cout << std::endl;
    reconstruct_file.close(); 
}