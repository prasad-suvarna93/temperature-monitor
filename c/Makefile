# Temperature monitor -- host build (C).
#
#   make               build and run the tests
#   make demo          build and run the PC demonstration
#   make format        rewrite the sources in house style
#   make format-check  fail if a source is not in house style
#   make clean
#
# -Werror is not decoration. A warning in this tree is a defect that has not
# been triaged yet, and the cheapest moment to triage it is now.

# optimisation level lives in one variable so a debugger build is just
#   make OPT="-O0 -g3" ...
# without forking the flag list
OPT     ?= -O2

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wconversion -Wshadow \
           -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
           $(OPT) -g -DHOST_BUILD
INCS    := -Iapp -Ihal -Iport/host

# Whatever application modules exist so far, plus the host mock if it's present.
SRC     := $(wildcard app/*.c)
HOST    := $(wildcard port/host/host_hal.c)

# Headers are prerequisites too -- an edit to config.h must rebuild, or the
# next run silently demonstrates yesterday's constants.
HDRS    := $(wildcard app/*.h hal/*.h port/host/*.h test/*.h)

FMTSRC  := $(shell find app hal port test -name '*.c' -o -name '*.h' 2>/dev/null)

TESTBIN := build/tests
DEMOBIN := build/demo

.PHONY: all test demo format format-check clean

all: test

test: $(TESTBIN)
	@./$(TESTBIN)

demo: $(DEMOBIN)
	@./$(DEMOBIN)

$(TESTBIN): $(SRC) $(HOST) test/test_all.c $(HDRS) | build
	@$(CC) $(CFLAGS) $(INCS) -Itest $(filter %.c,$^) -o $@

$(DEMOBIN): $(SRC) $(HOST) port/host/demo.c $(HDRS) | build
	@$(CC) $(CFLAGS) $(INCS) $(filter %.c,$^) -o $@

format:
	@clang-format -i $(FMTSRC)

format-check:
	@clang-format --dry-run -Werror $(FMTSRC)

build:
	@mkdir -p build

clean:
	@rm -rf build
