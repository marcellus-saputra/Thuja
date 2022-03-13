#include "experiments/util/prune_util.hpp"
#include "src/dtl/bitmap/diff/diff.hpp"
#include <dtl/bitmap/dynamic_roaring_bitmap.hpp>
#include "src/dtl/bitmap/diff/merge.hpp"

// Bitmap generation from TEB paper
#include "experiments/util/params.hpp"
#include "experiments/util/gen.hpp"
#include "experiments/performance/common.hpp"
#include "experiments/util/prep_data.hpp"
#include "experiments/util/prep_updates.hpp"

/*
Experiment: hybrid approach with purely differential updates
*/

//std::vector<double> bit_densities {0.1, 0.01, 0.001, 0.0001, 0.00001};
//std::vector<double> clustering_factors {0.8, 0,5, 0.3, 0.1, 0.01, 0.001, 0.0001}; not relevant?
std::size_t bitset_size = 1000000;
double clustering_factor = 8.05;
std::vector<double> bit_densities = {0.701, 0.6001, 0.4999, 0.40001, 0.300011, 0.199999, 0.100001};
//double clustering_factor = 8;
//std::vector<double> bit_densities = {0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1};
std::vector<std::size_t> prune_thresholds {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
std::size_t benchmark_rep = 2;

int main() {
    std::vector<params_markov> params;

    std::ofstream log_file;
    log_file.open("hybrid_v_diff_log.txt");
    std::ofstream res_file;
    res_file.open("hybrid_v_diff_res.txt");


    // Generate bitmaps and update tasks
    if (GEN_DATA) {
        for (double bit_density : bit_densities) {
            if (!markov_parameters_are_valid(bitset_size, clustering_factor, bit_density)) std::exit(1);
            params_markov p;
            p.clustering_factor = clustering_factor;
            p.density = bit_density;
            p.n = bitset_size;
            params.push_back(p);
            prep_data(params, benchmark_rep, db);
        }
        std::exit(0);
    } else {
        if (db.empty()) {
            std::cerr << "Bitmap database is empty. Use GEN_DATA=1 to populate the "
                        "database."
                        << std::endl;
            std::exit(1);
        }
    }

    for (double bit_density : bit_densities) {
        if (markov_parameters_are_valid(bitset_size, clustering_factor, bit_density)) {
            auto bitmap_ids = db.find_bitmaps(bitset_size, clustering_factor, bit_density);
            if (bitmap_ids.empty()) {
                std::exit(1);
            }
            if (bitmap_ids.size() < benchmark_rep) {
                std::cerr << "There are only " << bitmap_ids.size() << " prepared "
                    << "bitmaps for the parameters n=" << bitset_size << ", f=" << clustering_factor
                    << ", d=" << bit_density << ", but " << benchmark_rep << " are required."
                    << std::endl;
            }
            if (bitmap_ids.size() >= 2) {
                const auto bid_a = bitmap_ids[0];
                const auto bid_b = bitmap_ids[1];
                auto bm_b = db.load_bitmap(bid_b);
                std::cerr << "popcount(bm_b)=" << bm_b.count() << std::endl;
                auto bm_a = db.load_bitmap(bid_a);
                auto range_updates = prepare_range_updates(bm_a, bm_b);

                // Shuffle the update order.
                //std::mt19937 gen(42); // for reproducible results (100k seed)
                std::mt19937 gen(10); // 50k seed
                std::shuffle(range_updates.begin(), range_updates.end(), gen);
                std::cout << range_updates[0];

                std::size_t max_updates = 50000;
                std::vector<update_entry> updates;
                updates.reserve(bm_b.count());
                for (auto& range : range_updates) {
                    for (std::size_t i = range.pos; i < range.pos + range.length; ++i) {
                        if (updates.size() < max_updates) {
                            updates.emplace_back(i, range.value);
                        }
                    }
                }
                auto validation_bm = bm_a;
                dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> hybrid_teb(bm_a);
                dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> diff_teb(bm_a);
                dtl::diff<dtl::teb_wrapper, dtl::dynamic_roaring_bitmap> hybridplus_teb(bm_a);

                std::size_t failed_count_hybrid = 0;
                std::size_t inplace_count_hybrid = 0;
                std::size_t diff_count_hybrid = 0;

                std::size_t failed_count_hybridplus = 0;
                std::size_t inplace_count_hybridplus = 0;
                std::size_t diff_count_hybridplus = 0;

                clock_t hybrid_time = 0;
                clock_t hybridplus_time = 0;
                clock_t diff_time = 0;
                for (update_entry update : updates) {
                    clock_t start_t = clock();
                    int update_res = hybrid_teb.update(update.pos, update.value);
                    hybrid_time += clock() - start_t;
                    switch (update_res)
                    {
                    case 1:
                        inplace_count_hybrid++;
                        /*
                        if (inplace_count_hybrid % 1000 == 0) {
                            //start_t = clock();
                            hybrid_teb.get_bitmap()->teb_->prune();
                            //hybrid_time += clock() - start_t;
                        }*/
                        break;
                    case 2:
                        diff_count_hybrid++;
                        break;
                    case 0:
                        failed_count_hybrid++;
                        break;
                    }

                    start_t = clock();
                    update_res = hybridplus_teb.update(update.pos, update.value, true);
                    hybridplus_time += clock() - start_t;
                    switch (update_res)
                    {
                    case 1:
                        inplace_count_hybridplus++;
                        /*
                        if (inplace_count_hybridplus % 1000 == 0) {
                            //start_t = clock();
                            hybrid_teb.get_bitmap()->teb_->prune();
                            //hybrid_time += clock() - start_t;
                        }*/
                        break;
                    case 2:
                        diff_count_hybridplus++;
                        break;
                    case 0:
                        failed_count_hybridplus++;
                        break;
                    }

                    start_t = clock();
                    diff_teb.set(update.pos, update.value);
                    diff_time += clock() - start_t;
                }
                bool tebs_valid = 1;
                for (std::size_t i = 0; i < bitset_size; i++) {
                    if (hybrid_teb.test(i) != diff_teb.test(i)) {
                        tebs_valid = 0;
                    }
                }
                if (!tebs_valid) { 
                    log_file << "TEBs are not equal.\n";
                } else {
                    log_file << "TEBs are equal.\n";
                }
                log_file
                    << "Bit density: " << bit_density << "\n"
                    << "Plain Hybrid\n"
                    << "Failed updates: " << failed_count_hybrid << "\n"
                    << "In-place updates: " << inplace_count_hybrid << "\n"
                    << "Diff updates: " << diff_count_hybrid << "\n"
                    << "Hybrid+\n"
                    << "Failed updates: " << failed_count_hybridplus << "\n"
                    << "In-Place updates: " << inplace_count_hybridplus << "\n"
                    << "Diff updates: " << diff_count_hybridplus << "\n"
                    << "Hybrid total update time: " << ((float) hybrid_time) / (updates.size()) / CLOCKS_PER_SEC * 1000000 << "micros\n"
                    << "Hybrid total update time: " << ((float) hybridplus_time) / (updates.size()) / CLOCKS_PER_SEC * 1000000 << "micros\n"
                    << "Diff total update time: " << ((float) diff_time) / (updates.size()) / CLOCKS_PER_SEC * 1000000 << "micros\n"
                    << "Hybrid size: " << hybrid_teb.size_in_bytes() << " bytes\n"
                    << "Diff size: " << diff_teb.size_in_bytes() << " bytes\n\n";
                log_file.flush();

                res_file 
                    << bit_density << " "
                    << inplace_count_hybrid << " "
                    << diff_count_hybrid << " "
                    << ((float) hybrid_time) / (updates.size()) / CLOCKS_PER_SEC * 1000000 << " "
                    << ((float) diff_time) / (updates.size()) / CLOCKS_PER_SEC * 1000000 << " "
                    << hybrid_teb.size_in_bytes() << " "
                    << diff_teb.size_in_bytes() << "\n";
            }
        } else {
            std::cout << "Markov params are invalid.\n";
        }
    }


    res_file.close();
    log_file.close();
}