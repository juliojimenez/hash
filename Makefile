CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = hash
SRC_DIR = src
BUILD_DIR = build
PREFIX = /usr/local

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

.PHONY: all clean install uninstall debug help

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

help:
	@echo "Available targets:"
	@echo " all     - Build hash (default)"
	@echo " clean   - Remove build artifacts"
	@echo " help    - Show this help message"
	@echo ""
	@echo "Directory structure:"
	@echo " src/    - Source files (.c)"
	@echo " build/  - Object files (.o)"
	@echo " ./      - Final binary (hash)"
