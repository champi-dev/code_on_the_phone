# Terminal C Port - Makefile
# High-performance build configuration

CC = clang
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE
LDFLAGS = -lpthread -lm

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -DLINUX
    LDFLAGS += -lrt
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -DDARWIN
endif

# Build modes
ifeq ($(MODE),debug)
    CFLAGS += -O0 -g3 -DDEBUG -fsanitize=address,undefined
    LDFLAGS += -fsanitize=address,undefined
else ifeq ($(MODE),profile)
    CFLAGS += -O3 -g -pg -fprofile-generate
    LDFLAGS += -pg -fprofile-generate
else
    # Release mode with aggressive optimizations
    CFLAGS += -O3 -march=native -flto -fomit-frame-pointer
    CFLAGS += -funroll-loops -fvectorize
    LDFLAGS += -flto -s
endif

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin
TESTDIR = tests
BENCHDIR = bench

# Include paths
INCLUDES = -I$(INCDIR)

# Source files
SERVER_SRCS = $(wildcard $(SRCDIR)/server/*.c)
AUTH_SRCS = $(wildcard $(SRCDIR)/auth/*.c)
PROXY_SRCS = $(wildcard $(SRCDIR)/proxy/*.c)
UTILS_SRCS = $(wildcard $(SRCDIR)/utils/*.c)
STATIC_SRCS = $(wildcard $(SRCDIR)/static/*.c)

ALL_SRCS = $(SERVER_SRCS) $(AUTH_SRCS) $(PROXY_SRCS) $(UTILS_SRCS) $(STATIC_SRCS)
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(ALL_SRCS))

# Main target
TARGET = $(BINDIR)/terminal

# Test files
TEST_SRCS = $(wildcard $(TESTDIR)/*.c)
TEST_BINS = $(patsubst $(TESTDIR)/%.c,$(BINDIR)/test_%,$(TEST_SRCS))

# Benchmark files
BENCH_SRCS = $(wildcard $(BENCHDIR)/*.c)
BENCH_BINS = $(patsubst $(BENCHDIR)/%.c,$(BINDIR)/bench_%,$(BENCH_SRCS))

.PHONY: all clean test bench install

all: directories $(TARGET)

directories:
	@mkdir -p $(OBJDIR)/{server,auth,proxy,utils,static}
	@mkdir -p $(BINDIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Tests
test: directories $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done

$(BINDIR)/test_%: $(TESTDIR)/%.c $(filter-out $(OBJDIR)/server/main.o,$(OBJS))
	$(CC) $(CFLAGS) $(INCLUDES) $< $(filter-out $(OBJDIR)/server/main.o,$(OBJS)) -o $@ $(LDFLAGS)

# Benchmarks
bench: directories $(BENCH_BINS)
	@for bench in $(BENCH_BINS); do \
		echo "Running $$bench..."; \
		$$bench; \
	done

$(BINDIR)/bench_%: $(BENCHDIR)/%.c $(filter-out $(OBJDIR)/server/main.o,$(OBJS))
	$(CC) $(CFLAGS) $(INCLUDES) $< $(filter-out $(OBJDIR)/server/main.o,$(OBJS)) -o $@ $(LDFLAGS)

# Installation
PREFIX ?= /usr/local
install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/
	install -d $(PREFIX)/share/terminal
	cp -r ../render-app/public $(PREFIX)/share/terminal/

# Development helpers
format:
	clang-format -i $(ALL_SRCS) $(INCDIR)/*.h $(TEST_SRCS) $(BENCH_SRCS)

lint:
	clang-tidy $(ALL_SRCS) -- $(CFLAGS) $(INCLUDES)

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Profile-guided optimization
pgo-generate: MODE=profile
pgo-generate: clean all
	@echo "Run the application with typical workload to generate profile data"

pgo-use:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fprofile-use" LDFLAGS="$(LDFLAGS) -fprofile-use"

# Clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f *.gcda *.gcno

# Dependencies
-include $(OBJS:.o=.d)

$(OBJDIR)/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@