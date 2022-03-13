#include "experiments/util/prune_util.hpp"

/*
Experiment:
    Measuring the scaling of update time (bit flipping) with respect to initial bitmap size.

Expectation:
    Logarithmic time
*/

benchmark_result perform_benchmark(benchmark_task benchmark, Update_Type type, std::ofstream &log_file, std::ofstream &res_file) {
    std::vector<update_task> *update_vector;
    switch (type) {
    case mixed:
        update_vector = &benchmark.mixed_updates;
        break;
    case ones:
        update_vector = &benchmark.all_ones;
        break;
    case zeroes:
        update_vector = &benchmark.all_zeroes;
        break;
    }
    dtl::teb_wrapper new_teb = dtl::teb_wrapper(benchmark.bitset);
    assert(bitmap_equals_teb(benchmark.bitset, &new_teb));
    std::size_t update_count = 0;
    std::size_t total_update_count = 0;
    boost::dynamic_bitset<u_int32_t> test_bitset = benchmark.bitset;
    clock_t update_total_t = 0;
    for (update_task update : *update_vector) {
        clock_t t_update = clock();
        u1 update_result = new_teb.teb_->update(update.pos, update.val);
        t_update = clock() - t_update;
        std::cout << "Updating position " << update.pos << " to " << update.val << std::endl;
        if (update_result) {
            total_update_count++;
            update_total_t += t_update;
            if (new_teb.test(update.pos) != update.val) {
                log_file << "Update at " << update.pos << " performed incorrectly.\n";
            }
            test_bitset.set(update.pos, update.val);
            update_count++;
        } else {
            log_file << "Update at " << update.pos << " failed!\n";
        }
    }
    log_file << "Completed " << total_update_count << " updates in " << ((float) update_total_t) / CLOCKS_PER_SEC * 1000 << " milisecond(s).\n";
    log_file << "Average update time: " << ((float) update_total_t) / CLOCKS_PER_SEC / total_update_count * 1000000000 << " nanosecond(s).\n";

    benchmark_result benchmark_res {
        bitmap_equals_teb(test_bitset, &new_teb),
        bitset_equals(test_bitset, new_teb.to_bitmap_using_iterator()),
        0
    };

    std::string update_type = type == mixed ? "mixed" : type == ones ? "ones" : "zeroes";
    // <bitset size> <update_type> <update count> <total t in ms> <average t in ns>
    res_file 
        << benchmark.bitset.size() << " " 
        << update_type << " " << update_count << " " 
        << ((float) update_total_t) / CLOCKS_PER_SEC * 1000 << " "
        << ((float) update_total_t) / CLOCKS_PER_SEC / total_update_count * 1000000000 << "\n";

    return benchmark_res;
}

std::vector<double> bit_densities {0.1, 0.01, 0.001, 0.0001, 0.00001};
//std::vector<double> clustering_factors {0.8, 0,5, 0.3, 0.1, 0.01, 0.001, 0.0001}; not relevant?
std::vector<std::size_t> bitset_sizes {1000, 10000, 100000, 1000000, 10000000};
//std::vector<std::size_t> bitset_sizes {10000};
//std::vector<std::size_t> prune_thresholds {1};
//std::vector<std::size_t> update_numbers {10, 100, 1000, 10000}; not relevant?
std::size_t benchmark_rep = 100;

int main() {
    std::ofstream prune_test_log;
    prune_test_log.open("size_v_update_log.txt");
    std::ofstream res_file;
    res_file.open("size_v_update_res.txt");
    std::vector<benchmark_task> benchmark_list;

    // Generate bitmaps and update tasks
    for (std::size_t bitset_size : bitset_sizes) {
        // Generate bitmap
        std::cout << "Generating a new bitset...";
        boost::dynamic_bitset<u_int32_t> new_bitset = generate_bitset_bernoulli(bitset_size, 0.25);

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
                std::cout << new_bitset[i] << new_bitset[i + 1] << " ";
                update_task new_update{i + 1, new_bitset[i]};
                mixed.push_back(new_update);
                (new_bitset[i]) ?
                    all_ones.push_back(new_update) : 
                    all_zeroes.push_back(new_update);
            }
        }

        prune_test_log << "Generated " << mixed.size() << " run-forming updates.\n";
        prune_test_log << "Generated " << all_ones.size() << " run-forming 1-updates.\n";
        prune_test_log << "Generated " << all_zeroes.size() << " run-forming 0-updates.\n";

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

    prune_test_log << "Finished generating " << benchmark_list.size() << " benchmark tasks.\n\n";
    prune_test_log.flush();

    for (benchmark_task benchmark : benchmark_list) {
        prune_test_log << "Performing benchmark test...\n";
        
        prune_test_log <<
            "Bitset size: " << benchmark.bitset.size() << "\n" <<
            "Update type: Mixed\n";
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

        prune_test_log <<
            "Bitset size: " << benchmark.bitset.size() << "\n" <<
            "Prune threshold: " << benchmark.prune_trigger << " updates\n" <<
            "Update type: All ones\n";
        prune_test_log.flush();
        benchmark_result benchmark_res_ones = perform_benchmark(benchmark, ones, prune_test_log, res_file);
        prune_test_log << "TEB pruned " << benchmark_res_ones.prune_count << " time(s).\n";
        if (benchmark_res_ones.teb_equals_bitset && benchmark_res_ones.decompressed_equals_bitset) {
            prune_test_log << "Benchmark successful, pruned TEB equals uncompressed bitset and decompressed TEB equals uncompressed bitset.\n\n";
        } else  if (!benchmark_res_ones.teb_equals_bitset && benchmark_res_ones.decompressed_equals_bitset){
            prune_test_log << "Failed: pruned TEB does not equal uncompressed bitset, but decompressed TEB equals uncompressed bitset\n\n";
        } else if (benchmark_res_ones.teb_equals_bitset && !benchmark_res_ones.decompressed_equals_bitset) {
            prune_test_log << "Failed: pruned TEB equals uncompressed bitset, but decompressed TEB does not equal uncompressed bitset\n\n";
        } else {
            prune_test_log << "Failed: pruned TEB does not equal uncompressed bitset and decompressed TEB does not equal uncompressed bitset\n\n";
        }
        prune_test_log.flush();

        prune_test_log <<
            "Bitset size: " << benchmark.bitset.size() << "\n" <<
            "Prune threshold: " << benchmark.prune_trigger << " updates\n" <<
            "Update type: All zeroes\n";
        prune_test_log.flush();
        benchmark_result benchmark_res_zeroes = perform_benchmark(benchmark, zeroes, prune_test_log, res_file);
        prune_test_log << "TEB pruned " << benchmark_res_zeroes.prune_count << " time(s).\n";
        if (benchmark_res_zeroes.teb_equals_bitset && benchmark_res_zeroes.decompressed_equals_bitset) {
            prune_test_log << "Benchmark successful, pruned TEB equals uncompressed bitset and decompressed TEB equals uncompressed bitset.\n\n";
        } else  if (!benchmark_res_zeroes.teb_equals_bitset && benchmark_res_zeroes.decompressed_equals_bitset){
            prune_test_log << "Failed: pruned TEB does not equal uncompressed bitset, but decompressed TEB equals uncompressed bitset\n\n";
        } else if (benchmark_res_zeroes.teb_equals_bitset && !benchmark_res_zeroes.decompressed_equals_bitset) {
            prune_test_log << "Failed: pruned TEB equals uncompressed bitset, but decompressed TEB does not equal uncompressed bitset\n\n";
        } else {
            prune_test_log << "Failed: pruned TEB does not equal uncompressed bitset and decompressed TEB does not equal uncompressed bitset\n\n";
        }
        prune_test_log.flush();
    }
    res_file.close();
    prune_test_log.close(); 
}