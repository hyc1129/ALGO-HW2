#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "DataStructures.h"
#include "Experiments.h"
#include "Utils.h"

namespace {

void PrintUsage(const char* prog_name) {
    std::cout
        << "Usage: " << prog_name << " [OPTIONS]\n\n"
        << "Options:\n"
        << "  --test                 Run the 5 PDF correctness tests + custom test.\n"
        << "  --exp, -exp <1|2|3|all> Run the specified experiment:\n"
        << "                           1   : Total Insert time (n = 2^0 .. 2^20, 4 DS)\n"
        << "                           2   : Avg SearchAVG time (n = 2^0 .. 2^20, Q = 100,000)\n"
        << "                           3   : Avg Height (n = 2^0 .. 2^20, 7 DS) + Q1-Q4 tables\n"
        << "                           all : Run Experiments 1, 2, and 3 sequentially\n"
        << "  --trials, -trials <N>  Number of trials per n (default: 10).\n"
        << "  --seed, -seed <S>      Starting 64-bit random seed (default: 0).\n"
        << "  -h, --help             Show this help message and exit.\n\n"
        << "Examples:\n"
        << "  " << prog_name << " --test\n"
        << "  " << prog_name << " --exp 1 --trials 10 --seed 0\n"
        << "  " << prog_name << " --exp 2 --trials 10 --seed 0\n"
        << "  " << prog_name << " --exp 3 --trials 10 --seed 0\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        PrintUsage(argv[0]);
        return EXIT_SUCCESS;
    }

    bool run_test = false;
    int exp_id = 0;  // 0 = none, 1..3 = single experiment, 4 = all
    int trials = 10;
    std::uint64_t seed = 0ULL;

    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                PrintUsage(argv[0]);
                return EXIT_SUCCESS;
            } else if (arg == "--test" || arg == "-test") {
                run_test = true;
            } else if (arg == "--exp" || arg == "-exp") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("Missing value after " + arg +
                                                " (expected 1, 2, 3, or all).");
                }
                std::string val = argv[++i];
                if (val == "all") {
                    exp_id = 4;
                } else {
                    exp_id = std::stoi(val);
                    if (exp_id < 1 || exp_id > 3) {
                        throw std::invalid_argument("Invalid experiment '" + val +
                                                    "'. Expected 1, 2, 3, or all.");
                    }
                }
            } else if (arg == "--trials" || arg == "-trials") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("Missing integer value after " + arg + ".");
                }
                trials = std::stoi(argv[++i]);
                if (trials <= 0) {
                    throw std::invalid_argument("Number of trials must be >= 1.");
                }
            } else if (arg == "--seed" || arg == "-seed") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("Missing unsigned seed value after " + arg + ".");
                }
                seed = std::stoull(argv[++i]);
            } else {
                throw std::invalid_argument("Unrecognized option: " + arg);
            }
        }

        if (!run_test && exp_id == 0) {
            throw std::invalid_argument(
                "No action specified. Pass --test and/or --exp <1|2|3|all>.");
        }

        if (run_test) {
            algo_hw2::RunCorrectnessTests(seed);
        }

        if (exp_id == 1 || exp_id == 4) {
            algo_hw2::RunExperiment1(trials, seed);
        }
        if (exp_id == 2 || exp_id == 4) {
            algo_hw2::RunExperiment2(trials, seed);
        }
        if (exp_id == 3 || exp_id == 4) {
            algo_hw2::RunExperiment3(trials, seed);
        }

    } catch (const std::exception& ex) {
        std::cerr << "[Error] " << ex.what() << "\n\n";
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
