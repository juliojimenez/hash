CC = gcc
CFLAGS ?= -Wall -Wextra -O2 -std=gnu99
TARGET = hash-shell
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests
TEST_BUILD_DIR = $(BUILD_DIR)/tests
PREFIX = /usr/local

UNITY_DIR = $(TEST_DIR)/unity
UNITY_SRC = $(UNITY_DIR)/src/unity.c
UNITY_OBJ = $(TEST_BUILD_DIR)/unity.o

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

TEST_SRC = $(wildcard $(TEST_DIR)/test_*.c)
TEST_OBJ = $(filter-out $(BUILD_DIR)/main.o, $(OBJ))
TEST_BINS = $(patsubst $(TEST_DIR)/test_%.c,$(TEST_BUILD_DIR)/test_%,$(TEST_SRC))

.PHONY: all clean install uninstall debug help test test-setup test-clean

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: clean all


help:
	@echo "Available targets:"
	@echo " all         - Build hash (default)"
	@echo " clean       - Remove build artifacts"
	@echo " install     - Install hash to $(PREFIX)/bin"
	@echo " uninstall   - Remove hash from $(PREFIX)/bin"
	@echo " debug       - Build with debug symbols"
	@echo " test-setup  - Download and setup Unity test framework"
	@echo " test        - Run unit tests"
	@echo " test-clean  - Remove test build artifacts"
	@echo " help        - Show this help message"
	@echo ""
	@echo "Directory structure:"
	@echo " src/    - Source files (.c)"
	@echo " build/  - Object files (.o)"
	@echo " tests/  - Unit tests"
	@echo " ./      - Final binary (hash)"

test-setup:
	@if [ ! -d "$(UNITY_DIR)" ]; then \
		echo "Setting up Unity test framework..."; \
		$(TEST_DIR)/setup_unity.sh; \
	else \
		echo "Unity already set up. Run 'make test-clean' to remove and reinstall."; \
	fi

# Build Unity object file
$(UNITY_OBJ): $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -I$(UNITY_DIR)/src -c $< -o $@

# Create test build directory
$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

# Build individual test binaries
$(TEST_BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(TEST_OBJ) $(UNITY_OBJ) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(UNITY_DIR)/src $< $(TEST_OBJ) $(UNITY_OBJ) -o $@

# Run all unit tests
test: test-setup $(TEST_BINS)
	@echo ""
	@echo "Running unit tests..."
	@echo "===================="
	@passed=0; \
	failed=0; \
	for test in $(TEST_BINS); do \
		echo ""; \
		echo "Running $$test..."; \
		if $$test; then \
			passed=$$((passed + 1)); \
		else \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "===================="; \
	echo "Tests passed: $$passed"; \
	echo "Tests failed: $$failed"; \
	if [ $$failed -gt 0 ]; then \
		exit 1; \
	fi

# Clean test artifacts
test-clean:
	rm -rf $(TEST_BUILD_DIR) $(UNITY_DIR)
	rm -f $(TEST_DIR)/unity.h
