CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_DEFAULT_SOURCE -D_BSD_SOURCE
DEBUG_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -O0 -Iinclude -D_DEFAULT_SOURCE -D_BSD_SOURCE
ASAN_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -D_DEFAULT_SOURCE -D_BSD_SOURCE

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

SRCS = $(SRC_DIR)/allocator.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
TARGET_LIB = $(LIB_DIR)/libheapforge.a

TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/%, $(TEST_SRCS))

.PHONY: all test debug asan clean help

all: $(TARGET_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_LIB): $(OBJS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(TARGET_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -L$(LIB_DIR) -lheapforge -o $@

test: $(TARGET_LIB) $(BIN_DIR)/test_malloc
	@echo "Running Milestone 1 tests..."
	@$(BIN_DIR)/test_malloc

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean all

asan: CFLAGS = $(ASAN_CFLAGS)
asan: clean $(TARGET_LIB) $(BIN_DIR)/test_malloc
	@echo "Running tests under AddressSanitizer and UndefinedBehaviorSanitizer..."
	@$(BIN_DIR)/test_malloc

clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR)

help:
	@echo "HeapForge Build System"
	@echo "Targets:"
	@echo "  all      - Build static library lib/libheapforge.a"
	@echo "  test     - Build and run Milestone test suite"
	@echo "  debug    - Build with debug symbols and no optimizations (-O0 -g)"
	@echo "  asan     - Build and run tests with AddressSanitizer & UBSan"
	@echo "  clean    - Remove build artifacts and binaries"
	@echo "  help     - Show this help message"
