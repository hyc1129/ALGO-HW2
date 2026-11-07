#ifndef UTILS_H
#define UTILS_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace algo_hw2 {

/**
 * @brief Represents a single (id, score) input record.
 */
struct Record {
    int id;
    int score;
};

/**
 * @brief High-resolution monotonic stopwatch backed by std::chrono::steady_clock.
 */
class SteadyTimer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    SteadyTimer() : start_time_(), end_time_(), running_(false) {}

    inline void Start() noexcept {
        running_ = true;
        start_time_ = Clock::now();
    }

    inline void Stop() noexcept {
        end_time_ = Clock::now();
        running_ = false;
    }

    inline double ElapsedNanoseconds() const noexcept {
        const TimePoint finish = running_ ? Clock::now() : end_time_;
        return std::chrono::duration<double, std::nano>(finish - start_time_).count();
    }

    inline double ElapsedMicroseconds() const noexcept {
        const TimePoint finish = running_ ? Clock::now() : end_time_;
        return std::chrono::duration<double, std::micro>(finish - start_time_).count();
    }

    inline double ElapsedMilliseconds() const noexcept {
        const TimePoint finish = running_ ? Clock::now() : end_time_;
        return std::chrono::duration<double, std::milli>(finish - start_time_).count();
    }

private:
    TimePoint start_time_;
    TimePoint end_time_;
    bool running_;
};

/**
 * @brief Shared (id, score) input generator used across all three experiments.
 *        id ~ UNI(min_id, max_id), score ~ UNI(min_score, max_score), integers.
 */
inline std::vector<Record> GenerateInsertDataset(
    std::size_t count,
    int min_id,
    int max_id,
    int min_score,
    int max_score,
    std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> id_dist(min_id, max_id);
    std::uniform_int_distribution<int> score_dist(min_score, max_score);

    std::vector<Record> dataset;
    dataset.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        dataset.push_back(Record{id_dist(rng), score_dist(rng)});
    }
    return dataset;
}

/**
 * @brief Generates sequential IDs 1, 2, ..., count (for Experiment 3 Question 2).
 */
inline std::vector<Record> GenerateSequentialDataset(
    std::size_t count,
    int min_score,
    int max_score,
    std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> score_dist(min_score, max_score);

    std::vector<Record> dataset;
    dataset.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        dataset.push_back(Record{static_cast<int>(i + 1), score_dist(rng)});
    }
    return dataset;
}

/**
 * @brief Shared query-ID generator used in Experiment 2 (and Exp 3 Q4a).
 *        id ~ UNI(min_id, max_id), integers.
 */
inline std::vector<int> GenerateQueryDataset(
    std::size_t query_count,
    int min_id,
    int max_id,
    std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> id_dist(min_id, max_id);

    std::vector<int> queries;
    queries.reserve(query_count);
    for (std::size_t i = 0; i < query_count; ++i) {
        queries.push_back(id_dist(rng));
    }
    return queries;
}

/**
 * @brief Initializes a CSV file with its header row.
 */
inline void InitializeCSV(const std::string& filename, const std::string& header_line) {
    std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to create CSV file: " + filename);
    }
    ofs << header_line << '\n';
    ofs.flush();
}

/**
 * @brief Prepares the unified raw_data.csv file for `exp_id` (1, 2, or 3):
 *        preserves rows belonging to other experiments while clearing any old
 *        rows from `exp_id`, so subsequent AppendCSVRow calls safely append per n.
 */
inline void PrepareUnifiedRawCSV(const std::string& filename, int exp_id) {
    const std::string header = "exp,log2_n,n,structure,unit,mean_value,trials";
    std::vector<std::string> preserved_lines;
    const std::string prefix = std::to_string(exp_id) + ",";

    {
        std::ifstream ifs(filename);
        if (ifs.is_open()) {
            std::string line;
            bool is_first = true;
            while (std::getline(ifs, line)) {
                if (is_first) {
                    is_first = false;
                    continue;
                }
                if (!line.empty() && line.compare(0, prefix.size(), prefix) != 0) {
                    preserved_lines.push_back(line);
                }
            }
        }
    }

    std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open unified CSV file: " + filename);
    }
    ofs << header << '\n';
    for (std::size_t i = 0; i < preserved_lines.size(); ++i) {
        ofs << preserved_lines[i] << '\n';
    }
    ofs.flush();
}

/**
 * @brief Appends a single row to a CSV file and immediately flushes & closes it
 *        to prevent data loss if a later, larger dataset triggers OOM.
 */
inline void AppendCSVRow(const std::string& filename, const std::string& row_line) {
    std::ofstream ofs(filename, std::ios::out | std::ios::app);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to append to CSV file: " + filename);
    }
    ofs << row_line << '\n';
    ofs.flush();
}

/**
 * @brief SplitMix64 bijection for deriving independent deterministic sub-seeds
 *        from (starting_seed + trial_i) without seed correlation.
 */
inline std::uint64_t MixSeed(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

}  // namespace algo_hw2

#endif  // UTILS_H
