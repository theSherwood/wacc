CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = wacc
SOURCES = src/main.c src/arena.c src/lexer.c src/parser.c src/ir.c src/codegen.c src/utils.c src/error.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) out.wat

test: $(TARGET)
	./$(TARGET) test_simple.c
	cat out.wat

.PHONY: all clean test
