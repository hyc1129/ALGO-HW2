#ifndef EXPERIMENTS_H
#define EXPERIMENTS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "DataStructures.h"
#include "Utils.h"

namespace algo_hw2 {

// PDF Specification: n = 2^0, 2^1, ..., 2^20; id ~ UNI(1, 2^20); score ~ UNI(0, 100); Q = 100,000
constexpr int kMinExpPower = 0;
constexpr int kMaxExpPower = 20;
constexpr int kMinID = 1;
constexpr int kMaxID = (1 << 20);  // 1,048,576
constexpr int kMinScore = 0;
constexpr int kMaxScore = 100;
constexpr std::size_t kQueryBatchSize = 100000;

/**
 * @brief Deterministic seed derivation for (power_k, trial_i) from `base_seed`.
 *        Follows PDF p. 3 ("starting_seed + i" mixed per n) so:
 *          1. Within the same (n, trial_i), ALL data structures receive the exact
 *             same insertion records and query IDs.
 *          2. Running twice with the same `--seed` reproduces identical results.
 */
struct TrialSeeds {
    std::uint64_t insert_seed;
    std::uint64_t query_seed;
    std::uint64_t ds_seed;
};

inline TrialSeeds DeriveTrialSeeds(std::uint64_t base_seed, int power_k, int trial_i) noexcept {
    std::uint64_t s = base_seed + static_cast<std::uint64_t>(trial_i);
    std::uint64_t key = MixSeed(s ^ (static_cast<std::uint64_t>(power_k) << 32));
    TrialSeeds out{};
    out.insert_seed = MixSeed(key ^ 0x1111111111111111ULL);
    out.query_seed  = MixSeed(key ^ 0x2222222222222222ULL);
    out.ds_seed     = MixSeed(key ^ 0x3333333333333333ULL);
    return out;
}

// ============================================================================
// Experiment 1: Total Insertion Time for n = 2^0 .. 2^20 (4 Data Structures)
// ============================================================================

/**
 * @brief Implements the PDF p. 3 timing protocol for Experiment 1:
 *        - Times ONLY the Insert operations (excluding input generation and
 *          memory deallocation).
 *        - If a single structure takes < 1.0 ms to build (small n), constructs
 *          `r` independent instances on the SAME input, times building all `r`
 *          together, divides by `r`, and frees all `r` instances AFTER stopping
 *          the timer.
 */
template <typename DS, typename DSFactory>
double MeasureInsertTimeMsWithReplication(
    const std::vector<Record>& inserts,
    std::uint64_t ds_seed,
    DSFactory make_ds,
    std::size_t& reps_used_out) {
    constexpr double kMinWindowMs = 1.0;
    constexpr std::size_t kMaxReplications = 65536;

    std::size_t r = 1;
    while (true) {
        // Pre-allocate `r` independent empty structures BEFORE starting the timer
        std::vector<std::unique_ptr<DS>> instances;
        instances.reserve(r);
        for (std::size_t rep = 0; rep < r; ++rep) {
            std::uint64_t rep_seed = (rep == 0) ? ds_seed : MixSeed(ds_seed + rep);
            instances.push_back(make_ds(rep_seed));
        }

        // Timed region: ONLY insertion of the n records across the r instances
        SteadyTimer timer;
        timer.Start();
        for (std::size_t rep = 0; rep < r; ++rep) {
            DS& ds = *instances[rep];
            for (std::size_t j = 0; j < inserts.size(); ++j) {
                ds.Insert(inserts[j].id, inserts[j].score);
            }
        }
        timer.Stop();

        // Free all `r` data structures AFTER stopping the timer
        instances.clear();

        double elapsed_ms = timer.ElapsedMilliseconds();
        if (elapsed_ms >= kMinWindowMs || r >= kMaxReplications) {
            reps_used_out = r;
            return elapsed_ms / static_cast<double>(r);
        }

        // Adaptively scale `r` to reach >= 1.0 ms window cleanly
        if (elapsed_ms <= 0.005) {
            r = std::min<std::size_t>(r * 16, kMaxReplications);
        } else {
            double factor = 1.35 / elapsed_ms;
            std::size_t est = static_cast<std::size_t>(std::ceil(static_cast<double>(r) * factor));
            r = std::min<std::size_t>(std::max<std::size_t>(r * 2, est), kMaxReplications);
        }
    }
}

inline void RunExperiment1(int trials, std::uint64_t base_seed) {
    const std::string exp_csv = "exp1_insert_time.csv";
    const std::string unified_csv = "raw_data.csv";

    InitializeCSV(exp_csv, "exp,log2_n,n,structure,avg_total_insert_ms,avg_replications_r,trials");
    PrepareUnifiedRawCSV(unified_csv, 1);

    std::cout << "========================================================================\n"
              << "[Experiment 1] Total Time to Insert n Records (n = 2^0 .. 2^20)\n"
              << "  Structures : BST, AVL(delta=1), Treap, SkipList(p=1/2)\n"
              << "  Trials     : " << trials << " | Starting Seed : " << base_seed << "\n"
              << "  Outputs    : " << exp_csv << " & " << unified_csv << "\n"
              << "========================================================================\n";

    auto save_exp1_row = [&](int k, std::size_t n, const std::string& name,
                             double avg_ms, double avg_r) {
        std::ostringstream r1;
        r1 << "1," << k << "," << n << "," << name << ","
           << std::setprecision(9) << avg_ms << ","
           << std::fixed << std::setprecision(1) << avg_r << "," << trials;
        AppendCSVRow(exp_csv, r1.str());

        std::ostringstream ru;
        ru << "1," << k << "," << n << "," << name << ",ms,"
           << std::setprecision(9) << avg_ms << "," << trials;
        AppendCSVRow(unified_csv, ru.str());

        std::cout << "  n=2^" << std::setw(2) << k << " (" << std::setw(7) << n << ") | "
                  << std::left << std::setw(16) << name << std::right
                  << " | Avg Total Insert: " << std::setw(13) << std::setprecision(6)
                  << avg_ms << " ms"
                  << " (avg r=" << std::fixed << std::setprecision(1) << avg_r << ")\n";
        std::cout.unsetf(std::ios::floatfield);
    };

    for (int k = kMinExpPower; k <= kMaxExpPower; ++k) {
        const std::size_t n = (static_cast<std::size_t>(1) << k);

        double sum_bst = 0.0, sum_avl = 0.0, sum_trp = 0.0, sum_skp = 0.0;
        double rep_bst = 0.0, rep_avl = 0.0, rep_trp = 0.0, rep_skp = 0.0;

        for (int t = 0; t < trials; ++t) {
            TrialSeeds seeds = DeriveTrialSeeds(base_seed, k, t);
            std::vector<Record> inserts = GenerateInsertDataset(
                n, kMinID, kMaxID, kMinScore, kMaxScore, seeds.insert_seed);

            std::size_t r_used = 1;
            sum_bst += MeasureInsertTimeMsWithReplication<BST>(
                inserts, seeds.ds_seed,
                [](std::uint64_t) { return std::unique_ptr<BST>(new BST()); }, r_used);
            rep_bst += static_cast<double>(r_used);

            sum_avl += MeasureInsertTimeMsWithReplication<AVL>(
                inserts, seeds.ds_seed,
                [](std::uint64_t) { return std::unique_ptr<AVL>(new AVL(1)); }, r_used);
            rep_avl += static_cast<double>(r_used);

            sum_trp += MeasureInsertTimeMsWithReplication<Treap>(
                inserts, seeds.ds_seed,
                [](std::uint64_t s) { return std::unique_ptr<Treap>(new Treap(s)); }, r_used);
            rep_trp += static_cast<double>(r_used);

            sum_skp += MeasureInsertTimeMsWithReplication<SkipList>(
                inserts, seeds.ds_seed,
                [](std::uint64_t s) { return std::unique_ptr<SkipList>(new SkipList(0.5, s)); },
                r_used);
            rep_skp += static_cast<double>(r_used);
        }

        const double inv = 1.0 / static_cast<double>(trials);
        // Immediately flush all 4 averages for this `n` to CSV
        save_exp1_row(k, n, "BST",             sum_bst * inv, rep_bst * inv);
        save_exp1_row(k, n, "AVL(delta=1)",    sum_avl * inv, rep_avl * inv);
        save_exp1_row(k, n, "Treap",           sum_trp * inv, rep_trp * inv);
        save_exp1_row(k, n, "SkipList(p=1/2)", sum_skp * inv, rep_skp * inv);
    }

    std::cout << "[Experiment 1] Completed successfully.\n";
}

// ============================================================================
// Experiment 2: Average Single SearchAVG Time (n = 2^0 .. 2^20, Q = 100,000)
// ============================================================================

struct SearchTrialOutcome {
    double avg_query_ns;
    double return_sum;
    double hit_ratio;  // Fraction of Q queries where SearchAVG != -1 (PDF p. 5 Q2a)
};

template <typename DS>
SearchTrialOutcome MeasureSearchTrial(DS& ds, const std::vector<int>& queries) {
    // 1. Timed block: ONLY the Q SearchAVG queries
    double acc_sum = 0.0;
    SteadyTimer timer;
    timer.Start();
    for (std::size_t q = 0; q < queries.size(); ++q) {
        acc_sum += ds.SearchAVG(queries[q]);
    }
    timer.Stop();

    // 2. Post-timing pass (PDF p. 5 Q2a): re-run the same Q queries OUTSIDE timer
    //    to count how many searches found an existing ID (return value != -1).
    std::size_t hit_count = 0;
    for (std::size_t q = 0; q < queries.size(); ++q) {
        if (ds.SearchAVG(queries[q]) != -1.0) {
            ++hit_count;
        }
    }

    SearchTrialOutcome out{};
    out.avg_query_ns = timer.ElapsedNanoseconds() / static_cast<double>(queries.size());
    out.return_sum = acc_sum;
    out.hit_ratio = static_cast<double>(hit_count) / static_cast<double>(queries.size());
    return out;
}

inline void RunExperiment2(int trials, std::uint64_t base_seed) {
    const std::string exp_csv = "exp2_search_time.csv";
    const std::string hit_csv = "exp2_hit_ratio.csv";
    const std::string unified_csv = "raw_data.csv";

    InitializeCSV(exp_csv,
                  "exp,log2_n,n,structure,avg_search_ns,avg_hit_ratio,checksum_sum,trials");
    InitializeCSV(hit_csv, "log2_n,n,queries_Q,avg_hit_count,avg_hit_ratio");
    PrepareUnifiedRawCSV(unified_csv, 2);

    std::cout << "========================================================================\n"
              << "[Experiment 2] Average Time per SearchAVG (n = 2^0 .. 2^20, Q = "
              << kQueryBatchSize << ")\n"
              << "  Structures : BST, AVL(delta=1), Treap, SkipList(p=1/2)\n"
              << "  Trials     : " << trials << " | Starting Seed : " << base_seed << "\n"
              << "  Outputs    : " << exp_csv << ", " << hit_csv << " & " << unified_csv << "\n"
              << "========================================================================\n";

    double grand_total_checksum = 0.0;

    auto save_exp2_row = [&](int k, std::size_t n, const std::string& name,
                             double avg_ns, double avg_hit_ratio, double sum_ret) {
        std::ostringstream r2;
        r2 << "2," << k << "," << n << "," << name << ","
           << std::fixed << std::setprecision(4) << avg_ns << ","
           << std::setprecision(6) << avg_hit_ratio << ","
           << std::setprecision(2) << sum_ret << "," << trials;
        AppendCSVRow(exp_csv, r2.str());

        std::ostringstream ru;
        ru << "2," << k << "," << n << "," << name << ",ns,"
           << std::fixed << std::setprecision(4) << avg_ns << "," << trials;
        AppendCSVRow(unified_csv, ru.str());

        // Print both average search time AND accumulated return sum (anti-O2 requirement)
        std::cout << "  n=2^" << std::setw(2) << k << " (" << std::setw(7) << n << ") | "
                  << std::left << std::setw(16) << name << std::right
                  << " | Avg Search: " << std::fixed << std::setprecision(2) << std::setw(8)
                  << avg_ns << " ns"
                  << " | Hit%: " << std::setprecision(3) << std::setw(6)
                  << (avg_hit_ratio * 100.0) << "%"
                  << " | Sum: " << std::setprecision(1) << sum_ret << "\n";
    };

    for (int k = kMinExpPower; k <= kMaxExpPower; ++k) {
        const std::size_t n = (static_cast<std::size_t>(1) << k);

        double ns_bst = 0.0, ns_avl = 0.0, ns_trp = 0.0, ns_skp = 0.0;
        double sum_bst = 0.0, sum_avl = 0.0, sum_trp = 0.0, sum_skp = 0.0;
        double hit_ratio_acc = 0.0;

        for (int t = 0; t < trials; ++t) {
            TrialSeeds seeds = DeriveTrialSeeds(base_seed, k, t);
            std::vector<Record> inserts = GenerateInsertDataset(
                n, kMinID, kMaxID, kMinScore, kMaxScore, seeds.insert_seed);
            std::vector<int> queries = GenerateQueryDataset(
                kQueryBatchSize, kMinID, kMaxID, seeds.query_seed);

            // 1. BST (build un-timed, time Q searches, free at block exit)
            {
                BST ds;
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertBST(inserts[j].id, inserts[j].score);
                }
                SearchTrialOutcome out = MeasureSearchTrial(ds, queries);
                ns_bst += out.avg_query_ns;
                sum_bst += out.return_sum;
                hit_ratio_acc += out.hit_ratio;
            }

            // 2. AVL(delta=1)
            {
                AVL ds(1);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertAVL(inserts[j].id, inserts[j].score);
                }
                SearchTrialOutcome out = MeasureSearchTrial(ds, queries);
                ns_avl += out.avg_query_ns;
                sum_avl += out.return_sum;
            }

            // 3. Treap
            {
                Treap ds(seeds.ds_seed);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertTreap(inserts[j].id, inserts[j].score);
                }
                SearchTrialOutcome out = MeasureSearchTrial(ds, queries);
                ns_trp += out.avg_query_ns;
                sum_trp += out.return_sum;
            }

            // 4. SkipList(p=1/2)
            {
                SkipList ds(0.5, seeds.ds_seed);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertSkipList(inserts[j].id, inserts[j].score);
                }
                SearchTrialOutcome out = MeasureSearchTrial(ds, queries);
                ns_skp += out.avg_query_ns;
                sum_skp += out.return_sum;
            }
        }

        const double inv = 1.0 / static_cast<double>(trials);
        const double avg_hit_ratio = hit_ratio_acc * inv;
        grand_total_checksum += (sum_bst + sum_avl + sum_trp + sum_skp);

        save_exp2_row(k, n, "BST",             ns_bst * inv, avg_hit_ratio, sum_bst);
        save_exp2_row(k, n, "AVL(delta=1)",    ns_avl * inv, avg_hit_ratio, sum_avl);
        save_exp2_row(k, n, "Treap",           ns_trp * inv, avg_hit_ratio, sum_trp);
        save_exp2_row(k, n, "SkipList(p=1/2)", ns_skp * inv, avg_hit_ratio, sum_skp);

        // Save Q2(a) Hit-Ratio table row immediately for this `n`
        std::ostringstream hr;
        hr << k << "," << n << "," << kQueryBatchSize << ","
           << std::fixed << std::setprecision(1) << (avg_hit_ratio * kQueryBatchSize) << ","
           << std::setprecision(6) << avg_hit_ratio;
        AppendCSVRow(hit_csv, hr.str());
    }

    std::cout << "  [Anti-O2 Total SearchAVG Return Sum] = "
              << std::fixed << std::setprecision(2) << grand_total_checksum << "\n"
              << "[Experiment 2] Completed successfully.\n";
}

// ============================================================================
// Experiment 3: Average Height of 7 Structures (n = 2^0 .. 2^20) + Q1-Q4 Tables
// ============================================================================

inline int ComputeMaxAVLHeightForNodes(int delta, std::uint64_t max_nodes,
                                       std::vector<std::uint64_t>& min_nodes_out) {
    // Recurrence for minimum nodes S_h of a height-h AVL(delta) tree:
    //   For 1 <= h <= delta + 1: S_h = h  (a chain of length h has |bf| <= h - 1 <= delta)
    //   For h > delta + 1      : S_h = S_{h-1} + S_{h - 1 - delta} + 1
    min_nodes_out.clear();
    min_nodes_out.push_back(0);  // S_0 = 0
    int h = 0;
    while (true) {
        int next_h = h + 1;
        std::uint64_t s_next = 0;
        if (next_h <= delta + 1) {
            s_next = static_cast<std::uint64_t>(next_h);
        } else {
            s_next = min_nodes_out[static_cast<std::size_t>(next_h - 1)] +
                     min_nodes_out[static_cast<std::size_t>(next_h - 1 - delta)] + 1ULL;
        }
        if (s_next > max_nodes) {
            break;
        }
        min_nodes_out.push_back(s_next);
        h = next_h;
    }
    return h;
}

inline void RunExperiment3(int trials, std::uint64_t base_seed) {
    const std::string exp_csv = "exp3_height.csv";
    const std::string unified_csv = "raw_data.csv";
    const std::string report_txt = "exp3_report_tables.txt";

    InitializeCSV(exp_csv, "exp,log2_n,n,structure,avg_height,trials");
    PrepareUnifiedRawCSV(unified_csv, 3);

    const std::vector<std::string> names = {
        "BST",
        "AVL(delta=1)",
        "AVL(delta=5)",
        "Treap",
        "SkipList(p=1/2)",
        "SkipList(p=3/4)",
        "SkipList(p=1/4)"
    };

    std::cout << "========================================================================\n"
              << "[Experiment 3] Average Height of 7 Data Structures (n = 2^0 .. 2^20)\n"
              << "  Structures : BST, AVL(1), AVL(5), Treap, SkipList(1/2, 3/4, 1/4)\n"
              << "  Trials     : " << trials << " | Starting Seed : " << base_seed << "\n"
              << "  Outputs    : " << exp_csv << ", " << unified_csv << " & " << report_txt << "\n"
              << "========================================================================\n";

    std::vector<double> height_at_2_10(7, 0.0);
    std::vector<double> height_at_2_20(7, 0.0);

    // Supplementary counters at n = 2^20 for Report Q3(d) and Q4(a)-(b)
    double sum_rot_avl1_2_20 = 0.0;
    double sum_rot_avl5_2_20 = 0.0;
    double sum_steps_p12_2_20 = 0.0;
    double sum_steps_p34_2_20 = 0.0;
    double sum_steps_p14_2_20 = 0.0;

    for (int k = kMinExpPower; k <= kMaxExpPower; ++k) {
        const std::size_t n = (static_cast<std::size_t>(1) << k);
        std::vector<double> h_sums(7, 0.0);

        for (int t = 0; t < trials; ++t) {
            TrialSeeds seeds = DeriveTrialSeeds(base_seed, k, t);
            std::vector<Record> inserts = GenerateInsertDataset(
                n, kMinID, kMaxID, kMinScore, kMaxScore, seeds.insert_seed);

            // For k == 20, also generate Q = 100,000 queries for Q4(a) SkipList step count
            std::vector<int> queries_2_20;
            if (k == 20) {
                queries_2_20 = GenerateQueryDataset(
                    kQueryBatchSize, kMinID, kMaxID, seeds.query_seed);
            }

            // 0: BST
            {
                BST ds;
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertBST(inserts[j].id, inserts[j].score);
                }
                h_sums[0] += static_cast<double>(ds.HeightBST());
            }

            // 1: AVL(delta=1)
            {
                AVL ds(1);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertAVL(inserts[j].id, inserts[j].score);
                }
                h_sums[1] += static_cast<double>(ds.HeightAVL());
                if (k == 20) {
                    sum_rot_avl1_2_20 += static_cast<double>(ds.GetRotationCount());
                }
            }

            // 2: AVL(delta=5)
            {
                AVL ds(5);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertAVL(inserts[j].id, inserts[j].score);
                }
                h_sums[2] += static_cast<double>(ds.HeightAVL());
                if (k == 20) {
                    sum_rot_avl5_2_20 += static_cast<double>(ds.GetRotationCount());
                }
            }

            // 3: Treap
            {
                Treap ds(seeds.ds_seed);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertTreap(inserts[j].id, inserts[j].score);
                }
                h_sums[3] += static_cast<double>(ds.HeightTreap());
            }

            // 4: SkipList(p = 1/2)
            {
                SkipList ds(0.5, seeds.ds_seed);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertSkipList(inserts[j].id, inserts[j].score);
                }
                h_sums[4] += static_cast<double>(ds.HeightSkipList());
                if (k == 20) {
                    std::uint64_t total_steps = 0;
                    for (std::size_t q = 0; q < queries_2_20.size(); ++q) {
                        std::uint64_t st = 0;
                        ds.SearchAVGWithStepsSkipList(queries_2_20[q], st);
                        total_steps += st;
                    }
                    sum_steps_p12_2_20 +=
                        static_cast<double>(total_steps) / static_cast<double>(queries_2_20.size());
                }
            }

            // 5: SkipList(p = 3/4)
            {
                SkipList ds(0.75, seeds.ds_seed);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertSkipList(inserts[j].id, inserts[j].score);
                }
                h_sums[5] += static_cast<double>(ds.HeightSkipList());
                if (k == 20) {
                    std::uint64_t total_steps = 0;
                    for (std::size_t q = 0; q < queries_2_20.size(); ++q) {
                        std::uint64_t st = 0;
                        ds.SearchAVGWithStepsSkipList(queries_2_20[q], st);
                        total_steps += st;
                    }
                    sum_steps_p34_2_20 +=
                        static_cast<double>(total_steps) / static_cast<double>(queries_2_20.size());
                }
            }

            // 6: SkipList(p = 1/4)
            {
                SkipList ds(0.25, seeds.ds_seed);
                for (std::size_t j = 0; j < inserts.size(); ++j) {
                    ds.InsertSkipList(inserts[j].id, inserts[j].score);
                }
                h_sums[6] += static_cast<double>(ds.HeightSkipList());
                if (k == 20) {
                    std::uint64_t total_steps = 0;
                    for (std::size_t q = 0; q < queries_2_20.size(); ++q) {
                        std::uint64_t st = 0;
                        ds.SearchAVGWithStepsSkipList(queries_2_20[q], st);
                        total_steps += st;
                    }
                    sum_steps_p14_2_20 +=
                        static_cast<double>(total_steps) / static_cast<double>(queries_2_20.size());
                }
            }
        }

        const double inv = 1.0 / static_cast<double>(trials);
        std::cout << "  n=2^" << std::setw(2) << k << " (" << std::setw(7) << n << "): ";
        for (std::size_t idx = 0; idx < 7; ++idx) {
            double avg_h = h_sums[idx] * inv;
            if (k == 10) height_at_2_10[idx] = avg_h;
            if (k == 20) height_at_2_20[idx] = avg_h;

            std::ostringstream r3;
            r3 << "3," << k << "," << n << "," << names[idx] << ","
               << std::fixed << std::setprecision(4) << avg_h << "," << trials;
            AppendCSVRow(exp_csv, r3.str());

            std::ostringstream ru;
            ru << "3," << k << "," << n << "," << names[idx] << ",levels,"
               << std::fixed << std::setprecision(4) << avg_h << "," << trials;
            AppendCSVRow(unified_csv, ru.str());

            std::cout << names[idx] << "=" << std::fixed << std::setprecision(2) << avg_h;
            if (idx + 1 < 7) std::cout << ", ";
        }
        std::cout << "\n";
    }

    // ------------------------------------------------------------------------
    // Supplementary Report Tables for Experiment 3 Questions 1, 2, 3, and 4
    // ------------------------------------------------------------------------
    const double inv = 1.0 / static_cast<double>(trials);

    // Exp 3 Q2: Sequential input id = 1, 2, ..., 2^10 (n = 1024)
    const std::size_t seq_n = 1024;
    std::vector<double> seq_h_sums(7, 0.0);
    for (int t = 0; t < trials; ++t) {
        TrialSeeds seeds = DeriveTrialSeeds(base_seed, 10, t);
        std::vector<Record> seq_data =
            GenerateSequentialDataset(seq_n, kMinScore, kMaxScore, seeds.insert_seed);

        {
            BST ds;
            for (std::size_t j = 0; j < seq_n; ++j) ds.InsertBST(seq_data[j].id, seq_data[j].score);
            seq_h_sums[0] += ds.HeightBST();
        }
        {
            AVL ds(1);
            for (std::size_t j = 0; j < seq_n; ++j) ds.InsertAVL(seq_data[j].id, seq_data[j].score);
            seq_h_sums[1] += ds.HeightAVL();
        }
        {
            AVL ds(5);
            for (std::size_t j = 0; j < seq_n; ++j) ds.InsertAVL(seq_data[j].id, seq_data[j].score);
            seq_h_sums[2] += ds.HeightAVL();
        }
        {
            Treap ds(seeds.ds_seed);
            for (std::size_t j = 0; j < seq_n; ++j) ds.InsertTreap(seq_data[j].id, seq_data[j].score);
            seq_h_sums[3] += ds.HeightTreap();
        }
        {
            SkipList ds(0.5, seeds.ds_seed);
            for (std::size_t j = 0; j < seq_n; ++j) ds.InsertSkipList(seq_data[j].id, seq_data[j].score);
            seq_h_sums[4] += ds.HeightSkipList();
        }
        {
            SkipList ds(0.75, seeds.ds_seed);
            for (std::size_t j = 0; j < seq_n; ++j) ds.InsertSkipList(seq_data[j].id, seq_data[j].score);
            seq_h_sums[5] += ds.HeightSkipList();
        }
        {
            SkipList ds(0.25, seeds.ds_seed);
            for (std::size_t j = 0; j < seq_n; ++j) ds.InsertSkipList(seq_data[j].id, seq_data[j].score);
            seq_h_sums[6] += ds.HeightSkipList();
        }
    }

    // Exp 3 Q3(a) & Q3(b): Recurrence S_h worst-case height bounds at 2^20 nodes
    std::vector<std::uint64_t> s_d1, s_d5;
    int max_h_d1 = ComputeMaxAVLHeightForNodes(1, 1ULL << 20, s_d1);
    int max_h_d5 = ComputeMaxAVLHeightForNodes(5, 1ULL << 20, s_d5);

    std::ostringstream summary;
    summary << "\n========================================================================\n"
            << "[Experiment 3 Report Helper Tables]\n"
            << "========================================================================\n\n"
            << "1) [Q1] Empirical Slope dh / d(log2 n) from n = 2^10 to n = 2^20:\n";
    for (std::size_t idx = 0; idx < 7; ++idx) {
        double slope = (height_at_2_20[idx] - height_at_2_10[idx]) / 10.0;
        summary << "   - " << std::left << std::setw(16) << names[idx] << std::right
                << ": h(2^10)=" << std::fixed << std::setprecision(2) << std::setw(6)
                << height_at_2_10[idx]
                << ", h(2^20)=" << std::setw(6) << height_at_2_20[idx]
                << " => slope = " << std::setprecision(4) << slope << "\n";
    }

    summary << "\n2) [Q2] Sequential Input id = 1, 2, ..., 2^10 (n = 1024) Average Heights:\n";
    for (std::size_t idx = 0; idx < 7; ++idx) {
        summary << "   - " << std::left << std::setw(16) << names[idx] << std::right
                << ": avg_height = " << std::fixed << std::setprecision(2)
                << (seq_h_sums[idx] * inv) << "\n";
    }

    summary << "\n3) [Q3] AVL(delta=1) vs AVL(delta=5) at n = 2^20:\n"
            << "   - Theoretical max height for 2^20 nodes (via S_h recurrence):\n"
            << "       AVL(delta=1): h_max = " << max_h_d1
            << " (S_" << max_h_d1 << " = " << s_d1[static_cast<std::size_t>(max_h_d1)] << ")\n"
            << "       AVL(delta=5): h_max = " << max_h_d5
            << " (S_" << max_h_d5 << " = " << s_d5[static_cast<std::size_t>(max_h_d5)]
            << "; S_1..S_6 = 1, 2, 3, 4, 5, 6)\n"
            << "   - Measured avg height at n = 2^20:\n"
            << "       AVL(delta=1): " << std::fixed << std::setprecision(2)
            << height_at_2_20[1] << "\n"
            << "       AVL(delta=5): " << height_at_2_20[2] << "\n"
            << "   - Measured avg rotation count at n = 2^20 (double rotation = 2):\n"
            << "       AVL(delta=1): " << std::setprecision(1) << (sum_rot_avl1_2_20 * inv) << "\n"
            << "       AVL(delta=5): " << (sum_rot_avl5_2_20 * inv) << "\n";

    summary << "\n4) [Q4] SkipList Height & Average Search Steps (Q = " << kQueryBatchSize
            << ") at n = 2^20:\n"
            << "   - SkipList(p=1/4): avg_height = " << std::fixed << std::setprecision(2)
            << height_at_2_20[6] << ", avg_search_steps = " << std::setprecision(2)
            << (sum_steps_p14_2_20 * inv) << "\n"
            << "   - SkipList(p=1/2): avg_height = " << height_at_2_20[4]
            << ", avg_search_steps = " << (sum_steps_p12_2_20 * inv) << "\n"
            << "   - SkipList(p=3/4): avg_height = " << height_at_2_20[5]
            << ", avg_search_steps = " << (sum_steps_p34_2_20 * inv) << "\n";

    std::cout << summary.str();

    std::ofstream rpt(report_txt, std::ios::out | std::ios::trunc);
    if (rpt.is_open()) {
        rpt << summary.str();
        rpt.flush();
    }

    std::cout << "\n[Experiment 3] Completed successfully. Report tables saved to "
              << report_txt << ".\n";
}

// ============================================================================
// Correctness Test Suite (--test): Exact 5 PDF Sets + Custom + Parameter Stress
// ============================================================================

inline bool ApproxEqual(double a, double b, double eps = 1e-9) noexcept {
    return std::fabs(a - b) <= eps;
}

inline void RunCorrectnessTests(std::uint64_t seed) {
    MemoryTracker::ResetCounters();
    int failed_checks = 0;

    auto check = [&](bool cond, const std::string& msg) {
        if (!cond) {
            std::cerr << "  [FAIL] " << msg << "\n";
            ++failed_checks;
        } else {
            std::cout << "  [PASS] " << msg << "\n";
        }
    };

    std::cout << "========================================================================\n"
              << "Correctness Test Suite (--test): 5 Hand-Calculable PDF Sets + Stress Tests\n"
              << "========================================================================\n\n";

    // ------------------------------------------------------------------------
    // PDF Test 1: BST -- Insert (3,100), (2,60), (1,70), (5,40), (4,70)
    // ------------------------------------------------------------------------
    std::cout << "=== [Test 1] BST: (3,100), (2,60), (1,70), (5,40), (4,70) ===\n";
    {
        BST bst;
        const Record input1[] = {{3, 100}, {2, 60}, {1, 70}, {5, 40}, {4, 70}};
        for (std::size_t i = 0; i < 5; ++i) {
            bst.InsertBST(input1[i].id, input1[i].score);
        }
        bst.PrintBST();
        std::cout << "HeightBST = " << bst.HeightBST() << "\n";
        check(bst.HeightBST() == 3 && bst.VerifyBST(),
              "BST Test 1 matches hand calculation (Height == 3, valid BST)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // PDF Test 2: AVL (delta = 1) -- Insert (1,100), (2,60), (3,70), (4,40), (5,70)
    //   Expected step-by-step rotations:
    //     - Insert #3 (3, 70): RR rotation at node id=1
    //     - Insert #5 (5, 70): RR rotation at node id=3
    // ------------------------------------------------------------------------
    std::cout << "=== [Test 2] AVL (delta=1): (1,100), (2,60), (3,70), (4,40), (5,70) ===\n";
    {
        AVL avl(1);
        avl.SetVerboseRotations(true);
        const Record input2[] = {{1, 100}, {2, 60}, {3, 70}, {4, 40}, {5, 70}};
        for (std::size_t i = 0; i < 5; ++i) {
            avl.InsertAVL(input2[i].id, input2[i].score);
        }
        avl.PrintAVL();
        std::cout << "HeightAVL = " << avl.HeightAVL()
                  << " (Total single-rotations performed = " << avl.GetRotationCount() << ")\n";

        const std::vector<AVL::RotationLogEntry>& logs = avl.GetRotationLogs();
        bool logs_match = (logs.size() == 2) &&
                          (logs[0].insert_step == 3 && logs[0].inserted_id == 3 &&
                           logs[0].inserted_score == 70 && logs[0].pivot_id == 1 &&
                           logs[0].type == "RR") &&
                          (logs[1].insert_step == 5 && logs[1].inserted_id == 5 &&
                           logs[1].inserted_score == 70 && logs[1].pivot_id == 3 &&
                           logs[1].type == "RR");
        check(avl.HeightAVL() == 3 && avl.GetRotationCount() == 2 &&
                  logs_match && avl.VerifyAVL(),
              "AVL Test 2 matches hand calculation: Insert #3 (3,70) RR at id=1, "
              "Insert #5 (5,70) RR at id=3, Height == 3");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // PDF Test 3: Treap (Min-Heap) --
    //   (3,100)[0.9], (2,60)[0.5], (1,70)[0.3], (5,60)[0.2], (4,80)[0.1]
    // ------------------------------------------------------------------------
    std::cout << "=== [Test 3] Treap (Min-Heap): "
              << "(3,100)[0.9], (2,60)[0.5], (1,70)[0.3], (5,60)[0.2], (4,80)[0.1] ===\n";
    {
        Treap treap;
        treap.InsertTreap(3, 100, 0.9);
        treap.InsertTreap(2, 60,  0.5);
        treap.InsertTreap(1, 70,  0.3);
        treap.InsertTreap(5, 60,  0.2);
        treap.InsertTreap(4, 80,  0.1);

        treap.PrintTreap();
        std::cout << "HeightTreap = " << treap.HeightTreap() << "\n";
        check(treap.HeightTreap() == 4 && treap.VerifyTreap(),
              "Treap Test 3 matches hand calculation (Min-Heap root id=4[0.1], Height == 4)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // PDF Test 4: SkipList (p = 1/2) --
    //   (1,100)[HHT], (2,60)[T], (3,70)[HT], (4,80)[HHHT],
    //   (5,60)[T], (6,80)[T], (7,90)[HHT]
    // ------------------------------------------------------------------------
    std::cout << "=== [Test 4] SkipList (p=1/2): "
              << "(1,100)[HHT], (2,60)[T], (3,70)[HT], (4,80)[HHHT], "
              << "(5,60)[T], (6,80)[T], (7,90)[HHT] ===\n";
    {
        SkipList sl(0.5, 7);  // Also verifies SkipList(double, int) has zero ambiguity
        sl.InsertSkipList(1, 100, "HHT");
        sl.InsertSkipList(2, 60,  "T");
        sl.InsertSkipList(3, 70,  "HT");
        sl.InsertSkipList(4, 80,  "HHHT");
        sl.InsertSkipList(5, 60,  "T");
        sl.InsertSkipList(6, 80,  "T");
        sl.InsertSkipList(7, 90,  "HHT");

        sl.PrintSkipList();
        std::cout << "HeightSkipList = " << sl.HeightSkipList() << "\n";
        check(sl.HeightSkipList() == 4 && sl.VerifySkipList(),
              "SkipList Test 4 matches hand calculation (Height == 4, [HHT]=3 levels)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // PDF Test 5: SearchAVG across all 4 structures --
    //   Insert: (1,80),(2,70),(1,60),(3,90),(1,91),(4,50),(5,40),(2,35)
    //   Query : id = 1, 2, 3, 6
    //   Hand calculation:
    //     id=1 -> (80 + 60 + 91) / 3 = 231 / 3 = 77.00
    //     id=2 -> (70 + 35) / 2      = 105 / 2 = 52.50
    //     id=3 -> 90 / 1             = 90.00
    //     id=6 -> -1 (never inserted)
    // ------------------------------------------------------------------------
    std::cout << "=== [Test 5] SearchAVG on All 4 Structures (Query id = 1, 2, 3, 6) ===\n";
    {
        BST bst;
        AVL avl(1);
        Treap treap(42ULL);
        SkipList sl(0.5, 42ULL);

        const Record input5[] = {
            {1, 80}, {2, 70}, {1, 60}, {3, 90},
            {1, 91}, {4, 50}, {5, 40}, {2, 35}
        };
        for (std::size_t i = 0; i < 8; ++i) {
            bst.InsertBST(input5[i].id, input5[i].score);
            avl.InsertAVL(input5[i].id, input5[i].score);
            treap.InsertTreap(input5[i].id, input5[i].score);
            sl.InsertSkipList(input5[i].id, input5[i].score);
        }

        const int qids[] = {1, 2, 3, 6};
        const double expected[] = {77.0, 52.5, 90.0, -1.0};

        std::cout << "  Query ID | Hand Expected |      BST |      AVL |    Treap | SkipList\n"
                  << "  ---------+---------------+----------+----------+----------+----------\n";
        bool all_ok = true;
        for (int idx = 0; idx < 4; ++idx) {
            int q = qids[idx];
            double b = bst.SearchAVGBST(q);
            double a = avl.SearchAVGAVL(q);
            double t = treap.SearchAVGTreap(q);
            double s = sl.SearchAVGSkipList(q);

            std::cout << "  " << std::setw(8) << q << " | "
                      << std::fixed << std::setprecision(2)
                      << std::setw(13) << expected[idx] << " | "
                      << std::setw(8) << b << " | "
                      << std::setw(8) << a << " | "
                      << std::setw(8) << t << " | "
                      << std::setw(8) << s << "\n";

            if (!ApproxEqual(b, expected[idx]) ||
                !ApproxEqual(a, expected[idx]) ||
                !ApproxEqual(t, expected[idx]) ||
                !ApproxEqual(s, expected[idx])) {
                all_ok = false;
            }
        }
        check(all_ok,
              "All 4 structures match hand-calculated SearchAVG (77.0, 52.5, 90.0, -1.0)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Custom Test 6 (Required by PDF p. 4):
    //   Targets three subtle bugs simultaneously:
    //   (a) AVL Left-Right (LR) and Right-Left (RL) double rotations combined
    //       with duplicate-ID updates both BEFORE and AFTER double rotations.
    //   (b) Treap duplicate-ID insertion with a smaller priority (verifies that
    //       duplicate IDs update the existing node in place instead of creating
    //       a second node or corrupting parent/child pointers).
    //   (c) SkipList unbounded height (> 32 levels) to ensure no hardcoded cap.
    // ------------------------------------------------------------------------
    std::cout << "=== [Test 6] Custom Edge-Case Test (LR/RL Double Rotations, "
              << "Duplicate Priority Trap & >32-Level SkipList) ===\n"
              << "  [Bug Targeted]: Catches (1) losing/misplacing duplicate-ID nodes across "
              << "AVL LR/RL double rotations, (2) corrupting Treap Min-Heap/parent links when "
              << "a duplicate ID arrives with a smaller priority, and (3) hardcoded 32-level "
              << "SkipList caps.\n";
    {
        BST bst;
        AVL avl(1);
        avl.SetVerboseRotations(true);
        Treap treap;
        SkipList sl(0.5);

        struct CustomOp {
            int id;
            int score;
            double prio;
            const char* coins;
        };
        const CustomOp ops[] = {
            {30,  60, 0.50, "HT"},
            {10,  80, 0.70, "T"},
            {20, 100, 0.20, "HHT"},
            {20,  50, 0.01, "HHHHT"},  // Duplicate id=20 with tiny prio & tall coins!
            {50,  40, 0.80, "T"},
            {40,  90, 0.15, "HT"},
            {10,  70, 0.05, "HHHT"}    // Duplicate id=10 after rotations!
        };

        for (std::size_t i = 0; i < 7; ++i) {
            bst.InsertBST(ops[i].id, ops[i].score);
            avl.InsertAVL(ops[i].id, ops[i].score);
            treap.InsertTreap(ops[i].id, ops[i].score, ops[i].prio);
            sl.InsertSkipList(ops[i].id, ops[i].score, ops[i].coins);
        }

        avl.PrintAVL();
        treap.PrintTreap();

        // Also insert id=99 with 40 consecutive 'H's (41 levels > 32) into SkipList
        std::string forty_heads(40, 'H');
        forty_heads.push_back('T');
        sl.InsertSkipList(99, 88, forty_heads);

        check(avl.VerifyAVL() && treap.VerifyTreap() && bst.VerifyBST() && sl.VerifySkipList(),
              "All invariants (LR/RL AVL balance, Treap parent+min-heap, SkipList) hold");
        check(ApproxEqual(avl.SearchAVGAVL(20), 75.0) &&
                  ApproxEqual(treap.SearchAVGTreap(20), 75.0) &&
                  ApproxEqual(sl.SearchAVGSkipList(20), 75.0) &&
                  ApproxEqual(avl.SearchAVGAVL(10), 75.0) &&
                  ApproxEqual(treap.SearchAVGTreap(10), 75.0) &&
                  ApproxEqual(sl.SearchAVGSkipList(10), 75.0),
              "Duplicate IDs updated accurately across LR/RL rotations and priority traps");
        check(sl.HeightSkipList() == 41 && ApproxEqual(sl.SearchAVGSkipList(99), 88.0),
              "SkipList scales past 32 levels (Height == 41) with no artificial cap");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Test 7: Parameterized AVL (delta = 2, 3, 5) & SkipList (p = 1/4, 3/4)
    //         High-Volume Invariant Verification
    // ------------------------------------------------------------------------
    std::cout << "=== [Test 7] Parameterized AVL (delta=2,3,5) & SkipList (p=1/4, 3/4) "
              << "Large-Scale Verification ===\n";
    {
        constexpr std::size_t kAvlStressN = 20000;
        std::cout << "  [Test 7A Target]: Inserts " << kAvlStressN << " random records AND "
                  << kAvlStressN << " ascending records (id = 1.." << kAvlStressN << ") into\n"
                  << "    AVL trees with delta = 2, 3, 5. Catches bugs where (i) |bf| > 1 is\n"
                  << "    hardcoded instead of |bf| > delta, (ii) subtree heights are not\n"
                  << "    updated when |bf| <= delta, or (iii) single vs. double rotation\n"
                  << "    selection leaves a child with |bf| > delta under relaxed balance.\n";

        const std::vector<Record> rand_20k =
            GenerateInsertDataset(kAvlStressN, 1, 10000, 0, 100, MixSeed(seed ^ 0xABCDEFULL));
        const std::vector<Record> asc_20k =
            GenerateSequentialDataset(kAvlStressN, 0, 100, MixSeed(seed ^ 0xFEDCBAULL));

        const int deltas[] = {2, 3, 5};
        for (std::size_t di = 0; di < 3; ++di) {
            int d = deltas[di];
            AVL avl_rand(d);
            AVL avl_asc(d);
            for (std::size_t i = 0; i < kAvlStressN; ++i) {
                avl_rand.InsertAVL(rand_20k[i].id, rand_20k[i].score);
                avl_asc.InsertAVL(asc_20k[i].id, asc_20k[i].score);
            }
            bool ok_rand = avl_rand.VerifyAVL();
            bool ok_asc = avl_asc.VerifyAVL();
            std::ostringstream msg;
            msg << "AVL(delta=" << d << ") VerifyAVL() == true on 20,000 random (h="
                << avl_rand.HeightAVL() << ") & 20,000 ascending (h="
                << avl_asc.HeightAVL() << ") insertions";
            check(ok_rand && ok_asc, msg.str());
        }

        constexpr std::size_t kSkipStressN = 50000;
        std::cout << "\n  [Test 7B Target]: Inserts " << kSkipStressN
                  << " records into SkipList with p = 1/4 and p = 3/4.\n"
                  << "    Catches bugs where (i) coin probability p = 1/2 is hardcoded,\n"
                  << "    (ii) a fixed MAX_LEVEL cap (e.g. 32) truncates tall towers under\n"
                  << "    p = 3/4 or causes out-of-bounds level access, or (iii) multi-level\n"
                  << "    forward pointers are spliced out of sorted order.\n";

        const std::vector<Record> rand_50k =
            GenerateInsertDataset(kSkipStressN, 1, kMaxID, 0, 100, MixSeed(seed ^ 0x777777ULL));
        const double probs[] = {0.25, 0.75};
        const char* prob_labels[] = {"1/4", "3/4"};
        for (std::size_t pi = 0; pi < 2; ++pi) {
            SkipList sl(probs[pi], MixSeed(seed + pi + 99));
            for (std::size_t i = 0; i < kSkipStressN; ++i) {
                sl.InsertSkipList(rand_50k[i].id, rand_50k[i].score);
            }
            bool ok_sl = sl.VerifySkipList();
            std::ostringstream msg;
            msg << "SkipList(p=" << prob_labels[pi] << ") VerifySkipList() == true on "
                << kSkipStressN << " insertions (Height = " << sl.HeightSkipList() << ")";
            check(ok_sl, msg.str());
        }
    }
    std::cout << "\n";

    // Verify post-order destructors freed 100% of allocated nodes
    std::cout << "=== [Memory Deallocation Audit] ===\n";
    if (MemoryTracker::kEnabled) {
        std::cout << "  Total nodes allocated   : " << MemoryTracker::TotalAllocations() << "\n"
                  << "  Total nodes deallocated : " << MemoryTracker::TotalDeallocations() << "\n"
                  << "  Live nodes remaining    : " << MemoryTracker::LiveNodes() << "\n";
        check(MemoryTracker::LiveNodes() == 0 &&
                  MemoryTracker::TotalAllocations() == MemoryTracker::TotalDeallocations() &&
                  MemoryTracker::TotalAllocations() > 0,
              "Post-order destructors freed 100% of allocated nodes (0 leaks)");
    } else {
        std::cout << "  (Compiled with -DNO_MEMORY_TRACKER for zero-overhead benchmarking;\n"
                  << "   compile without -DNO_MEMORY_TRACKER via `make` to audit node counts.)\n";
    }

    std::cout << "\n========================================================================\n";
    if (failed_checks == 0) {
        std::cout << "ALL 7 CORRECTNESS TEST SUITES & MEMORY CHECKS PASSED!\n";
    } else {
        std::cerr << failed_checks << " CHECK(S) FAILED!\n";
        std::exit(EXIT_FAILURE);
    }
    std::cout << "========================================================================\n";
}

}  // namespace algo_hw2

#endif  // EXPERIMENTS_H
