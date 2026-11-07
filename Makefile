CXX      := g++
CXXFLAGS := -std=c++11 -O2 -Wall -Wextra -Wpedantic
TARGET   := treebench

HEADERS  := Utils.h DataStructures.h Experiments.h
SOURCES  := main.cpp

.PHONY: all test exp1 exp2 exp3 exp_all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

test: $(TARGET)
	./$(TARGET) --test

exp1: $(TARGET)
	./$(TARGET) --exp 1 --trials 10 --seed 0

exp2: $(TARGET)
	./$(TARGET) --exp 2 --trials 10 --seed 0

exp3: $(TARGET)
	./$(TARGET) --exp 3 --trials 10 --seed 0

exp_all: $(TARGET)
	./$(TARGET) --exp all --trials 10 --seed 0

clean:
	rm -f $(TARGET) $(TARGET).exe *.o *.obj
