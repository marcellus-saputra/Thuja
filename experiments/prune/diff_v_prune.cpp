#include "experiments/util/prune_util.hpp"
#include "src/dtl/bitmap/diff/diff.hpp"
#include <dtl/bitmap/dynamic_roaring_bitmap.hpp>
#include "src/dtl/bitmap/diff/merge.hpp"

/*
Experiment: compare pruning v. differential updates for run-forming updates
*/

void run_diff_benchmark(benchmark_task task, std::ofstream &log_file, std::ofstream &res_file) {
    dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> new_diff_teb(task.bitset);
    dtl::teb_wrapper new_teb(task.bitset);
    using merge_type = dtl::merge_naive<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap>;

    log_file << "New TEB granularity: " << new_teb.teb_->rank_.get_block_bitlength() << std::endl;

    log_file << "Performing benchmark...\n";
    log_file << "Initial bitmap size: " << task.bitset.size() << ".\n";
    log_file << "Prune threshold: " << task.prune_trigger << ".\n";
    log_file << "Number of updates queued: " << task.mixed_updates.size() << ".\n";
    
    std::size_t update_count = 0;
    std::size_t merge_count = 0;
    std::size_t lookup_count = 0;
    clock_t diff_lookup_t = 0;
    clock_t prune_lookup_t = 0;
    clock_t diff_update_t = 0;
    clock_t prune_update_t = 0;
    clock_t merge_total_t = 0;
    clock_t prune_total_t = 0;
    for (update_task update : task.mixed_updates) {
        clock_t t_diff = clock();
        new_diff_teb.set(update.pos, update.val);
        t_diff = clock() - t_diff;
        
        clock_t t_teb = clock();
        new_teb.update(update.pos, update.val);
        t_teb = clock() - t_teb;

        assert(new_diff_teb.test(update.pos) == new_teb.test(update.pos));
        update_count++;
        diff_update_t += t_diff;
        prune_update_t += t_teb;

        if (update_count % task.prune_trigger == 0) {
            for (auto i = 0; i < task.bitset.size(); i++) {
                clock_t lookup_t = clock();
                new_diff_teb.test(i);
                diff_lookup_t += clock() - lookup_t;

                lookup_t = clock();
                new_teb.test(i);
                prune_lookup_t += clock() - lookup_t;
            }
            lookup_count += task.bitset.size();

            clock_t merge_t = clock();
            new_diff_teb.template merge<merge_type>();
            merge_t = clock() - merge_t;
            
            clock_t prune_t = clock();
            new_teb.teb_->prune();
            prune_t = clock() - prune_t;

            merge_count++;

            merge_total_t += merge_t;
            prune_total_t += prune_t;
        }
    }

    log_file << "Applied " << update_count << " updates.\n";
    log_file << "Total update time for TEB + diff: " << ((float) diff_update_t) / CLOCKS_PER_SEC * 1000000 << " microseconds.\n";
    log_file << "Average update time for TEB + diff: " << ((float) diff_update_t) / CLOCKS_PER_SEC * 1000000000 / update_count << " nanoseconds.\n";
    log_file << "Total update time for TEB w/ prune: " << ((float) prune_update_t) / CLOCKS_PER_SEC  * 1000000 << " microseconds.\n";
    log_file << "Average update time for TEB + prune: " << ((float) prune_update_t) / CLOCKS_PER_SEC * 1000000000 / update_count << " nanoseconds.\n";
    log_file << "Performed " << lookup_count << " lookups.\n";
    log_file << "Total lookup time for TEB + diff: " << ((float) diff_lookup_t) / CLOCKS_PER_SEC * 1000000 << " microseconds.\n";
    log_file << "Average lookup time for TEB + diff: " << ((float) diff_lookup_t) / CLOCKS_PER_SEC / lookup_count * 1000000000 << " nanoseconds.\n";
    log_file << "Total lookup time for TEB w/ prune: " << ((float) prune_lookup_t) / CLOCKS_PER_SEC * 1000000 << " microseconds.\n";
    log_file << "Average lookup time for TEB w/ prune: " << ((float) prune_lookup_t) / CLOCKS_PER_SEC / lookup_count * 1000000000 << " nanoseconds.\n";
    log_file << "Performed " << merge_count << " merges/prunes.\n";
    log_file << "Total merge time: " << ((float) merge_total_t) / CLOCKS_PER_SEC << " seconds.\n";
    log_file << "Average merge time: " << ((float) merge_total_t) / CLOCKS_PER_SEC / merge_count << " seconds.\n";
    log_file << "Total prune time: " << ((float) prune_total_t) / CLOCKS_PER_SEC << " seconds.\n";
    log_file << "Average prune: " << ((float) prune_total_t) / CLOCKS_PER_SEC / merge_count << " seconds.\n";
    float avg_total_t_diff = 
        ((float) diff_update_t) / CLOCKS_PER_SEC / update_count +
        ((float) diff_lookup_t) / CLOCKS_PER_SEC / lookup_count + 
        ((float) merge_total_t) / CLOCKS_PER_SEC / merge_count;
    float avg_total_t_prune = 
        ((float) prune_update_t) / CLOCKS_PER_SEC / update_count +
        ((float) prune_lookup_t) / CLOCKS_PER_SEC / lookup_count +
        ((float) prune_total_t) / CLOCKS_PER_SEC / merge_count;
    log_file << "Average total time for TEB + diff: " << avg_total_t_diff << " second(s).\n";
    log_file << "Average total time for TEB + prune: " << avg_total_t_prune << " second(s).\n\n";
    log_file.flush();

    // <bitset size> <prune threshold>
    // <update count> <total diff update time in microsec> <avg diff update time in ns> <total teb update time in microsec> <avg teb update time in ns>
    // <lookup count> <total diff lookup t in microsec> <avg diff lookup t in ns> <total teb lookup t in microsec> <avg teb lookup t in ns>
    // <merge/prune count> <total merge time in s> <avg merge time in s> <total prune time in s> <avg prune time in s>
    res_file
        << task.bitset.size() << " "
        << task.prune_trigger << " "
        << update_count << " "
        << ((float) diff_update_t) / CLOCKS_PER_SEC * 1000000 << " "
        << ((float) diff_update_t) / CLOCKS_PER_SEC * 1000000000 / update_count << " "
        << ((float) prune_update_t) / CLOCKS_PER_SEC  * 1000000 << " "
        << ((float) prune_update_t) / CLOCKS_PER_SEC * 1000000000 / update_count << " "
        << lookup_count << " "
        << ((float) diff_lookup_t) / CLOCKS_PER_SEC * 1000000 << " "
        << ((float) diff_lookup_t) / CLOCKS_PER_SEC / lookup_count * 1000000000 << " "
        << ((float) prune_lookup_t) / CLOCKS_PER_SEC * 1000000 << " "
        << ((float) prune_lookup_t) / CLOCKS_PER_SEC / lookup_count * 1000000000 << " "
        << merge_count << " "
        << ((float) merge_total_t) / CLOCKS_PER_SEC << " "
        << ((float) merge_total_t) / CLOCKS_PER_SEC / merge_count << " "
        << ((float) prune_total_t) / CLOCKS_PER_SEC << " "
        << ((float) prune_total_t) / CLOCKS_PER_SEC / merge_count << "\n";
    res_file.flush();
}

//std::vector<double> bit_densities {0.1, 0.01, 0.001, 0.0001, 0.00001};
//std::vector<double> clustering_factors {0.8, 0,5, 0.3, 0.1, 0.01, 0.001, 0.0001}; not relevant?
std::vector<std::size_t> bitset_sizes {1000000};
std::vector<std::size_t> prune_thresholds {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
std::size_t benchmark_rep = 1;

int main() {
    std::ofstream prune_test_log;
    prune_test_log.open("diff_v_prune_log.txt");
    std::ofstream res_file;
    res_file.open("diff_v_prune_res.txt");
    std::vector<benchmark_task> benchmark_list;

    // Generate bitmaps and update tasks
    for (std::size_t bitset_size : bitset_sizes) {
        for (auto i = 0; i < benchmark_rep; i++) { 
            // Generate bitmap
            boost::dynamic_bitset<u_int32_t> new_bitset = generate_bitset_bernoulli(bitset_size, 1ul, 0.1);

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
    }

    prune_test_log << "Finished generating " << benchmark_list.size() << " benchmark tasks.\n\n";
    prune_test_log.flush();

    for (benchmark_task benchmark : benchmark_list) {
        run_diff_benchmark(benchmark, prune_test_log, res_file);
 
    }
    res_file.close();
    prune_test_log.close(); 
}