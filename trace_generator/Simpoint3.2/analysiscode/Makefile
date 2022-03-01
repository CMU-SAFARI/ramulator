CPPFLAGS = -std=c++98 -Wall -pedantic -pedantic-errors -O3

CXX = g++

all: simpoint

# This bit of trickery is to create ".d" files which describe the dependencies
# of each .cpp file, which are then included in this makefile (see the
# "-include" directive below). See the makefile info page for more info on
# this technique.
%.d:%.cpp
	set -e; $(CXX) -MM $(CPPFLAGS) $< \
	| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
	[ -s $@ ] || rm -f $@

SOURCES = CmdLineParser.cpp Datapoint.cpp Dataset.cpp FVParser.cpp KMeans.cpp \
          Logger.cpp Simpoint.cpp SimpointOptions.cpp Utilities.cpp
OBJECTS = $(SOURCES:.cpp=.o)
DEPENDENCIES = $(SOURCES:.cpp=.d)

# SimpointOptions takes forever to compile with optimizations on, so we simply
# do it without optimizations (shouldn't affect the run-time of the program)
SimpointOptions.o:
	$(CXX)  -Wall -pedantic -pedantic-errors -o SimpointOptions.o -c SimpointOptions.cpp

# If the target is not "clean", then include the dependencies (which also makes
# them as necessary)
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDENCIES)
endif

simpoint: $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(OBJECTS) -o simpoint
	cp simpoint ../bin/.

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(DEPENDENCIES) core simpoint
