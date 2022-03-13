#include "experiments/util/prune_util.hpp"
#include "src/dtl/bitmap/diff/diff.hpp"
#include <dtl/bitmap/dynamic_roaring_bitmap.hpp>
#include "src/dtl/bitmap/diff/merge.hpp"

/*
Experiment: compare prune/diff lookup times
*/

void run_diff_benchmark(benchmark_task task, std::vector<std::size_t> update_cps, std::ofstream &log_file, std::ofstream &res_file) {
    dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> new_diff_teb(task.bitset);
    dtl::teb_wrapper new_teb(task.bitset);
    using merge_type = dtl::merge_naive<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap>;

    log_file << "Performing benchmark...\n";
    log_file << "Initial bitmap size: " << task.bitset.size() << ".\n";
    log_file << "Prune threshold: " << task.prune_trigger << ".\n";
    log_file << "Number of updates queued: " << task.mixed_updates.size() << ".\n";
    
    std::size_t update_count = 0;
    std::size_t lookup_count = 0;
    clock_t prune_lookup_total_t = 0;
    clock_t diff_lookup_total_t = 0;
    std::size_t next_cp = update_cps[0];
    std::size_t current_cp = 0;
    for (update_task update : task.mixed_updates) {
        new_diff_teb.set(update.pos, update.val);
        
        new_teb.update(update.pos, update.val);

        assert(new_diff_teb.test(update.pos) == new_teb.test(update.pos));
        update_count++;

        if (update_count % 4000 == 0) { 
            new_diff_teb.template merge<merge_type>();
        }

        if (update_count % task.prune_trigger == 0) {
            new_teb.teb_->prune();
        }

        if (update_count == next_cp) {
            for (auto i = 0; i < task.bitset.size(); i++) {
                clock_t prune_lookup_t = clock();
                new_teb.test(i);
                prune_lookup_total_t += clock() - prune_lookup_t;

                clock_t diff_lookup_t = clock();
                new_diff_teb.test(i);
                diff_lookup_total_t += clock() - diff_lookup_t;
            }

            current_cp++;
            if (current_cp == update_cps.size()) break;
            next_cp = update_cps[current_cp];

            log_file << "Checkpoint: " << update_count << " updates.\n";
            log_file << "In-place TEB average lookup time: " << ((float) prune_lookup_total_t) / CLOCKS_PER_SEC * 1000000000 / task.bitset.size() << " ns.\n";
            log_file << "Free bits in in-place TEB: " << new_teb.teb_->free_bits_T_ << " + " << new_teb.teb_->free_bits_L_ << "\n";
            log_file << "In-place TEB perfect levels: " << new_teb.teb_->perfect_level_cnt_ << "\n";
            log_file << "TEB-Roaring average lookup time: " << ((float) diff_lookup_total_t) / CLOCKS_PER_SEC * 1000000000 / task.bitset.size() << " ns.\n";
            log_file << "TEB-Roating perfect levels: " << new_diff_teb.get_bitmap()->teb_->perfect_level_cnt_ << "\n\n";

            log_file.flush();

            res_file
                << update_count << " "
                << ((float) prune_lookup_total_t) / CLOCKS_PER_SEC * 1000000000 / task.bitset.size() << " "
                << new_teb.teb_->perfect_level_cnt_ << " "
                << ((float) diff_lookup_total_t) / CLOCKS_PER_SEC * 1000000000 / task.bitset.size() << " "
                << new_diff_teb.get_bitmap()->teb_->perfect_level_cnt_ << "\n";

            prune_lookup_total_t = 0;
            diff_lookup_total_t = 0;
        }
    }
}

//std::vector<double> bit_densities {0.1, 0.01, 0.001, 0.0001, 0.00001};
//std::vector<double> clustering_factors {0.8, 0,5, 0.3, 0.1, 0.01, 0.001, 0.0001}; not relevant?
std::vector<std::size_t> bitset_sizes {1000000};
std::vector<std::size_t> prune_thresholds {4000};
std::vector<std::size_t> updates_applied_cp {10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000,110000, 120000, 130000, 140000, 150000};
std::size_t benchmark_rep = 1;

int main() {
    std::ofstream prune_test_log;
    prune_test_log.open("diff_v_prune_lookup_log.txt");
    std::ofstream res_file;
    res_file.open("diff_v_prune_lookup_res.txt");
    std::vector<benchmark_task> benchmark_list;

    // Generate bitmaps and update tasks
    for (std::size_t bitset_size : bitset_sizes) {
        // Generate bitmap
        boost::dynamic_bitset<u_int32_t> new_bitset = generate_bitset_bernoulli(bitset_size, 10, 0.1);

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

        prune_test_log << "Generated " << mixed.size() << " run-forming updates.\n";
        prune_test_log << "Generated " << all_ones.size() << " run-forming 1-updates.\n";
        prune_test_log << "Generated " << all_zeroes.size() << " run-forming 0-updates.\n";

        // Generate benchmark tasks/configs
        prune_test_log << "Generating benchmark tasks...\n\n";
        for (std::size_t threshold : prune_thresholds) {
            benchmark_task new_benchmark {
                new_bitset,
                threshold,
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

    for (benchmark_task benchmark : benchmark_list) {
        run_diff_benchmark(benchmark, updates_applied_cp, prune_test_log, res_file);
 
    }
    res_file.close();
    prune_test_log.close(); 
}