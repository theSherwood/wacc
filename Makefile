CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = wacc
SOURCES = main.c arena.c lexer.c parser.c ir.c codegen.c utils.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) out.wat

test: $(TARGET)
	./$(TARGET) test_simple.c
	cat out.wat

.PHONY: all clean test
