# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -O2 -fno-stack-protector

# System detection and platform-specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -DLINUX
    LIBS_OS :=
else
    CFLAGS += -D_WIN32 -DWIN32_LEAN_AND_MEAN
    LIBS_OS := -lws2_32
endif

# Library and include paths
LIB_DIR := lib
LDFLAGS := -pthread -L./$(LIB_DIR)

# OpenSSL configuration
PKG_CONFIG := $(shell which pkg-config 2>/dev/null)
ifneq ($(PKG_CONFIG),)
    CFLAGS += $(shell pkg-config --cflags openssl)
    LIBS := $(shell pkg-config --libs openssl)
else
    CFLAGS += -I/usr/include/openssl
    LIBS := -lssl -lcrypto
endif

# Complete libraries list
LIBS += $(LIBS_OS) -lrt

# Directory structure
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

# Source files and objects
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Binary name
TARGET := $(BIN_DIR)/phantomid

# Header files
DEPS := $(wildcard $(SRC_DIR)/*.h)

# Create build directories
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# Default target
.PHONY: all
all: check_env $(TARGET)

# Environment check
.PHONY: check_env
check_env:
	@echo "Checking build environment..."
	@echo "Building for $(UNAME_S)"
	@if [ ! -f /usr/include/openssl/ssl.h ]; then \
		echo "Error: OpenSSL development files not found"; \
		echo "Please install libssl-dev (Debian/Ubuntu) or openssl-devel (RHEL/CentOS)"; \
		exit 1; \
	fi
	@echo "OpenSSL development files found"

# Link the final binary
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build complete."

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files and start fresh
.PHONY: clean
clean:
	@echo "Cleaning build files..."
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*
	@echo "Clean complete."

# Debug build
.PHONY: debug
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Run the program
.PHONY: run
run: $(TARGET)
	./$(TARGET)