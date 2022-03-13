#include "experiments/util/prune_util.hpp"
#include "src/dtl/bitmap/diff/diff.hpp"
#include <dtl/bitmap/dynamic_roaring_bitmap.hpp>
#include "src/dtl/bitmap/diff/merge.hpp"

/*
Experiment: compare aggregated update and prune/merge times for TEB + Roaring and pruned TEB for run-forming updates.
TEB + Roaring uses 1% Roaring size, prune uses variable thresholds.
*/

void run_diff_benchmark(benchmark_task task, std::ofstream &log_file, std::ofstream &res_file) {
    dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> new_diff_teb(task.bitset);
    dtl::teb_wrapper new_teb(task.bitset);
    using merge_type = dtl::merge_naive<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap>;

    log_file << "Performing benchmark...\n";
    log_file << "Initial bitmap size: " << task.bitset.size() << ".\n";
    log_file << "Prune threshold: " << task.prune_trigger << ".\n";
    log_file << "Number of updates queued: " << task.mixed_updates.size() << ".\n";
    
    std::size_t update_count = 0;
    std::size_t prune_count = 0;
    clock_t merge_total_t = 0;
    clock_t prune_total_t = 0;
    clock_t merge_update_total_t = 0;
    clock_t prune_update_total_t = 0;

    for (update_task update : task.mixed_updates) {
        clock_t t_diff = clock();
        new_diff_teb.set(update.pos, update.val);
        t_diff = clock() - t_diff;
        
        clock_t t_teb = clock();
        new_teb.update(update.pos, update.val);
        t_teb = clock() - t_teb;

        assert(new_diff_teb.test(update.pos) == new_teb.test(update.pos));

        update_count++;
        merge_update_total_t += t_diff;
        prune_update_total_t += t_teb;

        if (new_diff_teb.diff_size_in_bytes() > new_diff_teb.get_bitmap()->size_in_bytes() / 100) {
            clock_t merge_t = clock();
            new_diff_teb.template merge<merge_type>();
            merge_t = clock() - merge_t;

            merge_total_t += merge_t;
        }

        if (update_count % task.prune_trigger == 0) {
            clock_t prune_t = clock();
            new_teb.teb_->prune();
            prune_t = clock() - prune_t;

            prune_total_t += prune_t;
            prune_count++;
        }
    }

    log_file << "Applied " << update_count << " updates.\n";
    float avg_total_t_diff = ((float) (merge_total_t + merge_update_total_t)) / CLOCKS_PER_SEC / update_count;
    float avg_total_t_prune = ((float) (prune_total_t + prune_update_total_t)) / CLOCKS_PER_SEC / update_count;
    log_file << "Average total time for TEB + diff: " << avg_total_t_diff * 1000000 << " microsecond(s).\n";
    log_file << "Average total time for TEB + prune: " << avg_total_t_prune * 1000000 << " microsecond(s).\n";
    log_file << "Pruned " << prune_count << " time(s).\n";
    log_file << "Total time spent pruning: " << ((float) prune_total_t) / CLOCKS_PER_SEC << " second(s).\n";
    log_file << "Total time spent updating TEB + prune: " << ((float) prune_update_total_t) / CLOCKS_PER_SEC << " second(s).\n\n";
    log_file.flush();

    // <bitset size> <prune threshold>
    // <update count> <total diff update time in microsec> <avg diff update time in ns> <total teb update time in microsec> <avg teb update time in ns>
    // <lookup count> <total diff lookup t in microsec> <avg diff lookup t in ns> <total teb lookup t in microsec> <avg teb lookup t in ns>
    // <merge/prune count> <total merge time in s> <avg merge time in s> <total prune time in s> <avg prune time in s>

    res_file
        << task.prune_trigger << " "
        << avg_total_t_diff * 1000000 << " "
        << avg_total_t_prune * 1000000 << " "
        << ((float) prune_total_t) / CLOCKS_PER_SEC << " "
        << prune_count << " "
        << ((float) prune_update_total_t) / CLOCKS_PER_SEC << "\n";
    
    res_file.flush();
}

//std::vector<double> bit_densities {0.1, 0.01, 0.001, 0.0001, 0.00001};
//std::vector<double> clustering_factors {0.8, 0,5, 0.3, 0.1, 0.01, 0.001, 0.0001}; not relevant?
std::vector<std::size_t> bitset_sizes {1000000};
std::vector<std::size_t> prune_thresholds {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
std::size_t benchmark_rep = 1;

int main() {
    std::ofstream prune_test_log;
    prune_test_log.open("avg_diff_v_prune_log.txt");
    std::ofstream res_file;
    res_file.open("avg_diff_v_prune_res.txt");
    std::vector<benchmark_task> benchmark_list;

    // Generate bitmaps and update tasks
    for (std::size_t bitset_size : bitset_sizes) {
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

        // Generate in reverse
        /*
        for (int i = bitset_size - 1; i > 0; i -= 2) {
            if (new_bitset[i] != new_bitset[i - 1]) {
                update_task new_update{i - 1, new_bitset[i]};
                mixed.push_back(new_update);
                (new_bitset[i]) ?
                    all_ones.push_back(new_update) : 
                    all_zeroes.push_back(new_update);
            }
        }*/

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

    prune_test_log << "Finished generating " << benchmark_list.size() << " benchmark tasks.\n\n";
    prune_test_log.flush();

    for (benchmark_task benchmark : benchmark_list) {
        run_diff_benchmark(benchmark, prune_test_log, res_file);
 
    }
    res_file.close();
    prune_test_log.close(); 
}