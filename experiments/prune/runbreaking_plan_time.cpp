#include "experiments/util/prune_util.hpp"
#include "src/dtl/bitmap/diff/diff.hpp"
#include <dtl/bitmap/dynamic_roaring_bitmap.hpp>
#include "src/dtl/bitmap/diff/merge.hpp"

// Template test program

benchmark_result perform_benchmark(benchmark_task benchmark, Update_Type type, std::ofstream &log_file, std::ofstream &res_file) {
    std::vector<update_task> *update_vector = &benchmark.mixed_updates;
    dtl::teb_wrapper new_teb = dtl::teb_wrapper(benchmark.bitset);
    dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> new_diff_teb(benchmark.bitset);
    using merge_type = dtl::merge_naive<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap>;
    assert(bitmap_equals_teb(benchmark.bitset, &new_teb));

    boost::dynamic_bitset<u_int32_t> test_bitset = benchmark.bitset;
    for (update_task update : *update_vector) {
        if (new_teb.teb_->update(update.pos, update.val)) {
            new_diff_teb.set(update.pos, update.val);
            test_bitset.set(update.pos, update.val);
        }    
    }
    new_teb.teb_->prune();
    new_diff_teb.template merge<merge_type>();

    log_file << "Finished applying run-forming updates.\n";
    log_file << "Free bits (T + L): " << new_teb.teb_->free_bits_T_ << " + " << new_teb.teb_->free_bits_L_ << "\n";
    log_file << "Perfect levels: " << new_teb.teb_->perfect_level_cnt_ << "\n";

    std::size_t update_count = 0;
    clock_t update_total_t  = 0;
    clock_t diff_update_total_t = 0;
    std::size_t failed_update_count = 0;
    bool failed_once = false;
    for (update_task update : benchmark.run_breaking) {
        std::size_t run_length = new_teb.teb_->get_run_length_by_pos(update.pos);

        clock_t update_t = clock();
        clock_t plan_t = clock();
        int update_res = new_teb.teb_->update(update.pos, update.val, &plan_t);
        update_t = clock() - update_t;

        if (update_res == 1) {
            clock_t diff_update_t = clock();
            new_diff_teb.set(update.pos, update.val);
            diff_update_t = clock() - diff_update_t;

            update_count++;

            if (update_count % 10000 == 0) {
                clock_t merge_t = clock();
                new_diff_teb.template merge<merge_type>();
                merge_t = clock() - merge_t;
                diff_update_total_t += merge_t;
            }

            update_total_t += update_t;
            diff_update_total_t += diff_update_t;
            test_bitset.set(update.pos, update.val);
            res_file << run_length << " " << ((float) update_t) / CLOCKS_PER_SEC * 1000000 << " " << ((float) diff_update_t) / CLOCKS_PER_SEC * 1000000 << " " << ((float) plan_t) / CLOCKS_PER_SEC * 1000000 << "\n";
        } else {
            if (!failed_once){
                log_file << "First update fail at update after " << update_count << ".\n";
                failed_once = true;
            }
            failed_update_count++;
        }
        if (failed_once) {
            log_file << "Free bits (T + L): " << new_teb.teb_->free_bits_T_ << " + " << new_teb.teb_->free_bits_L_ << "\n";
            break;
        }
    }

    log_file << "Applied " << update_count << " run-breaking updates.\n";
    log_file << "Average in-place update time: " << (float) update_total_t / CLOCKS_PER_SEC / update_count << " s\n";
    log_file << "Total time spent updating in place: " << (float) update_total_t / CLOCKS_PER_SEC << " s\n";
    log_file << "Failed updates: " << failed_update_count << " updates.\n";
    log_file << "Total time spent updating diff with merging: " << (float) diff_update_total_t / CLOCKS_PER_SEC << " s\n";
    log_file << "Average update time diff with merging: " << (float) diff_update_total_t / CLOCKS_PER_SEC / update_count << " s\n\n";

    benchmark_result benchmark_res {
        bitmap_equals_teb(test_bitset, &new_teb),
        bitset_equals(test_bitset, new_teb.to_bitmap_using_iterator()),
        1
    };
    return benchmark_res;
}

std::vector<double> bit_densities {0.1, 0.01, 0.001, 0.0001, 0.00001};
//std::vector<double> clustering_factors {0.8, 0,5, 0.3, 0.1, 0.01, 0.001, 0.0001}; not relevant?
std::vector<std::size_t> bitset_sizes {1000000};
//std::vector<std::size_t> bitset_sizes {10000};
//std::vector<std::size_t> prune_thresholds {1};
std::vector<std::size_t> prune_thresholds {20000};
//std::vector<std::size_t> update_numbers {10, 100, 1000, 10000}; not relevant?
std::size_t benchmark_rep = 1;

int main() {
    std::ofstream prune_test_log;
    prune_test_log.open("runbreaking_plan_time_log.txt");
    std::vector<benchmark_task> benchmark_list;

    std::ofstream res_file;
    res_file.open("runbreaking_plan_time_res.txt");

    // Generate bitmaps and update tasks
    for (std::size_t bitset_size : bitset_sizes) {
        // Generate bitmap
        std::cout << "Generating a new bitset...";
        boost::dynamic_bitset<u_int32_t> new_bitset = generate_bitset_bernoulli(bitset_size, 10, 0.1);

        prune_test_log << "Bitset size: " << bitset_size << "\nBitset:\n";
        if (bitset_size >= 20000) {
            prune_test_log << "Too large to display";
        } else {
            prune_test_log << new_bitset;
        }
        prune_test_log << std::endl;
        
        // Generate updates
        std::vector<update_task> mixed;
        std::vector<update_task> all_ones;
        std::vector<update_task> all_zeroes;
        std::vector<update_task> runbreaking;

        for (int i = 0; i < bitset_size; i += 2) {
            if (new_bitset[i] != new_bitset[i + 1]) {
                update_task new_update{i + 1, new_bitset[i]};
                mixed.push_back(new_update);
                (new_bitset[i]) ?
                    all_ones.push_back(new_update) : 
                    all_zeroes.push_back(new_update);
            } else {
                update_task new_update{i + 1, !new_bitset[i]};
                runbreaking.push_back(new_update);
            }
        }

        auto rng = std::default_random_engine();
        std::shuffle(std::begin(runbreaking), std::end(runbreaking), rng);

        prune_test_log << "Generated " << mixed.size() << " run-forming updates.\n";

        // Generate benchmark tasks/configs
        prune_test_log << "Generating benchmark tasks...\n\n";
        for (std::size_t threshold : prune_thresholds) {
            benchmark_task new_benchmark {
                new_bitset,
                threshold,
                mixed,
                all_zeroes,
                all_ones,
                benchmark_rep,
                runbreaking
            };
            benchmark_list.push_back(new_benchmark);
        }
    }

    prune_test_log << "Finished generating " << benchmark_list.size() << " benchmark tasks.\n\n";
    prune_test_log.flush();

    for (benchmark_task benchmark : benchmark_list) {
        prune_test_log << "Performing benchmark test...\n";
        
        prune_test_log <<
            "Bitset size: " << benchmark.bitset.size() << "\n" <<
            "Prune threshold: " << benchmark.prune_trigger << " updates\n";
        prune_test_log.flush();
        benchmark_result benchmark_res_mixed = perform_benchmark(benchmark, mixed, prune_test_log, res_file);
        prune_test_log << "TEB pruned " << benchmark_res_mixed.prune_count << " time(s).\n";
        if (benchmark_res_mixed.teb_equals_bitset && benchmark_res_mixed.decompressed_equals_bitset) {
            prune_test_log << "Benchmark successful, pruned TEB equals uncompressed bitset and decompressed TEB equals uncompressed bitset.\n\n";
        } else  if (!benchmark_res_mixed.teb_equals_bitset && benchmark_res_mixed.decompressed_equals_bitset){
            prune_test_log << "Failed: pruned TEB does not equal uncompressed bitset, but decompressed TEB equals uncompressed bitset\n\n";
        } else if (benchmark_res_mixed.teb_equals_bitset && !benchmark_res_mixed.decompressed_equals_bitset) {
            prune_test_log << "Failed: pruned TEB equals uncompressed bitset, but decompressed TEB does not equal uncompressed bitset\n\n";
        } else {
            prune_test_log << "Failed: pruned TEB does not equal uncompressed bitset and decompressed TEB does not equal uncompressed bitset\n\n";
        }
        prune_test_log.flush();
    }
    prune_test_log.close();
    res_file.close();
}