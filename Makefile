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
	./scripts/test_runtime.sh

test_invalid:
	./scripts/test_invalid.sh

run:
	./wacc $(F)
	node test_wasm.js

wat:
	./wacc $(F)
	wasm2wat out.wasm

ast:
	./wacc --print-ast $(F)

ir:
	./wacc --print-ir $(F)

.PHONY: all clean test
