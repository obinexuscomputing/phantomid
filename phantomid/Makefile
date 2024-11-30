CC = gcc
CFLAGS = -Wall -Wextra -Werror -I$(INCLUDE_DIR) -fPIC
LDFLAGS = -shared -lssl -lcrypto -pthread -lrt
INCLUDE_DIR = ./include
LIB_DIR = ./lib
BIN_DIR = ./bin

TARGET_LIB = libphantomid.so
TARGET_STATIC = libphantomid.a
TARGET_EXEC = phantomid

all: $(TARGET_LIB) $(TARGET_STATIC) $(TARGET_EXEC)

$(TARGET_LIB): $(patsubst %.c,%.o,$(wildcard src/*.c))
	@mkdir -p $(LIB_DIR)
	$(CC) $(LDFLAGS) -o $(LIB_DIR)/$(TARGET_LIB) $^

$(TARGET_STATIC): $(patsubst %.c,%.o,$(wildcard src/*.c))
	@mkdir -p $(LIB_DIR)
	ar rcs $(LIB_DIR)/$(TARGET_STATIC) $^

$(TARGET_EXEC): $(patsubst %.c,%.o,$(wildcard src/*.c))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$(TARGET_EXEC) $^ $(LDFLAGS)

clean:
	rm -f $(LIB_DIR)/*.so $(LIB_DIR)/*.a $(BIN_DIR)/$(TARGET_EXEC) $(patsubst %.c,%.o,$(wildcard src/*.c))