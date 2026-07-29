# Temperature monitor -- host build (C++).
#
#   make               build and run the tests
#   make demo          build and run the PC demonstration
#   make format        rewrite the sources in house style
#   make format-check  fail if a source is not in house style
#   make clean
#
# -fno-exceptions -fno-rtti is not a micro-optimisation, it is a statement
# about what this program is. There is nowhere for an exception to go in a
# device whose job is to keep watching a temperature -- the handler would have
# to decide what the lamps show while the stack unwinds, and the honest answer
# is that it cannot. Errors are return values here, checked at the call site,
# with the failure paths as visible as the success ones. RTTI buys dynamic_cast
# and typeid, neither of which appears in this design.
#
# Together they also remove the unwind tables and the type-info records, which
# on a part with 64 KB of flash is the difference between fitting and not.

# optimisation level lives in one variable so a debugger build is just
#   make OPT="-O0 -g3" ...
# without forking the flag list
OPT      ?= -O2

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -Wconversion -Wshadow \
            -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual \
            -fno-exceptions -fno-rtti \
            $(OPT) -g -DHOST_BUILD
INCS     := -Iapp -Ihal -Iport/host

SRC := app/TemperatureSensor.cpp \
       app/DeviceInfo.cpp \
       app/Classifier.cpp \
       app/Indicator.cpp \
       app/TemperatureMonitor.cpp

HOST := port/host/HostHal.cpp

# Headers are prerequisites too -- an edit to Config.hpp must rebuild, or the
# next run silently demonstrates yesterday's constants.
HDRS := $(wildcard app/*.hpp hal/*.hpp port/host/*.hpp test/*.hpp)

# Everything the style applies to, including port/target, which the host build
# never compiles.
FMTSRC := $(shell find app hal port test -name '*.cpp' -o -name '*.hpp')

TESTBIN := build/tests
DEMOBIN := build/demo

.PHONY: all test demo format format-check clean

all: test

test: $(TESTBIN)
	@./$(TESTBIN)

demo: $(DEMOBIN)
	@./$(DEMOBIN)

$(TESTBIN): $(SRC) $(HOST) test/test_all.cpp $(HDRS) | build
	@$(CXX) $(CXXFLAGS) $(INCS) -Itest $(filter %.cpp,$^) -o $@

$(DEMOBIN): $(SRC) $(HOST) port/host/demo.cpp $(HDRS) | build
	@$(CXX) $(CXXFLAGS) $(INCS) $(filter %.cpp,$^) -o $@

format:
	@clang-format -i $(FMTSRC)

format-check:
	@clang-format --dry-run -Werror $(FMTSRC)

build:
	@mkdir -p build

clean:
	@rm -rf build
