MAIN := src/Main.cpp
SRC := $(filter-out $(MAIN), $(wildcard src/*.cpp))
OBJ := $(SRC:.cpp=.o)

CXX := clang++
CXXFLAGS := -O3 -std=c++11 -stdlib=libc++ -g -Wall

all: ramulator-dramtrace ramulator-cputrace

ramulator-dramtrace: $(MAIN) $(OBJ)
	$(CXX) $(CXXFLAGS) -DRAMULATOR_DRAMTRACE -o $@ $^

ramulator-cputrace: $(MAIN) $(OBJ)
	$(CXX) $(CXXFLAGS) -DRAMULATOR_CPUTRACE -o $@ $^

clean:
	rm -f src/*.o ramulator-dramtrace ramulator-cputrace

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
