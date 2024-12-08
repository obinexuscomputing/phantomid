CC = gcc
CFLAGS = -Wall -Wextra -I../../runtime/tree -pthread
LDFLAGS = -pthread

SRCS = test_tree.c ../../runtime/tree/tree.c
OBJS = $(SRCS:.c=.o)
TARGET = test_tree

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)