# Intermediate Representation (IR) Design for C99 to WASM Compiler

## Overview

This IR is designed to be a simple, efficient intermediate representation that facilitates translation from C99 source code to WebAssembly (WASM) bytecode. The IR prioritizes simplicity, performance, and direct mapping to WASM constructs.

## Core Design Principles

1. **Stack-based evaluation** - mirrors WASM's stack machine model
2. **Static Single Assignment (SSA)** - simplified control flow analysis
3. **Explicit memory operations** - clear distinction between stack and heap
4. **Type-aware** - maintains C99 type information for optimization
5. **WASM-oriented** - instructions map directly to WASM operations

## IR Structure

### Structured Control Flow

WASM requires structured control flow, so we use a hierarchical approach instead of basic blocks:

```c
typedef enum {
    REGION_BLOCK,      // Linear sequence of instructions
    REGION_LOOP,       // Loop construct
    REGION_IF,         // If-then-else construct
    REGION_FUNCTION    // Function body
} RegionType;

typedef struct Region {
    RegionType type;
    int id;
    Instruction* instructions;
    size_t instruction_count;
    struct Region** children;    // Nested regions
    size_t child_count;
    struct Region* parent;       // Parent region

    // For control flow
    union {
        struct {
            struct Region* then_region;
            struct Region* else_region;
        } if_data;
        struct {
            struct Region* body;
        } loop_data;
    } data;
} Region;

typedef struct Function {
    const char* name;
    Type return_type;
    Parameter* parameters;
    size_t param_count;
    Region* body;                // Function body as root region
    LocalVariable* locals;
    size_t local_count;
    int max_stack_size;          // For stack simulation
} Function;
```

### Instruction Set

The IR uses a three-address code format with explicit operand and result typing:

```c
typedef enum {
    // Arithmetic
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_NEG, IR_NOT, IR_BITWISE_AND, IR_BITWISE_OR, IR_BITWISE_XOR, IR_BITWISE_NOT,

    // Comparison
    IR_EQ, IR_NE, IR_LT, IR_LE, IR_GT, IR_GE,

    // Logical
    IR_LOGICAL_AND, IR_LOGICAL_OR, IR_LOGICAL_NOT,

    // Memory
    IR_LOAD, IR_STORE, IR_LOAD_GLOBAL, IR_STORE_GLOBAL,
    IR_ALLOCA, IR_LOAD_LOCAL, IR_STORE_LOCAL,
    IR_STACK_SAVE, IR_STACK_RESTORE, IR_MEMCPY, IR_MEMSET,

    // Control Flow
    IR_BLOCK, IR_LOOP, IR_IF, IR_ELSE, IR_END,
    IR_BREAK, IR_CONTINUE, IR_RETURN, IR_CALL, IR_CALL_INDIRECT,

    // Constants
    IR_CONST_INT, IR_CONST_FLOAT, IR_CONST_STRING,

    // Type conversions
    IR_CAST, IR_TRUNCATE, IR_EXTEND,

    // Stack operations (for expression evaluation)
    IR_PUSH, IR_POP, IR_DUP,

    // WASM-specific
    IR_UNREACHABLE, IR_NOP, IR_SELECT
} IROpcode;

typedef struct {
    IROpcode opcode;
    Type result_type;
    Operand operands[3];  // Maximum 3 operands
    int operand_count;
} Instruction;
```

### Operand Types

```c
typedef enum {
    OPERAND_CONSTANT,     // Immediate value
    OPERAND_LOCAL,        // Local variable
    OPERAND_GLOBAL,       // Global variable
    OPERAND_MEMORY,       // Memory address
    OPERAND_LABEL         // Branch target
} OperandType;

typedef struct {
    OperandType type;
    Type value_type;
    union {
        ConstantValue constant;
        int local_index;
        int global_index;
        int memory_offset;
        int label_id;
    } value;
} Operand;
```

### Type System

WASM has limited native types, so we use a two-level type system:

```c
// WASM native types
typedef enum {
    WASM_I32,
    WASM_I64,
    WASM_F32,
    WASM_F64,
    WASM_FUNCREF,
    WASM_EXTERNREF
} WASMType;

// C99 types (for optimization and validation)
typedef enum {
    TYPE_VOID,
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_F32, TYPE_F64,
    TYPE_POINTER,      // Lowered to i32 (linear memory offset)
    TYPE_ARRAY,        // Lowered to pointer + size info
    TYPE_STRUCT,       // Lowered to multiple values or memory
    TYPE_FUNCTION      // Lowered to function table index
} TypeKind;

typedef struct Type {
    TypeKind kind;
    WASMType wasm_type;    // Corresponding WASM type
    size_t size;           // Size in bytes
    size_t alignment;      // Alignment requirement
    union {
        struct {
            struct Type* element_type;
            size_t element_count;
        } array;
        struct {
            struct Type* pointee_type;
        } pointer;
        struct {
            struct Type* return_type;
            struct Type* param_types;
            size_t param_count;
        } function;
        struct {
            struct Type* field_types;
            size_t field_count;
            size_t* field_offsets;
        } struct_info;
    } details;
} Type;
```

## Memory Model

### Linear Memory and Stack Simulation

WASM uses linear memory, so we simulate the C stack:

- **Linear Memory**: Single contiguous memory space (0 to max_memory)
- **Stack Simulation**: Use global stack pointer, grow upward from low addresses
- **Heap**: Allocated from low addresses upward, starting at the end of the allocated stack
- **Local Variables**: Either WASM locals or simulated stack slots
  - Conditions that require putting a value in the simulated stack rather than WASM locals:
    - the address of the value is taken
    - the value has a variable size (like a variable sized array)
    - it is value with a width larger than 128 bits
      - (structs with width <= 128 bits may be WASM locals)

### Memory Layout

```
Linear Memory Layout:
[0x00000000       ] - Static data (globals, string literals)
[              ...] - Stack base (grows upward)
[Stack base + 1 Mb] - Heap base
```

### Memory Operations

- `IR_ALLOCA`: Allocate stack space (adjust stack pointer)
- `IR_LOAD/STORE`: Access heap
- `IR_LOAD_LOCAL/STORE_LOCAL`: Access WASM locals or stack slots
- `IR_LOAD_GLOBAL/STORE_GLOBAL`: Access global variables in data section
- `IR_STACK_SAVE/RESTORE`: Save/restore stack pointer for variable arrays

## Control Flow

### Structured Control Flow

WASM enforces structured control flow with nested blocks:

- **Regions**: Hierarchical control structures (block, loop, if)
- **Depth-based**: Break/continue reference nesting depth
- **Stack-based**: Labels form a stack for structured jumps

### Control Flow Instructions

- `IR_BLOCK`: Start a block region
- `IR_LOOP`: Start a loop region
- `IR_IF`: Start conditional region
- `IR_ELSE`: Alternative path in conditional
- `IR_END`: End current region
- `IR_BREAK`: Break to outer region (by depth)
- `IR_CONTINUE`: Continue loop (by depth)
- `IR_RETURN`: Return from function
- `IR_CALL`: Direct function call
- `IR_CALL_INDIRECT`: Indirect call through function table

### Function Tables

For indirect calls and function pointers:

```c
typedef struct FunctionTable {
    int* function_indices;    // Function indices in table
    size_t size;
    size_t capacity;
} FunctionTable;
```

## Example IR Generation

C99 code:

```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

Generated IR:

```
function factorial(i32 %n) -> i32 {
    %1 = IR_CONST_INT i32 1
    %2 = IR_LE i32 %n, %1
    IR_IF %2
        %3 = IR_CONST_INT i32 1
        IR_RETURN %3
    IR_ELSE
        %4 = IR_CONST_INT i32 1
        %5 = IR_SUB i32 %n, %4
        %6 = IR_CALL factorial(%5)
        %7 = IR_MUL i32 %n, %6
        IR_RETURN %7
    IR_END
}
```

## WASM Code Generation

### Instruction Mapping

- `IR_ADD` → `i32.add` / `i64.add` / `f32.add` / `f64.add` (based on type)
- `IR_LOAD` → `i32.load` / `i64.load` / `f32.load` / `f64.load` (with alignment)
- `IR_STORE` → `i32.store` / `i64.store` / `f32.store` / `f64.store`
- `IR_IF` → `if` block with result type
- `IR_LOOP` → `loop` block
- `IR_BLOCK` → `block` with result type
- `IR_BREAK` → `br` with depth
- `IR_CALL` → `call` instruction
- `IR_CALL_INDIRECT` → `call_indirect` with function table
- `IR_SELECT` → `select` instruction
- `IR_ALLOCA` → Adjust stack pointer global

### Memory Management

- Linear memory model for all memory operations
- Global stack pointer for stack simulation
- Global variables in data section
- Function tables for indirect calls

## Optimization Opportunities

1. **Constant Folding**: Evaluate constant expressions at compile time
2. **Dead Code Elimination**: Remove unreachable instructions
3. **Common Subexpression Elimination**: Reuse computed values

## Implementation Notes

- All IR operations are arena-allocated for memory efficiency
- Structured control flow ensures WASM compatibility
- Type information is preserved for optimization and validation
- Two-phase lowering: C99 semantics → WASM-compatible IR → WASM bytecode
- Function tables managed globally for indirect calls
- Stack simulation requires careful pointer arithmetic

## WASM-Specific Considerations

- **Structured Control Flow**: All control flow must be properly nested
- **Limited Types**: Only i32, i64, f32, f64 are native
- **Linear Memory**: Single address space
- **Function Tables**: Required for function pointers and indirect calls
- **No Exceptions**: Error handling through return codes or traps
- **Stack Simulation**: C-style stack must be manually managed

This IR design addresses WASM's constraints while maintaining C99 semantics, enabling efficient compilation through structured control flow and explicit memory management.
