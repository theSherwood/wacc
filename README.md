# wacc

w.a.c.c - WebAssembly C Compiler

An in-progress compiler for a useful subset of C99, implemented in that same subset of C99 that targets WASM. The goal is to self-host it.

## Implementation Status

I only got as far as some control flow (`if`, `while`, `for`, `break`, `continue`) before I discovered [xcc](https://github.com/tyfkda/xcc), which achieves most of what I'm looking for in a self-hosting C-to-WASM compiler. Consequently, I'm archiving this project.
