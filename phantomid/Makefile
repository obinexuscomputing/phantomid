CC = gcc
CFLAGS = -Wall -Wextra -Werror -I$(INCLUDE_DIR) -fPIC
LDFLAGS = -shared -lssl -lcrypto -pthread -lrt
INCLUDE_DIR = ./include
LIB_DIR = ./lib
BIN_DIR = ./bin
OBJ_DIR = ./obj

TARGET_LIB = libphantomid.so
TARGET_STATIC = libphantomid.a
TARGET_EXEC = phantomid

SRC_FILES = main.c network.c phantom_history.c phantomid.c
OBJ_FILES = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC_FILES))

all: $(TARGET_LIB) $(TARGET_STATIC) $(TARGET_EXEC)

# Shared library
$(TARGET_LIB): $(OBJ_FILES)
	@mkdir -p $(LIB_DIR)
	$(CC) $(LDFLAGS) -o $(LIB_DIR)/$(TARGET_LIB) $^

# Static library
$(TARGET_STATIC): $(OBJ_FILES)
	@mkdir -p $(LIB_DIR)
	ar rcs $(LIB_DIR)/$(TARGET_STATIC) $^

# Executable
$(TARGET_EXEC): $(OBJ_FILES)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$(TARGET_EXEC) $^ $(LDFLAGS)

# Object file compilation
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -rf $(LIB_DIR) $(BIN_DIR) $(OBJ_DIR)

