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

// Default benchmark constants (n = 2^10 .. 2^20, ID in [1, 2^20], score in [0, 100], Q = 100,000)
constexpr int kMinExpPower = 10;
// PDF Specification: n = 2^0, 2^1, ..., 2^20; id ~ UNI(1, 2^20); score ~ UNI(0, 100); Q = 100,000
constexpr int kMinExpPower = 0;
constexpr int kMaxExpPower = 20;
constexpr int kMinID = 1;
constexpr int kMaxID = (1 << 20);  // 1,048,576
constexpr int kMinScore = 0;
constexpr int kMaxScore = 100;
constexpr std::size_t kQueryBatchSize = 100000;

/**
 * @brief Holds timing and structural measurements from a single benchmark trial.
 * @brief Deterministic seed derivation for (power_k, trial_i) from `base_seed`.
 *        Follows PDF p. 3 ("starting_seed + i" mixed per n) so:
 *          1. Within the same (n, trial_i), ALL data structures receive the exact
 *             same insertion records and query IDs.
 *          2. Running twice with the same `--seed` reproduces identical results.
 */
struct TrialMetrics {
    double insert_total_ms;
    double insert_per_op_ns;
    double search_batch_ms;
    double search_per_query_ns;
    int height;
struct TrialSeeds {
    std::uint64_t insert_seed;
    std::uint64_t query_seed;
    std::uint64_t ds_seed;
};

/**
 * @brief Holds averaged metrics across T independent trials for a fixed n.
 */
struct AveragedMetrics {
    double avg_insert_total_ms;
    double avg_insert_per_op_ns;
    double avg_search_batch_ms;
    double avg_search_per_query_ns;
    double avg_height;
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
 * @brief Executes timed Insert and timed batch SearchAVG on an already-constructed
 *        data structure instance. Data generation and destructor deallocation
 *        strictly occur OUTSIDE this function's timed blocks.
 * @brief Implements the PDF p. 3 timing protocol for Experiment 1:
 *        - Times ONLY the Insert operations (excluding input generation and
 *          memory deallocation).
 *        - If a single structure takes < 1.0 ms to build (small n), constructs
 *          `r` independent instances on the SAME input, times building all `r`
 *          together, divides by `r`, and frees all `r` instances AFTER stopping
 *          the timer.
 */
template <typename DataStructure>
TrialMetrics MeasureSingleTrial(
    DataStructure& ds,
    const std::vector<Record>& insert_data,
    const std::vector<int>& query_data) {
    // 1. Timed Insertion Block ONLY
    SteadyTimer insert_timer;
    insert_timer.Start();
    for (std::size_t i = 0; i < insert_data.size(); ++i) {
        ds.Insert(insert_data[i].id, insert_data[i].score);
    }
    insert_timer.Stop();
template <typename DS, typename DSFactory>
double MeasureInsertTimeMsWithReplication(
    const std::vector<Record>& inserts,
    std::uint64_t ds_seed,
    DSFactory make_ds,
    std::size_t& reps_used_out) {
    constexpr double kMinWindowMs = 1.0;
    constexpr std::size_t kMaxReplications = 65536;

    // Measure height outside timers
    int levels = ds.Height();
    std::size_t r = 1;
    while (true) {
        // Pre-allocate `r` independent empty structures BEFORE starting the timer
        std::vector<std::unique_ptr<DS>> instances;
        instances.reserve(r);
        for (std::size_t rep = 0; rep < r; ++rep) {
            std::uint64_t rep_seed = (rep == 0) ? ds_seed : MixSeed(ds_seed + rep);
            instances.push_back(make_ds(rep_seed));
        }

    // 2. Timed Batch SearchAVG Block ONLY
    double search_checksum = 0.0;
    SteadyTimer search_timer;
    search_timer.Start();
    for (std::size_t q = 0; q < query_data.size(); ++q) {
        search_checksum += ds.SearchAVG(query_data[q]);
    }
    search_timer.Stop();
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

    // Prevent -O2 dead-code elimination of the SearchAVG loop (outside timed region)
    ConsumeChecksum(search_checksum);
        // Free all `r` data structures AFTER stopping the timer
        instances.clear();

    TrialMetrics m{};
    m.insert_total_ms = insert_timer.ElapsedMilliseconds();
    m.insert_per_op_ns = insert_timer.ElapsedNanoseconds() /
                         static_cast<double>(insert_data.size());
    m.search_batch_ms = search_timer.ElapsedMilliseconds();
    m.search_per_query_ns = search_timer.ElapsedNanoseconds() /
                            static_cast<double>(query_data.size());
    m.height = levels;
    return m;
}
        double elapsed_ms = timer.ElapsedMilliseconds();
        if (elapsed_ms >= kMinWindowMs || r >= kMaxReplications) {
            reps_used_out = r;
            return elapsed_ms / static_cast<double>(r);
        }

/**
 * @brief Runs `trials` independent trials for a given dataset size `n` using a
 *        factory functor `make_ds(trial_seed)`. Each trial's data structure and
 *        input vectors are destroyed immediately at the end of that trial to
 *        keep peak memory minimal.
 */
template <typename DSFactory>
AveragedMetrics RunTrialsForConfig(
    std::size_t n,
    int power_k,
    int trials,
    std::uint64_t base_seed,
    DSFactory make_ds) {
    double sum_insert_ms = 0.0;
    double sum_insert_ns = 0.0;
    double sum_search_ms = 0.0;
    double sum_search_ns = 0.0;
    double sum_height = 0.0;

    for (int t = 0; t < trials; ++t) {
        // Deterministic per-(k, trial) seeds so all compared structures/parameters
        // face the exact same insertion and query sequences on trial `t`.
        std::uint64_t trial_key = MixSeed(
            base_seed ^ (static_cast<std::uint64_t>(power_k) << 32) ^
            static_cast<std::uint64_t>(t));
        std::uint64_t insert_seed = MixSeed(trial_key ^ 0xA5A5A5A55A5A5A5AULL);
        std::uint64_t query_seed = MixSeed(trial_key ^ 0x5A5A5A5AA5A5A5A5ULL);
        std::uint64_t ds_internal_seed = MixSeed(trial_key ^ 0x123456789ABCDEF0ULL);

        // Generate datasets OUTSIDE timed blocks
        std::vector<Record> inserts = GenerateInsertDataset(
            n, kMinID, kMaxID, kMinScore, kMaxScore, insert_seed);
        std::vector<int> queries = GenerateQueryDataset(
            kQueryBatchSize, kMinID, kMaxID, query_seed);

        TrialMetrics tm{};
        {
            // Scope ensures post-order destructor runs OUTSIDE MeasureSingleTrial timers
            // and frees all nodes BEFORE the next trial allocates anything.
            auto ds = make_ds(ds_internal_seed);
            tm = MeasureSingleTrial(*ds, inserts, queries);
        // Adaptively scale `r` to reach >= 1.0 ms window cleanly
        if (elapsed_ms <= 0.005) {
            r = std::min<std::size_t>(r * 16, kMaxReplications);
        } else {
            double factor = 1.35 / elapsed_ms;
            std::size_t est = static_cast<std::size_t>(std::ceil(static_cast<double>(r) * factor));
            r = std::min<std::size_t>(std::max<std::size_t>(r * 2, est), kMaxReplications);
        }

        sum_insert_ms += tm.insert_total_ms;
        sum_insert_ns += tm.insert_per_op_ns;
        sum_search_ms += tm.search_batch_ms;
        sum_search_ns += tm.search_per_query_ns;
        sum_height += static_cast<double>(tm.height);
    }

    const double inv_trials = 1.0 / static_cast<double>(trials);
    AveragedMetrics avg{};
    avg.avg_insert_total_ms = sum_insert_ms * inv_trials;
    avg.avg_insert_per_op_ns = sum_insert_ns * inv_trials;
    avg.avg_search_batch_ms = sum_search_ms * inv_trials;
    avg.avg_search_per_query_ns = sum_search_ns * inv_trials;
    avg.avg_height = sum_height * inv_trials;
    return avg;
}

// ============================================================================
// Experiment 1: Compare BST, AVL (delta=1), Treap, and SkipList (p=0.5) across n
// ============================================================================

inline void RunExperiment1(int trials, std::uint64_t base_seed) {
    const std::string csv_file = "exp1_comparison.csv";
    InitializeCSV(csv_file,
                  "structure,k,n,trials,queries_Q,avg_insert_total_ms,"
                  "avg_insert_per_op_ns,avg_search_batch_ms,avg_search_per_query_ns,avg_height");
    const std::string exp_csv = "exp1_insert_time.csv";
    const std::string unified_csv = "raw_data.csv";

    std::cout << "======================================================================\n"
              << "[Experiment 1] Comparing BST, AVL(delta=1), Treap, SkipList(p=0.5)\n"
              << "  n = 2^" << kMinExpPower << " .. 2^" << kMaxExpPower
              << " | Q = " << kQueryBatchSize
              << " | trials = " << trials
              << " | seed = " << base_seed << "\n"
              << "  Output CSV (incremental append): " << csv_file << "\n"
              << "======================================================================\n";
    InitializeCSV(exp_csv, "exp,log2_n,n,structure,avg_total_insert_ms,avg_replications_r,trials");
    PrepareUnifiedRawCSV(unified_csv, 1);

    auto format_and_save = [&](const std::string& name, int k, std::size_t n,
                               const AveragedMetrics& m) {
        std::ostringstream row;
        row << name << "," << k << "," << n << "," << trials << "," << kQueryBatchSize << ","
            << std::fixed << std::setprecision(4)
            << m.avg_insert_total_ms << ","
            << m.avg_insert_per_op_ns << ","
            << m.avg_search_batch_ms << ","
            << m.avg_search_per_query_ns << ","
            << std::setprecision(2) << m.avg_height;
        AppendCSVRow(csv_file, row.str());
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
                  << std::left << std::setw(15) << name << std::right
                  << " | Ins: " << std::fixed << std::setprecision(3) << std::setw(8)
                  << m.avg_insert_total_ms << " ms"
                  << " | Search(avg): " << std::setprecision(2) << std::setw(7)
                  << m.avg_search_per_query_ns << " ns"
                  << " | Height: " << std::setprecision(2) << std::setw(6)
                  << m.avg_height << "\n";
                  << std::left << std::setw(16) << name << std::right
                  << " | Avg Total Insert: " << std::setw(13) << std::setprecision(6)
                  << avg_ms << " ms"
                  << " (avg r=" << std::fixed << std::setprecision(1) << avg_r << ")\n";
        std::cout.unsetf(std::ios::floatfield);
    };

    for (int k = kMinExpPower; k <= kMaxExpPower; ++k) {
        const std::size_t n = (static_cast<std::size_t>(1) << k);

        // 1. Unbalanced BST -- immediately flushed to CSV upon finishing `trials`
        {
            AveragedMetrics m = RunTrialsForConfig(n, k, trials, base_seed,
                [](std::uint64_t) { return std::unique_ptr<BST>(new BST()); });
            format_and_save("BST", k, n, m);
        }
        double sum_bst = 0.0, sum_avl = 0.0, sum_trp = 0.0, sum_skp = 0.0;
        double rep_bst = 0.0, rep_avl = 0.0, rep_trp = 0.0, rep_skp = 0.0;

        // 2. AVL (delta = 1) -- immediately flushed to CSV
        {
            AveragedMetrics m = RunTrialsForConfig(n, k, trials, base_seed,
                [](std::uint64_t) { return std::unique_ptr<AVL>(new AVL(1)); });
            format_and_save("AVL(delta=1)", k, n, m);
        }
        for (int t = 0; t < trials; ++t) {
            TrialSeeds seeds = DeriveTrialSeeds(base_seed, k, t);
            std::vector<Record> inserts = GenerateInsertDataset(
                n, kMinID, kMaxID, kMinScore, kMaxScore, seeds.insert_seed);

        // 3. Treap -- immediately flushed to CSV
        {
            AveragedMetrics m = RunTrialsForConfig(n, k, trials, base_seed,
                [](std::uint64_t ds_seed) { return std::unique_ptr<Treap>(new Treap(ds_seed)); });
            format_and_save("Treap", k, n, m);
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

        // 4. SkipList (p = 0.5) -- immediately flushed to CSV
        {
            AveragedMetrics m = RunTrialsForConfig(n, k, trials, base_seed,
                [](std::uint64_t ds_seed) {
                    return std::unique_ptr<SkipList>(new SkipList(0.5, ds_seed));
                });
            format_and_save("SkipList(p=0.5)", k, n, m);
        }
        const double inv = 1.0 / static_cast<double>(trials);
        // Immediately flush all 4 averages for this `n` to CSV
        save_exp1_row(k, n, "BST",             sum_bst * inv, rep_bst * inv);
        save_exp1_row(k, n, "AVL(delta=1)",    sum_avl * inv, rep_avl * inv);
        save_exp1_row(k, n, "Treap",           sum_trp * inv, rep_trp * inv);
        save_exp1_row(k, n, "SkipList(p=1/2)", sum_skp * inv, rep_skp * inv);
    }

    std::cout << "[Experiment 1] Completed. Results safely saved to " << csv_file << ".\n";
    std::cout << "[Experiment 1] Completed successfully.\n";
}

// ============================================================================
// Experiment 2: AVL Sensitivity to Height-Difference Limit delta
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
    const std::string csv_file = "exp2_avl_delta.csv";
    const std::vector<int> deltas = {1, 2, 3, 5, 10};
    const std::string exp_csv = "exp2_search_time.csv";
    const std::string hit_csv = "exp2_hit_ratio.csv";
    const std::string unified_csv = "raw_data.csv";

    InitializeCSV(csv_file,
                  "delta,k,n,trials,queries_Q,avg_insert_total_ms,"
                  "avg_insert_per_op_ns,avg_search_batch_ms,avg_search_per_query_ns,avg_height");
    InitializeCSV(exp_csv,
                  "exp,log2_n,n,structure,avg_search_ns,avg_hit_ratio,checksum_sum,trials");
    InitializeCSV(hit_csv, "log2_n,n,queries_Q,avg_hit_count,avg_hit_ratio");
    PrepareUnifiedRawCSV(unified_csv, 2);

    std::cout << "======================================================================\n"
              << "[Experiment 2] Parameterized AVL Tree across delta in {1, 2, 3, 5, 10}\n"
              << "  n = 2^" << kMinExpPower << " .. 2^" << kMaxExpPower
              << " | Q = " << kQueryBatchSize
              << " | trials = " << trials
              << " | seed = " << base_seed << "\n"
              << "  Output CSV (incremental append): " << csv_file << "\n"
              << "======================================================================\n";
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

        for (std::size_t di = 0; di < deltas.size(); ++di) {
            const int delta = deltas[di];
            AveragedMetrics m = RunTrialsForConfig(n, k, trials, base_seed,
                [delta](std::uint64_t) { return std::unique_ptr<AVL>(new AVL(delta)); });
        double ns_bst = 0.0, ns_avl = 0.0, ns_trp = 0.0, ns_skp = 0.0;
        double sum_bst = 0.0, sum_avl = 0.0, sum_trp = 0.0, sum_skp = 0.0;
        double hit_ratio_acc = 0.0;

            // Immediately append & flush to CSV after finishing `trials` for (n, delta)
            std::ostringstream row;
            row << delta << "," << k << "," << n << "," << trials << "," << kQueryBatchSize << ","
                << std::fixed << std::setprecision(4)
                << m.avg_insert_total_ms << ","
                << m.avg_insert_per_op_ns << ","
                << m.avg_search_batch_ms << ","
                << m.avg_search_per_query_ns << ","
                << std::setprecision(2) << m.avg_height;
            AppendCSVRow(csv_file, row.str());
        for (int t = 0; t < trials; ++t) {
            TrialSeeds seeds = DeriveTrialSeeds(base_seed, k, t);
            std::vector<Record> inserts = GenerateInsertDataset(
                n, kMinID, kMaxID, kMinScore, kMaxScore, seeds.insert_seed);
            std::vector<int> queries = GenerateQueryDataset(
                kQueryBatchSize, kMinID, kMaxID, seeds.query_seed);

            std::cout << "  n=2^" << std::setw(2) << k << " (" << std::setw(7) << n << ") | "
                      << "delta=" << std::setw(2) << delta
                      << " | Ins: " << std::fixed << std::setprecision(3) << std::setw(8)
                      << m.avg_insert_total_ms << " ms"
                      << " | Search(avg): " << std::setprecision(2) << std::setw(7)
                      << m.avg_search_per_query_ns << " ns"
                      << " | Height: " << std::setprecision(2) << std::setw(6)
                      << m.avg_height << "\n";
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

    std::cout << "[Experiment 2] Completed. Results safely saved to " << csv_file << ".\n";
    std::cout << "  [Anti-O2 Total SearchAVG Return Sum] = "
              << std::fixed << std::setprecision(2) << grand_total_checksum << "\n"
              << "[Experiment 2] Completed successfully.\n";
}

// ============================================================================
// Experiment 3: Unbounded SkipList Sensitivity to Promotion Probability p
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
    const std::string csv_file = "exp3_skiplist_p.csv";
    const std::vector<double> probs = {0.10, 0.25, 0.50, 0.75, 0.90};
    const std::string exp_csv = "exp3_height.csv";
    const std::string unified_csv = "raw_data.csv";
    const std::string report_txt = "exp3_report_tables.txt";

    InitializeCSV(csv_file,
                  "prob_p,k,n,trials,queries_Q,avg_insert_total_ms,"
                  "avg_insert_per_op_ns,avg_search_batch_ms,avg_search_per_query_ns,avg_height");
    InitializeCSV(exp_csv, "exp,log2_n,n,structure,avg_height,trials");
    PrepareUnifiedRawCSV(unified_csv, 3);

    std::cout << "======================================================================\n"
              << "[Experiment 3] Unbounded SkipList across p in {0.10, 0.25, 0.50, 0.75, 0.90}\n"
              << "  n = 2^" << kMinExpPower << " .. 2^" << kMaxExpPower
              << " | Q = " << kQueryBatchSize
              << " | trials = " << trials
              << " | seed = " << base_seed << "\n"
              << "  Output CSV (incremental append): " << csv_file << "\n"
              << "======================================================================\n";
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

        for (std::size_t pi = 0; pi < probs.size(); ++pi) {
            const double p = probs[pi];
            AveragedMetrics m = RunTrialsForConfig(n, k, trials, base_seed,
                [p](std::uint64_t ds_seed) {
                    return std::unique_ptr<SkipList>(new SkipList(p, ds_seed));
                });
        for (int t = 0; t < trials; ++t) {
            TrialSeeds seeds = DeriveTrialSeeds(base_seed, k, t);
            std::vector<Record> inserts = GenerateInsertDataset(
                n, kMinID, kMaxID, kMinScore, kMaxScore, seeds.insert_seed);

            // Immediately append & flush to CSV after finishing `trials` for (n, p)
            std::ostringstream row;
            row << std::fixed << std::setprecision(2) << p << ","
                << k << "," << n << "," << trials << "," << kQueryBatchSize << ","
                << std::fixed << std::setprecision(4)
                << m.avg_insert_total_ms << ","
                << m.avg_insert_per_op_ns << ","
                << m.avg_search_batch_ms << ","
                << m.avg_search_per_query_ns << ","
                << std::setprecision(2) << m.avg_height;
            AppendCSVRow(csv_file, row.str());
            // For k == 20, also generate Q = 100,000 queries for Q4(a) SkipList step count
            std::vector<int> queries_2_20;
            if (k == 20) {
                queries_2_20 = GenerateQueryDataset(
                    kQueryBatchSize, kMinID, kMaxID, seeds.query_seed);
            }

            std::cout << "  n=2^" << std::setw(2) << k << " (" << std::setw(7) << n << ") | "
                      << "p=" << std::fixed << std::setprecision(2) << p
                      << " | Ins: " << std::setprecision(3) << std::setw(8)
                      << m.avg_insert_total_ms << " ms"
                      << " | Search(avg): " << std::setprecision(2) << std::setw(7)
                      << m.avg_search_per_query_ns << " ns"
                      << " | Height: " << std::setprecision(2) << std::setw(6)
                      << m.avg_height << "\n";
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

    std::cout << "[Experiment 3] Completed. Results safely saved to " << csv_file << ".\n";
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
// Correctness Test Suite (--test)
// Correctness Test Suite (--test): Exact 5 PDF Hand-Calculable Sets + Custom Set
// ============================================================================

inline bool ApproxEqual(double a, double b, double eps = 1e-9) noexcept {
    return std::fabs(a - b) <= eps;
}

inline void RunCorrectnessTests(std::uint64_t seed) {
    (void)seed;
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

    std::cout << "======================================================================\n"
              << "Running Correctness & Invariant Test Suite (--test)\n"
              << "======================================================================\n\n";
    std::cout << "========================================================================\n"
              << "Correctness Test Suite (--test): 5 Hand-Calculable PDF Sets + Custom Set\n"
              << "========================================================================\n\n";

    // ------------------------------------------------------------------------
    // Test 1: Duplicate ID Aggregation, SearchAVG, Height, and Print
    // PDF Test 1: BST -- Insert (3,100), (2,60), (1,70), (5,40), (4,70)
    // ------------------------------------------------------------------------
    std::cout << "--- [Test 1] Duplicate IDs ('One Node per ID'), SearchAVG & Print ---\n";
    std::cout << "=== [Test 1] BST: (3,100), (2,60), (1,70), (5,40), (4,70) ===\n";
    {
        BST bst;
        AVL avl(1);
        Treap treap(seed);
        SkipList skiplist(0.5, seed);

        check(bst.Height() == 0 && avl.Height() == 0 &&
                  treap.Height() == 0 && skiplist.Height() == 0,
              "Empty structures report Height() == 0");
        check(ApproxEqual(bst.SearchAVG(42), -1.0) &&
                  ApproxEqual(avl.SearchAVG(42), -1.0) &&
                  ApproxEqual(treap.SearchAVG(42), -1.0) &&
                  ApproxEqual(skiplist.SearchAVG(42), -1.0),
              "Empty structures return -1 on SearchAVG miss");

        // ID 20 inserted 3 times: (80 + 100 + 90) / 3 = 90.00
        // ID 10 inserted 2 times: (90 + 60) / 2  = 75.00
        const std::vector<Record> sample = {
            {20, 80}, {10, 90}, {30, 70}, {20, 100},
            {10, 60}, {25, 85}, {5, 95},  {20, 90}
        };

        for (std::size_t i = 0; i < sample.size(); ++i) {
            bst.Insert(sample[i].id, sample[i].score);
            avl.Insert(sample[i].id, sample[i].score);
            treap.Insert(sample[i].id, sample[i].score);
            skiplist.Insert(sample[i].id, sample[i].score);
        const Record input1[] = {{3, 100}, {2, 60}, {1, 70}, {5, 40}, {4, 70}};
        for (std::size_t i = 0; i < 5; ++i) {
            bst.InsertBST(input1[i].id, input1[i].score);
        }

        std::cout << "\n";
        bst.Print();
        std::cout << "\n";
        avl.Print();
        std::cout << "\n";

        check(ApproxEqual(bst.SearchAVG(20), 90.0) &&
                  ApproxEqual(avl.SearchAVG(20), 90.0) &&
                  ApproxEqual(treap.SearchAVG(20), 90.0) &&
                  ApproxEqual(skiplist.SearchAVG(20), 90.0),
              "Duplicate ID=20 average score == 90.00 across all 4 structures");

        check(ApproxEqual(bst.SearchAVG(10), 75.0) &&
                  ApproxEqual(avl.SearchAVG(10), 75.0) &&
                  ApproxEqual(treap.SearchAVG(10), 75.0) &&
                  ApproxEqual(skiplist.SearchAVG(10), 75.0),
              "Duplicate ID=10 average score == 75.00 across all 4 structures");

        check(ApproxEqual(bst.SearchAVG(999), -1.0) &&
                  ApproxEqual(avl.SearchAVG(999), -1.0) &&
                  ApproxEqual(treap.SearchAVG(999), -1.0) &&
                  ApproxEqual(skiplist.SearchAVG(999), -1.0),
              "Missing ID=999 returns -1 across all 4 structures");

        check(bst.Verify() && avl.Verify() && treap.Verify() && skiplist.Verify(),
              "All 4 structures satisfy ordering & balance/heap/tower invariants");
        bst.PrintBST();
        std::cout << "HeightBST = " << bst.HeightBST() << "\n";
        check(bst.HeightBST() == 3 && bst.VerifyBST(),
              "BST Test 1 matches hand calculation (Height == 3, valid BST)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Test 2: Parameterized AVL(delta) Rebalancing Verification
    // PDF Test 2: AVL (delta = 1) -- Insert (1,100), (2,60), (3,70), (4,40), (5,70)
    // ------------------------------------------------------------------------
    std::cout << "--- [Test 2] Parameterized AVL(delta) Ascending & Zig-Zag Stress ---\n";
    std::cout << "=== [Test 2] AVL (delta=1): (1,100), (2,60), (3,70), (4,40), (5,70) ===\n";
    {
        BST bst_skewed;
        AVL avl_d1(1);
        AVL avl_d3(3);

        for (int id = 1; id <= 10; ++id) {
            bst_skewed.Insert(id, id * 10);
            avl_d1.Insert(id, id * 10);
            avl_d3.Insert(id, id * 10);
        AVL avl(1);
        const Record input2[] = {{1, 100}, {2, 60}, {3, 70}, {4, 40}, {5, 70}};
        for (std::size_t i = 0; i < 5; ++i) {
            avl.InsertAVL(input2[i].id, input2[i].score);
        }

        std::cout << "\n";
        avl_d1.Print();
        std::cout << "\n";
        avl_d3.Print();
        std::cout << "\n";

        check(bst_skewed.Height() == 10,
              "Skewed BST height == 10 on ascending IDs 1..10");
        check(avl_d1.Height() == 4 && avl_d1.Verify(),
              "AVL(delta=1) maintains Height == 4 and |bf| <= 1");
        check(avl_d3.Height() >= avl_d1.Height() && avl_d3.Verify(),
              "AVL(delta=3) maintains valid |bf| <= 3 with relaxed height");

        // Random + duplicate invariant stress across delta in {1, 2, 3, 5}
        const std::vector<Record> stress =
            GenerateInsertDataset(5000, 1, 1500, 0, 100, seed);
        bool all_deltas_valid = true;
        const int test_deltas[] = {1, 2, 3, 5};
        for (int d : test_deltas) {
            AVL avl_test(d);
            for (std::size_t i = 0; i < stress.size(); ++i) {
                avl_test.Insert(stress[i].id, stress[i].score);
            }
            if (!avl_test.Verify()) all_deltas_valid = false;
        }
        check(all_deltas_valid,
              "AVL(delta in {1,2,3,5}) verified on 5,000 random+duplicate insertions");
        avl.PrintAVL();
        std::cout << "HeightAVL = " << avl.HeightAVL()
                  << " (Total single-rotations performed = " << avl.GetRotationCount() << ")\n";
        check(avl.HeightAVL() == 3 && avl.GetRotationCount() == 2 && avl.VerifyAVL(),
              "AVL Test 2 matches hand calculation (Height == 3, 2 RR rotations)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Test 3: Treap Manual Correctness Test with Fixed External Priorities
    // PDF Test 3: Treap (Min-Heap) --
    //   (3,100)[0.9], (2,60)[0.5], (1,70)[0.3], (5,60)[0.2], (4,80)[0.1]
    // ------------------------------------------------------------------------
    std::cout << "--- [Test 3] Treap Manual Test with Fixed Priorities ---\n";
    std::cout << "=== [Test 3] Treap (Min-Heap): "
              << "(3,100)[0.9], (2,60)[0.5], (1,70)[0.3], (5,60)[0.2], (4,80)[0.1] ===\n";
    {
        Treap treap;
        treap.Insert(50, 80, 0.60);
        treap.Insert(30, 70, 0.40);
        treap.Insert(70, 90, 0.50);
        // Priority 0.95 > 0.40 and > 0.60 -> rotates ID=20 all the way up to root!
        treap.Insert(20, 85, 0.95);
        // Priority 0.75 -> rotates ID=60 above 70 (0.50) and 50 (0.60) as right child of 20!
        treap.Insert(60, 75, 0.75);
        // Duplicate ID=20: updates avg score to (85+95)/2 = 90.00, keeps root priority 0.95!
        treap.Insert(20, 95, 0.10);
        treap.InsertTreap(3, 100, 0.9);
        treap.InsertTreap(2, 60,  0.5);
        treap.InsertTreap(1, 70,  0.3);
        treap.InsertTreap(5, 60,  0.2);
        treap.InsertTreap(4, 80,  0.1);

        std::cout << "\n";
        treap.Print();
        std::cout << "\n";

        check(treap.Verify(),
              "Treap satisfies BST (id) and Max-Heap (fixed priority) invariants");
        check(treap.Height() == 4,
              "Treap deterministic topology matches expected Height() == 4");
        check(ApproxEqual(treap.SearchAVG(20), 90.0) &&
                  ApproxEqual(treap.SearchAVG(60), 75.0) &&
                  ApproxEqual(treap.SearchAVG(100), -1.0),
              "Treap SearchAVG returns exact averages and -1 for missing ID");
        treap.PrintTreap();
        std::cout << "HeightTreap = " << treap.HeightTreap() << "\n";
        check(treap.HeightTreap() == 4 && treap.VerifyTreap(),
              "Treap Test 3 matches hand calculation (Min-Heap root id=4[0.1], Height == 4)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Test 4: SkipList Manual Correctness Test with Fixed Coin-Toss Results
    // PDF Test 4: SkipList (p = 1/2) --
    //   (1,100)[HHT], (2,60)[T], (3,70)[HT], (4,80)[HHHT],
    //   (5,60)[T], (6,80)[T], (7,90)[HHT]
    // ------------------------------------------------------------------------
    std::cout << "--- [Test 4] SkipList Manual Test with Fixed Coin Tosses & Unbounded Height ---\n";
    std::cout << "=== [Test 4] SkipList (p=1/2): "
              << "(1,100)[HHT], (2,60)[T], (3,70)[HT], (4,80)[HHHT], "
              << "(5,60)[T], (6,80)[T], (7,90)[HHT] ===\n";
    {
        SkipList sl(0.5);
        // {true, false} -> 1 head -> 2 levels (Levels 0, 1)
        sl.Insert(10, 80, std::vector<bool>{true, false});
        // {false} -> 0 heads -> 1 level (Level 0)
        sl.Insert(20, 90, std::vector<bool>{false});
        // {true, true, true, false} -> 3 heads -> 4 levels (Levels 0, 1, 2, 3)
        sl.Insert(30, 70, std::vector<bool>{true, true, true, false});
        // {true, true, false} -> 2 heads -> 3 levels (Levels 0, 1, 2)
        sl.Insert(40, 85, std::vector<bool>{true, true, false});
        // Duplicate ID=20 -> updates avg to (90+100)/2 = 95.00, preserves 1-level tower
        sl.Insert(20, 100, std::vector<bool>{true, true, true, true, false});
        sl.InsertSkipList(1, 100, "HHT");
        sl.InsertSkipList(2, 60,  "T");
        sl.InsertSkipList(3, 70,  "HT");
        sl.InsertSkipList(4, 80,  "HHHT");
        sl.InsertSkipList(5, 60,  "T");
        sl.InsertSkipList(6, 80,  "T");
        sl.InsertSkipList(7, 90,  "HHT");

        std::cout << "\n";
        sl.Print();
        std::cout << "\n";

        check(sl.Height() == 4,
              "SkipList deterministic coin tosses yield Height() == 4");
        check(sl.Verify(),
              "SkipList sorted horizontal lists and tower heights verified");
        check(ApproxEqual(sl.SearchAVG(20), 95.0) &&
                  ApproxEqual(sl.SearchAVG(30), 70.0) &&
                  ApproxEqual(sl.SearchAVG(99), -1.0),
              "SkipList SearchAVG returns exact averages and -1 for missing ID");

        // Prove NO hardcoded height limit (e.g., 45 consecutive heads -> 46 levels > 32)
        SkipList tall_sl(0.5);
        tall_sl.Insert(100, 88, std::vector<bool>(45, true));
        check(tall_sl.Height() == 46 && tall_sl.Verify() &&
                  ApproxEqual(tall_sl.SearchAVG(100), 88.0),
              "SkipList dynamically scales to 46 levels (no strict height cap)");
        sl.PrintSkipList();
        std::cout << "HeightSkipList = " << sl.HeightSkipList() << "\n";
        check(sl.HeightSkipList() == 4 && sl.VerifySkipList(),
              "SkipList Test 4 matches hand calculation (Height == 4, [HHT]=3 levels)");
    }
    std::cout << "\n";

    // ------------------------------------------------------------------------
    // Test 5: Cross-Structure Consistency & Post-Order Destructor Leak Audit
    // PDF Test 5: SearchAVG across all 4 structures --
    //   Insert: (1,80),(2,70),(1,60),(3,90),(1,91),(4,50),(5,40),(2,35)
    //   Query : id = 1, 2, 3, 6
    //   Hand calculation:
    //     id=1 -> (80 + 60 + 91) / 3 = 231 / 3 = 77.00
    //     id=2 -> (70 + 35) / 2      = 105 / 2 = 52.50
    //     id=3 -> 90 / 1             = 90.00
    //     id=6 -> -1 (never inserted)
    // ------------------------------------------------------------------------
    std::cout << "--- [Test 5] Cross-Structure Consistency & Post-Order Memory Audit ---\n";
    std::cout << "=== [Test 5] SearchAVG on All 4 Structures (Query id = 1, 2, 3, 6) ===\n";
    {
        const std::vector<Record> records =
            GenerateInsertDataset(4000, 1, 1000, 0, 100, seed + 7);
        const std::vector<int> queries =
            GenerateQueryDataset(2000, 1, 1200, seed + 13);

        BST bst;
        AVL avl(2);
        Treap treap(seed + 17);
        SkipList sl(0.5, seed + 23);
        AVL avl(1);
        Treap treap(42ULL);
        SkipList sl(0.5, 42ULL);

        for (std::size_t i = 0; i < records.size(); ++i) {
            bst.Insert(records[i].id, records[i].score);
            avl.Insert(records[i].id, records[i].score);
            treap.Insert(records[i].id, records[i].score);
            sl.Insert(records[i].id, records[i].score);
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

        bool all_match = true;
        for (std::size_t q = 0; q < queries.size(); ++q) {
            double r_bst = bst.SearchAVG(queries[q]);
            double r_avl = avl.SearchAVG(queries[q]);
            double r_trp = treap.SearchAVG(queries[q]);
            double r_skp = sl.SearchAVG(queries[q]);
            if (!ApproxEqual(r_bst, r_avl) ||
                !ApproxEqual(r_bst, r_trp) ||
                !ApproxEqual(r_bst, r_skp)) {
                all_match = false;
                break;
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
        check(all_match,
              "BST, AVL, Treap, and SkipList agree on all 2,000 random SearchAVG queries");
        check(all_ok,
              "All 4 structures match hand-calculated SearchAVG (77.0, 52.5, 90.0, -1.0)");
    }
    std::cout << "\n";

        // Also exercise degenerate deep BST (> 4096 levels) post-order destruction
        BST deep_bst;
        for (int id = 1; id <= 6000; ++id) {
            deep_bst.Insert(id, id % 101);
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
              << "Duplicate Priority Trap & >32-Level SkipList) ===\n";
    {
        BST bst;
        AVL avl(1);
        Treap treap;
        SkipList sl(0.5);

        // (30,60), (10,80), (20,100) triggers LR double rotation at 30 in AVL -> root=20
        // (50,40), (40,90) triggers RL double rotation at 30's right subtree in AVL
        // Duplicate updates on 20 (new root) and 10 (rotated child):
        //   id=20: (100 + 50) / 2 = 75.00
        //   id=10: (80 + 70) / 2  = 75.00
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
        check(deep_bst.Height() == 6000,
              "Deep degenerate BST (6,000 levels) constructed & measured safely");

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

    // At this point, EVERY data structure instance has exited scope and run its
    // post-order destructor. Verify zero leaked nodes!
    std::cout << "  Total nodes allocated   : " << MemoryTracker::TotalAllocations() << "\n"
    // Verify post-order destructors freed 100% of allocated nodes
    std::cout << "=== [Memory Deallocation Audit] ===\n"
              << "  Total nodes allocated   : " << MemoryTracker::TotalAllocations() << "\n"
              << "  Total nodes deallocated : " << MemoryTracker::TotalDeallocations() << "\n"
              << "  Live nodes remaining    : " << MemoryTracker::LiveNodes() << "\n";
    check(MemoryTracker::LiveNodes() == 0 &&
              MemoryTracker::TotalAllocations() == MemoryTracker::TotalDeallocations(),
          "Post-order destructors freed 100% of allocated nodes (0 memory leaks)");
          "Post-order destructors freed 100% of allocated nodes (0 leaks)");

    std::cout << "\n======================================================================\n";
    std::cout << "\n========================================================================\n";
    if (failed_checks == 0) {
        std::cout << "ALL CORRECTNESS & MEMORY TESTS PASSED SUCCESSFULLY!\n";
        std::cout << "ALL 6 CORRECTNESS TEST SUITES & MEMORY CHECKS PASSED!\n";
    } else {
        std::cerr << failed_checks << " TEST CHECK(S) FAILED!\n";
        std::cerr << failed_checks << " CHECK(S) FAILED!\n";
        std::exit(EXIT_FAILURE);
    }
    std::cout << "======================================================================\n";
    std::cout << "========================================================================\n";
}

}  // namespace algo_hw2

#endif  // EXPERIMENTS_H
