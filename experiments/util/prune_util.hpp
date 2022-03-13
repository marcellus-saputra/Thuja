#include <iostream>
#include <fstream>
#include <boost/dynamic_bitset.hpp>
#include <bitset>
#include <random>
#include <time.h>
#include "src/dtl/bitmap/teb_wrapper.hpp"
#include "src/dtl/bitmap/teb_flat.hpp"
#include "src/dtl/bitmap/util/convert.hpp"

#include "src/dtl/bitmap/teb_types.hpp"
#include "src/dtl/bitmap/util/bitmap_fun.hpp"

dtl::teb_wrapper* teb_reconstruct;
dtl::teb_wrapper* teb_traverse;

using word_type = dtl::teb_word_type;
using bitmap_funcs = dtl::bitmap_fun<word_type>;
const size_t word_size = sizeof(word_type);

boost::dynamic_bitset<u_int32_t> generate_bitset(unsigned int bitset_size = 32, unsigned int inverse_bit_density = 4) {
    boost::dynamic_bitset<u_int32_t> initial_bitset(bitset_size);
    for (unsigned int i = 0; i < bitset_size; i++) {
        if (!(rand() % inverse_bit_density)) {
            initial_bitset.set(i);
        }
    }
    return initial_bitset;
}

boost::dynamic_bitset<u_int32_t> generate_bitset_bernoulli(unsigned int bitset_size = 32, std::size_t seed = 1u, double bit_density = 0.25) {
    boost::dynamic_bitset<u_int32_t> initial_bitset(bitset_size);
    std::default_random_engine generator;
    generator.seed(seed);
    std::bernoulli_distribution bernoulli(bit_density);
    for (unsigned int i = 0; i < bitset_size; i++) {
        if (bernoulli(generator)) {
            initial_bitset.set(i);
        }
    }
    return initial_bitset;
}

bool bitset_equals(boost::dynamic_bitset<u_int32_t> bitset1, boost::dynamic_bitset<u_int32_t> bitset2) {
    assert(bitset1.size() == bitset2.size());
    std::cout << std::endl;
    for (std::size_t i = 0; i < bitset1.size(); i++) {
        if (bitset1[i] != bitset2[i]) {
            std::cout << "\nBE: Unequal bit at position " << i << "\n";
            std::cout << "Bitset1: " << bitset1[i] << ", Bitset2: " << bitset2[i] << "\n\n";
            return false;
        } else {
            //std::cout << bitset1[i];
        }
    }
    std::cout << std::endl;
    return true;
}

bool bitmap_equals_teb(boost::dynamic_bitset<u_int32_t> bitset, dtl::teb_wrapper *teb) {
    assert(bitset.size() == teb->teb_->n_actual_);
    bool equal = true;
    for (std::size_t i = 0; i < bitset.size(); i++) {
        if (bitset[i] != teb->test(i)) {
            std::cout << "\nBeT: Unequal bit at position " << i << "\n";
            std::cout << "Bitset: " << bitset[i] << ", TEB: " << teb->test(i) << "\n\n";
            std::cout.flush();
            equal = false;
        }
    }
    return equal;
}

struct update_task {
    std::size_t pos;
    bool val;
};

namespace std {
    void swap(update_task &task1, update_task &task2) {
        std::size_t pos_temp = task1.pos; 
        bool pos_val = task1.val;

        task1.pos = task2.pos;
        task1.val = task2.val;

        task2.pos = pos_temp;
        task2.val = pos_val;
    }
}

struct benchmark_task {
    boost::dynamic_bitset<u_int32_t> bitset;
    std::size_t prune_trigger;
    std::vector<update_task> mixed_updates;
    std::vector<update_task> all_zeroes;
    std::vector<update_task> all_ones;
    std::size_t rep_count;
    std::vector<update_task> run_breaking;
};

struct benchmark_result {
    u1 teb_equals_bitset;
    u1 decompressed_equals_bitset;
    std::size_t prune_count;
};

enum Update_Type {mixed, ones, zeroes};