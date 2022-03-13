#include "experiments/util/prune_util.hpp"
#include "src/dtl/bitmap/diff/diff.hpp"
#include <dtl/bitmap/dynamic_roaring_bitmap.hpp>
#include "src/dtl/bitmap/diff/merge.hpp"

#include <algorithm>
#include <random>

/*
Experiment: compare TEB + roaring with in-place TEB with number of updates applied
*/

void run_diff_benchmark(benchmark_task task, std::vector<std::size_t> update_counts, std::ofstream &log_file, std::ofstream &res_file) {
    dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> new_diff_teb(task.bitset);
    dtl::teb_wrapper new_teb(task.bitset);
    using merge_type = dtl::merge_naive<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap>;

    log_file << "Performing benchmark...\n";
    log_file << "Initial bitmap size: " << task.bitset.size() << ".\n";
    log_file << "Number of updates queued: " << task.mixed_updates.size() << ".\n\n";
    log_file << "Initial TEB + diff size: " << new_diff_teb.get_bitmap()->size_in_bytes() << " + " << new_diff_teb.diff_size_in_bytes() << " bytes.\n";
    log_file << "Initial TEB size: " << new_teb.size_in_bytes() << " bytes.\n";

    res_file 
        << task.bitset.size() << " 0 " 
        << new_diff_teb.get_bitmap()->size_in_bytes() << " " 
        << new_diff_teb.diff_size_in_bytes() << " "
        << new_teb.size_in_bytes() << " ";
    
    std::size_t update_count = 0;
    std::size_t next_threshold = update_counts[0];
    std::size_t threshold_idx = 0;

    for (update_task update : task.mixed_updates) {
        if (new_teb.teb_->update(update.pos, update.val)) {
            new_diff_teb.set(update.pos, update.val);
            update_count++;
        }

        assert(new_diff_teb.test(update.pos) == new_teb.test(update.pos));
        if (update_count == next_threshold) {
            log_file << "Applied " << update_count << " updates.\n";
            log_file << "Size of TEB + diff: " << new_diff_teb.get_bitmap()->size_in_bytes() << " + " << new_diff_teb.diff_size_in_bytes() << " bytes.\n";
            log_file << "Size of TEB: " << new_teb.size_in_bytes() << " bytes.\n\n";

            res_file
                << next_threshold << " " 
                << new_diff_teb.get_bitmap()->size_in_bytes() << " "
                << new_diff_teb.diff_size_in_bytes() << " " 
                << new_teb.size_in_bytes() << " ";

            threshold_idx++;
            next_threshold = update_counts[threshold_idx];
        }
    }

    res_file << "\n";
    log_file.flush();

    // <bitset size> <prune threshold>
    // <update count> <total diff update time in microsec> <avg diff update time in ns> <total teb update time in microsec> <avg teb update time in ns>
    // <lookup count> <total diff lookup t in microsec> <avg diff lookup t in ns> <total teb lookup t in microsec> <avg teb lookup t in ns>
    // <merge/prune count> <total merge time in s> <avg merge time in s> <total prune time in s> <avg prune time in s>

    res_file.flush();
}

//std::vector<double> bit_densities {0.1, 0.01, 0.001, 0.0001, 0.00001};
//std::vector<double> clustering_factors {0.8, 0,5, 0.3, 0.1, 0.01, 0.001, 0.0001}; not relevant?
std::vector<std::size_t> bitset_sizes {1000000};
std::vector<std::size_t> update_counts {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000};
std::size_t benchmark_rep = 20;

int main() {
    std::ofstream prune_test_log;
    prune_test_log.open("update_count_v_teb_size_log.txt");
    std::ofstream res_file;
    res_file.open("update_count_v_teb_size_res.txt");
    std::vector<benchmark_task> benchmark_list;

    // Generate bitmaps and update tasks
    for (std::size_t bitset_size : bitset_sizes) {
        for (auto i = 0; i < benchmark_rep; i++) {
            // Generate bitmap
            boost::dynamic_bitset<u_int32_t> new_bitset = generate_bitset_bernoulli(bitset_size, i, 0.1);

            prune_test_log << "Bitset size: " << bitset_size << "\nBitset:\n";
            if (bitset_size >= 100) {
                prune_test_log << "Too large to display";
            } else {
                prune_test_log << new_bitset;
            }
            prune_test_log << std::endl;
            
            // Generate updates
            std::vector<update_task> mixed;
            std::vector<update_task> all_ones;
            std::vector<update_task> all_zeroes;

            for (int i = 0; i < bitset_size; i += 2) {
                if (new_bitset[i] != new_bitset[i + 1]) {
                    update_task new_update{i + 1, new_bitset[i]};
                    mixed.push_back(new_update);
                }
            }

            auto rng = std::default_random_engine();
            std::shuffle(std::begin(mixed), std::end(mixed), rng);

            prune_test_log << "Generated " << mixed.size() << " run-forming updates.\n";

            // Generate benchmark tasks/configs
            prune_test_log << "Generating benchmark tasks...\n\n";
            benchmark_task new_benchmark {
                new_bitset,
                0,
                mixed,
                all_zeroes,
                all_ones,
                benchmark_rep
            };
            benchmark_list.push_back(new_benchmark);
        }
    }

    prune_test_log << "Finished generating " << benchmark_list.size() << " benchmark tasks.\n\n";
    prune_test_log.flush();

    res_file << benchmark_rep << "\n";
    for (benchmark_task benchmark : benchmark_list) {
        run_diff_benchmark(benchmark, update_counts, prune_test_log, res_file);
 
    }
    res_file.close();
    prune_test_log.close(); 
}