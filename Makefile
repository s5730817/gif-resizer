CC = gcc
CFLAGS = -Iinclude -O2 -Wall -Wextra
LDFLAGS = -lm
SRCS = main.c src/gifdec.c src/gifenc.c
TARGET = main

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

clean:
	rm -f $(TARGET) processed/*.gif

.PHONY: all run valgrind clean
