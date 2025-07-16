Refer to @IMPLEMENTATION_PLAN.md for the implementation plan.

Use @IMPLEMENTATION_STATUS.md to understand the current project status. Keep it up to date when you make changes.

Refer to @IR_OUTLINE.md when changing the intermediate representation.

Tests should be added to the `tests` directory. Tests in `tests/invalid` are for code that should fail to compile. Tests in `tests/runtime` should compile and be run.

Don't attempt anything more than 3 times. If you can't succeed after 3 attempts, let me know so I can help.

# Build Commands

- `make` - Build the compiler
- `make clean` - Clean build artifacts
- `make test` - Run all tests
- `make test_runtime` - Run the runtime tests
- `make test_invalid` - Run the invalid tests
- `make F=<file.c> run` - Compiles a C file to WASM and runs it
- `make F=<file.c> wat` - Prints the WAT for debugging
- `make F=<file.c> ast` - Prints the AST for debugging
- `make F=<file.c> ir` - Prints the IR for debugging
- `./wacc <file.c>` - Compile a C file to WASM (outputs to out.wasm)
- `./wacc <file.c> -o <output_path>` - Compile a C file to WASM (outputs to `<output_path>`)

# When writing code

- Use functions instead of objects wherever possible
- Prefer simple, straight-line code to complex abstractions
- Don't alter tests unless instructed to do so
- Don't commit changes unless instructed to do so

## When writing C

- Prefer snake_case for values and functions. Prefer PascalCase for types.
- Use arenas for memory management.
- Use data-oriented design for fast code with good cache locality.
- Follow the philosophy of Mike Acton, Casey Muratori, and Ryan Fleury.
- Avoid the standard library
