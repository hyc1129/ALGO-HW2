CXX          := g++
CXXFLAGS     := -std=c++11 -O2 -Wall -Wextra -Wpedantic
TARGET       := treebench
BENCH_TARGET := treebench_bench

HEADERS      := Utils.h DataStructures.h Experiments.h
SOURCES      := main.cpp

.PHONY: all bench test exp1 exp2 exp3 exp_all plot clean

all: $(TARGET)

# Default target (`treebench`): keeps MemoryTracker active for `--test` leak verification
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

# Benchmark target (`treebench_bench`): defines -DNO_MEMORY_TRACKER so node construction
# and destruction incur zero counter overhead during Experiment 1/2/3 measurements
bench: $(BENCH_TARGET)

$(BENCH_TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -DNO_MEMORY_TRACKER $(SOURCES) -o $(BENCH_TARGET)

test: $(TARGET)
	./$(TARGET) --test

exp1: $(BENCH_TARGET)
	./$(BENCH_TARGET) --exp 1 --trials 10 --seed 0

exp2: $(BENCH_TARGET)
	./$(BENCH_TARGET) --exp 2 --trials 10 --seed 0

exp3: $(BENCH_TARGET)
	./$(BENCH_TARGET) --exp 3 --trials 10 --seed 0

exp_all: $(BENCH_TARGET)
	./$(BENCH_TARGET) --exp all --trials 10 --seed 0

plot:
	python3 plot.py

clean:
	rm -f $(TARGET) $(TARGET).exe $(BENCH_TARGET) $(BENCH_TARGET).exe *.o *.obj
