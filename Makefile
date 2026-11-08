CXX      := g++
CXXFLAGS := -std=c++11 -O2 -Wall -Wextra -Wpedantic
TARGET   := treebench

HEADERS  := Utils.h DataStructures.h Experiments.h
SOURCES  := main.cpp

.PHONY: all bench test exp1 exp2 exp3 exp_all plot clean

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

# Default build (`treebench`): keeps MemoryTracker active for `--test` leak verification
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

# Benchmark build (`treebench`): recompiles the single `treebench` executable with
# -DNO_MEMORY_TRACKER so node construction/destruction has zero counter overhead
bench: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -DNO_MEMORY_TRACKER $(SOURCES) -o $(TARGET)

test: all
	./$(TARGET) --test

exp1: bench
	./$(TARGET) --exp 1 --trials 10 --seed 0

exp2: bench
	./$(TARGET) --exp 2 --trials 10 --seed 0

exp3: bench
	./$(TARGET) --exp 3 --trials 10 --seed 0

exp_all: bench
	./$(TARGET) --exp all --trials 10 --seed 0

plot:
	python3 plot.py

clean:
	rm -f $(TARGET) $(TARGET).exe *.o *.obj
