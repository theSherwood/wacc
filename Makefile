CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = wacc
SOURCES = src/main.c src/arena.c src/lexer.c src/parser.c src/semantic.c src/ir.c src/codegen.c src/utils.c src/error.c
F = 0

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) out.wasm out.wat

test:
	./scripts/test_all.sh

test_runtime:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)
	./scripts/test_runtime.sh

test_invalid:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)
	./scripts/test_invalid.sh

run:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)
	./wacc $(F)
	node test_wasm.js

wat:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)
	./wacc $(F)
	wasm2wat out.wasm

ast:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)
	./wacc --print-ast $(F)

ir:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)
	./wacc --print-ir $(F)

.PHONY: all clean test
