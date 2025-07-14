# Implementation Plan

## Basic Strategy

We are implementing a C99 compiler in C99 that targets WASM. We want to be able to target both 32-bit and 64-bit WASM.

We are roughly following [An Incremental Approach to Compiler Construction](./an_incremental_approach_to_compiler_construction.pdf). But whereas that paper uses Scheme to implement a Scheme compiler, we use C to implement a C compiler. Refer to https://norasandler.com/2017/11/29/Write-a-Compiler.html.

Also refer to https://github.com/kign/c4wa, which implements a C compiler in Java that targets WASM.

Each step of the compiler will take an arena as its first argument. All allocations are to be made into the arena.

## Compiler Phases

1. **Lexical Analysis (Tokenization)**

   - Break source code into tokens (keywords, identifiers, literals, operators, delimiters)
   - Handle whitespace and comments
   - Return stream of tokens with position information

2. **Syntax Analysis (Parsing)**

   - Transform tokens into Abstract Syntax Tree (AST)
   - Use recursive descent parser
   - Handle operator precedence and associativity
   - Validate syntax according to C99 grammar subset

3. **Semantic Analysis**

   - Type checking and inference
   - Symbol table management
   - Scope resolution
   - Function signature validation
   - Declaration/definition matching

4. **Intermediate Representation (IR) Generation**

   - Convert AST to intermediate representation suitable for WASM
   - Handle control flow (if/else, loops, function calls)
   - Memory layout planning
   - Function prologue/epilogue generation

5. **Code Generation**

   - Generate WASM bytecode from IR
   - Handle WASM linear memory model
   - Generate function imports/exports
   - Optimize for minimal WASM output

6. **Output Generation**
   - Generate both binary WASM (.wasm) and text format (.wat)
   - Create module metadata
   - Handle external function declarations

## Testing

At each stage of development, we will test extensively to ensure that the changes we make are correct for both 32-bit and 64-bit WASM.

Tests are in `./tests/` directory. There are subdirectories. Any new tests should be added to the appropriate subdirectory.

### `valid` and `invalid` directories

These tests cover compile-time behavior, but do not cover run-time behavior. All parser tests will consist of a code file in C. `valid` tests must parse successfully. `invalid` tests must fail to parse. `invalid` tests will include comments at the end of the file that specify the line numbers and error codes of the compiler errors. If the comments with the line numbers and error codes are missing from a test, they must be added to the test.

### `runtime` directory

These tests should be compiled and run. Comments at the end of the file should be used to verify the output of the C code.

## Error Handling Strategy

The compiler implements a comprehensive error collection and reporting system that continues compilation through multiple errors to provide maximum feedback to the user in a single compilation pass.

### Core Principles

1. **Error Collection**: Each compiler phase collects all errors encountered rather than stopping at the first error
2. **Continuation**: The compiler attempts to continue compilation even after encountering errors
3. **Context Preservation**: Errors include detailed source location and context information
4. **User-Friendly Messages**: Error messages are clear, actionable, and include suggested fixes when possible

### Error Categories

1. **Lexical Errors**

   - Invalid character sequences
   - Malformed literals (strings, numbers)
   - Unterminated comments or strings
   - Invalid escape sequences

2. **Syntax Errors**

   - Missing tokens (semicolons, braces, parentheses)
   - Unexpected tokens
   - Malformed expressions or statements
   - Invalid grammar productions

3. **Semantic Errors**

   - Type mismatches
   - Undefined variables or functions
   - Redefinition errors
   - Invalid operations (e.g., assignment to const)
   - Scope violations

4. **Code Generation Errors**
   - WASM instruction limits exceeded
   - Invalid memory access patterns
   - Unsupported operations for target architecture

### Error Recovery Mechanisms

#### Lexer Recovery

- Skip invalid characters and continue tokenization
- Use synchronization tokens (semicolons, braces) to resume parsing
- Maintain accurate line/column tracking even after errors

#### Parser Recovery

- **Panic Mode**: On syntax error, skip tokens until reaching a synchronization point
- **Synchronization Points**: Statement boundaries (`{`, `}`, `;`), function boundaries
- **Error Productions**: Add grammar rules for common syntax errors to provide better messages
- **Minimum Distance Recovery**: Choose recovery strategy that minimizes token skipping

#### Semantic Recovery

- Insert placeholder symbols for undefined identifiers
- Use error types for type-checking continuation
- Maintain symbol table consistency even with errors

### Error Reporting Structure

```c
typedef struct {
    const char* filename;
    int line;
    int column;
    int start_pos;
    int end_pos;
} SourceLocation;

typedef enum {
    ERROR_LEXICAL,
    ERROR_SYNTAX,
    ERROR_SEMANTIC,
    ERROR_CODEGEN,
    WARNING
} ErrorLevel;

typedef struct {
    ErrorLevel level;
    SourceLocation location;
    const char* message;
    const char* suggestion;  // Optional fix suggestion
    const char* context;     // Relevant source line
} CompilerError;

typedef struct {
    CompilerError* errors;
    size_t count;
    size_t capacity;
    bool has_fatal_errors;
} ErrorList;
```

### Error Reporting Format

```
filename.c:line:column: id: error_id error: message
   source code line
   ^^^^^^^^^^^
note: suggestion (if available)
```

### Implementation Guidelines

1. **Early Error Detection**: Catch errors as early as possible in the pipeline
2. **Error Limits**: Stop compilation after a reasonable number of errors (e.g., 100) to prevent overwhelming output
3. **Warning System**: Distinguish between errors (compilation stops) and warnings (compilation continues)
4. **IDE Integration**: Provide structured error output suitable for editor integration
5. **Color Coding**: Use ANSI color codes for terminal output when supported

### Testing Error Handling

Each compiler phase includes negative test cases that verify:

- Proper error detection and reporting
- Continued compilation after errors
- Accurate error location reporting
- Recovery mechanism effectiveness

## Implementation Steps

Following the incremental approach, we'll build the compiler in small, testable steps:

### Step 1: Integer Constants

Refer to https://norasandler.com/2017/11/29/Write-a-Compiler.html

**Goal**: Compile `int main() { return 42; }` to WASM

**Features to implement**:

- **Lexer**: Recognize `int`, `main`, `return`, `{`, `}`, `(`, `)`, `;`, integer literals
- **Parser**: Handle simple function declaration with return statement
- **AST**: `Program(Function(name, Statement))` where `Statement` is `Return(Constant(int))`
- **IR**: Initialize IR structure with basic `Function` and `Region` types, implement `IR_CONST_INT` and `IR_RETURN` instructions
- **Codegen**: Generate WASM function that returns integer constant from IR

**Grammar**:

```
<program> ::= <function>
<function> ::= "int" <id> "(" ")" "{" <statement> "}"
<statement> ::= "return" <exp> ";"
<exp> ::= <int>
```

**WASM output**: Simple function with i32.const and return

### Step 2: Unary Operators

Refer to https://norasandler.com/2017/12/05/Write-a-Compiler-2.html

**Goal**: Add support for `!`, `~`, `-` operators

**New features**:

- **Lexer**: Recognize unary operators
- **Parser**: Handle operator precedence for unary expressions
- **AST**: Add `UnaryOp(operator, operand)` node
- **IR**: Add `IR_NEG`, `IR_NOT`, `IR_BITWISE_NOT` instructions and virtual register system
- **Codegen**: Generate appropriate WASM instructions (i32.eqz, i32.sub, etc.) from IR

**Grammar additions**:

```
<exp> ::= <int> | <unary_op> <exp>
<unary_op> ::= "!" | "~" | "-"
```

### Step 3: Binary Operators

Refer to https://norasandler.com/2017/12/15/Write-a-Compiler-3.html
and https://norasandler.com/2017/12/28/Write-a-Compiler-4.html

**Goal**: Add arithmetic operators `+`, `-`, `*`, `/`, `%`

**New features**:

- **Lexer**: Recognize binary operators
- **Parser**: Handle operator precedence and associativity
- **AST**: Add `BinaryOp(left, operator, right)` node
- **IR**: Add `IR_ADD`, `IR_SUB`, `IR_MUL`, `IR_DIV`, `IR_MOD`, comparison and logical operators
- **Codegen**: Generate WASM arithmetic instructions from IR

**Grammar additions**:

```
<exp> ::= <logical_or_exp>
<logical_or_exp> ::= <logical_and_exp> ("||" <logical_and_exp>)*
<logical_and_exp> ::= <equality_exp> ("&&" <equality_exp>)*
<equality_exp> ::= <relational_exp> (("==" | "!=") <relational_exp>)*
<relational_exp> ::= <additive_exp> (("<" | ">" | "<=" | ">=") <additive_exp>)*
<additive_exp> ::= <multiplicative_exp> (("+" | "-") <multiplicative_exp>)*
<multiplicative_exp> ::= <unary_exp> (("*" | "/" | "%") <unary_exp>)*
<unary_exp> ::= <postfix_exp> | <unary_op> <unary_exp>
<postfix_exp> ::= <primary_exp>
<primary_exp> ::= <int> | "(" <exp> ")"
```

### Step 4: Local Variables

Refer to https://norasandler.com/2018/01/08/Write-a-Compiler-5.html

**Goal**: Add support for variable declarations and assignments

**New features**:

- **Lexer**: Recognize assignment operator `=`
- **Parser**: Handle variable declarations and assignments
- **AST**: Add `Declaration(type, name, initializer)` and `Assignment(name, value)` nodes
- **Semantic**: Symbol table for local variables
- **IR**: Add `IR_LOAD_LOCAL`, `IR_STORE_LOCAL`, `IR_ALLOCA` instructions and local variable management
- **Codegen**: WASM local variables and get/set operations from IR

**Grammar additions**:

```
<statement> ::= "return" <exp> ";" | <declaration> | <assignment>
<declaration> ::= <type> <id> ("=" <exp>)? ";"
<assignment> ::= <id> "=" <exp> ";"
<primary_exp> ::= <int> | <id> | "(" <exp> ")"
```

### Step 5: Conditionals

Refer to https://norasandler.com/2018/02/25/Write-a-Compiler-6.html

**Goal**: Add `if` statements and ternary operator

**New features**:

- **Lexer**: Recognize `if`, `else`, `?`, `:`
- **Parser**: Handle conditional statements and expressions
- **AST**: Add `If(condition, then_stmt, else_stmt)` and `Ternary(condition, true_exp, false_exp)` nodes
- **IR**: Add `IR_IF`, `IR_ELSE`, `IR_END` instructions and structured control flow regions
- **Codegen**: WASM if/else blocks and branches from IR structured control flow

**Grammar additions**:

```
<statement> ::= "return" <exp> ";" | <declaration> | <assignment> | <if_statement>
<if_statement> ::= "if" "(" <exp> ")" <statement> ("else" <statement>)?
<exp> ::= <ternary_exp>
<ternary_exp> ::= <logical_or_exp> ("?" <exp> ":" <exp>)?
```

### Step 6: Compound Statements

Refer to https://norasandler.com/2018/03/14/Write-a-Compiler-7.html

**Goal**: Add support for `{` `}` blocks and multiple statements

**New features**:

- **Parser**: Handle statement blocks
- **AST**: Add `Compound(statements)` node
- **Semantic**: Block scope for variables
- **IR**: Add `IR_BLOCK` instruction and nested region management
- **Codegen**: WASM block structure from IR regions

**Grammar additions**:

```
<statement> ::= "return" <exp> ";" | <declaration> | <assignment> | <if_statement> | <compound_statement>
<compound_statement> ::= "{" <statement>* "}"
```

### Step 7: Loops

Refer to https://norasandler.com/2018/04/10/Write-a-Compiler-8.html

**Goal**: Add `for`, `while`, `do-while` loops

**New features**:

- **Lexer**: Recognize `for`, `while`, `do`, `break`, `continue`
- **Parser**: Handle loop constructs
- **AST**: Add `While(condition, body)`, `For(init, condition, update, body)`, `DoWhile(body, condition)` nodes
- **IR**: Add `IR_LOOP`, `IR_BREAK`, `IR_CONTINUE` instructions and loop region management
- **Codegen**: WASM loop blocks and branches from IR structured control flow

### Step 8: Functions

Refer to https://norasandler.com/2018/06/27/Write-a-Compiler-9.html

**Goal**: Add function calls and multiple function definitions

**New features**:

- **Parser**: Handle function parameters and calls
- **AST**: Add `FunctionCall(name, arguments)` and update `Function` node
- **Semantic**: Function signature validation
- **IR**: Add `IR_CALL` instruction and function table management
- **Codegen**: WASM function calls and parameter passing from IR

### Step 9: Global Variables

Refer to https://norasandler.com/2019/02/18/Write-a-Compiler-10.html

**Goal**: Add global variables

**New features**:

- **IR**: Add `IR_LOAD_GLOBAL`, `IR_STORE_GLOBAL` instructions and global variable management
- **Codegen**: WASM global variables and data section from IR

### Step 10: Arrays and Pointers

**Goal**: Add basic array and pointer support

**New features**:

- **Lexer**: Recognize `[`, `]`, `*`, `&`
- **Parser**: Handle array declarations and pointer operations
- **AST**: Add array and pointer nodes
- **IR**: Add `IR_LOAD`, `IR_STORE` instructions and pointer arithmetic operations
- **Codegen**: WASM linear memory operations from IR

### Step 11: Structs

**Goal**: Add struct definitions and member access

**New features**:

- **Lexer**: Recognize `struct`, `.`, `->`
- **Parser**: Handle struct definitions and member access
- **AST**: Add struct-related nodes
- **IR**: Add struct member access instructions and memory layout calculation
- **Codegen**: WASM memory layout for structs from IR

### Step 12: Standard Library Subset

**Goal**: Add basic standard library functions

**New features**:

- **Semantic**: Built-in function declarations
- **IR**: Add `IR_CALL_INDIRECT` instruction for external function calls
- **Codegen**: Import external functions (printf, malloc, etc.) from IR

### Step 13: Optimization and Cleanup

**Goal**: Optimize generated WASM and add error handling

**New features**:

- **IR**: Add optimization passes (dead code elimination, constant folding) on IR
- **Optimization**: Dead code elimination, constant folding at IR level
- **Error handling**: Better error messages and recovery
- **Testing**: Comprehensive test suite
