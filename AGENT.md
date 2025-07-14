Refer to @IMPLEMENTATION_PLAN.md for the implementation plan.

Use @IMPLEMENTATION_STATUS.md to understand the current project status. Keep it up to date when you make changes.

@IR_OUTLINE.md has the plan for the intermediate representation.

# Build Commands

- `make` - Build the compiler
- `make test` - Build and test with test_simple.c
- `make clean` - Clean build artifacts
- `./wacc <file.c>` - Compile a C file to WASM (outputs to out.wat)

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
