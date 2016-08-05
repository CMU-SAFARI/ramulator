SRCDIR := src
OBJDIR := obj
MAIN := $(SRCDIR)/Main.cpp
SRCS := $(filter-out $(MAIN) $(SRCDIR)/Gem5Wrapper.cpp, $(wildcard $(SRCDIR)/*.cpp))
OBJS := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))


# Ramulator currently supports g++ 5.1+ or clang++ 3.4+.  It will NOT work with
#   g++ 4.x due to an internal compiler error when processing lambda functions.
CXX := clang++
# CXX := g++-5
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
  CXXFLAGS := -O3 -std=c++11 -g -Wall -I/home/tianshi/tianshi-Workspace/DRAMPower/src -L/home/tianshi/tianshi-Workspace/DRAMPower/src
  LDFLAGS := -lboost_program_options -ldrampowerxml -ldrampower -lxerces-c
endif
ifeq ($(UNAME_S), Darwin)
  CXXFLAGS := -O3 -std=c++11 -g -Wall -I$(BOOST_PATH)/include
  LDFLAGS := -L$(BOOST_PATH)/lib -lboost_program_options
endif

.PHONY: all clean depend

all: depend ramulator

clean:
	rm -f ramulator
	rm -rf $(OBJDIR)

depend: $(OBJDIR)/.depend


$(OBJDIR)/.depend: $(SRCS)
	@mkdir -p $(OBJDIR)
	@rm -f $(OBJDIR)/.depend
	@$(foreach SRC, $(SRCS), $(CXX) $(CXXFLAGS) -DRAMULATOR -MM -MT $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC)) $(SRC) >> $(OBJDIR)/.depend ;)

ifneq ($(MAKECMDGOALS),clean)
-include $(OBJDIR)/.depend
endif


ramulator: $(MAIN) $(OBJS) $(SRCDIR)/*.h | depend
	$(CXX) $(CXXFLAGS) -DRAMULATOR -o $@ $(MAIN) $(INC) $(OBJS) $(LIB) $(LDFLAGS)

ramulator_debug: $(MAIN) $(OBJS) $(SRCDIR)/*.h | depend
	$(CXX) $(CXXFLAGS) -DRAMULATOR -o $@ $(MAIN) $(INC) $(OBJS) $(LIB) $(LDFLAGS)

$(OBJS): | $(OBJDIR)

$(OBJDIR): 
	@mkdir -p $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -DRAMULATOR -c -o $@ $(INC) $(LIB) $(LDFLAGS) $<
