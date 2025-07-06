# Implementation Plan

## Basic Strategy

We are implementing a C99 compiler in C99 that targets WASM. We want to be able to target both 32-bit and 64-bit WASM.

We are roughly following [An Incremental Approach to Compiler Construction](./an_incremental_approach_to_compiler_construction.pdf). But whereas that paper uses Scheme to implement a Scheme compiler, we use C to implement a C compiler. Refer to https://norasandler.com/2017/11/29/Write-a-Compiler.html.

Also refer to https://github.com/kign/c4wa, which implements a C compiler in Java that targets WASM.

Each step of the compiler will take an arena as its first argument. All allocations are to be made into the arena.

At each stage of development, we will test extensively to ensure that the changes we make are correct for both 32-bit and 64-bit WASM.

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

## Implementation Steps

Following the incremental approach, we'll build the compiler in small, testable steps:

### Step 1: Integer Constants

Refer to https://norasandler.com/2017/11/29/Write-a-Compiler.html

**Goal**: Compile `int main() { return 42; }` to WASM

**Features to implement**:

- **Lexer**: Recognize `int`, `main`, `return`, `{`, `}`, `(`, `)`, `;`, integer literals
- **Parser**: Handle simple function declaration with return statement
- **AST**: `Program(Function(name, Statement))` where `Statement` is `Return(Constant(int))`
- **Codegen**: Generate WASM function that returns integer constant

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
- **Codegen**: Generate appropriate WASM instructions (i32.eqz, i32.sub, etc.)

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
- **Codegen**: Generate WASM arithmetic instructions

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
- **Codegen**: WASM local variables and get/set operations

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
- **Codegen**: WASM if/else blocks and branches

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
- **Codegen**: WASM block structure

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
- **Codegen**: WASM loop blocks and branches

### Step 8: Functions

Refer to https://norasandler.com/2018/06/27/Write-a-Compiler-9.html

**Goal**: Add function calls and multiple function definitions

**New features**:

- **Parser**: Handle function parameters and calls
- **AST**: Add `FunctionCall(name, arguments)` and update `Function` node
- **Semantic**: Function signature validation
- **Codegen**: WASM function calls and parameter passing

### Step 9: Global Variables

Refer to https://norasandler.com/2019/02/18/Write-a-Compiler-10.html

**Goal**: Add global variables

### Step 10: Arrays and Pointers

**Goal**: Add basic array and pointer support

**New features**:

- **Lexer**: Recognize `[`, `]`, `*`, `&`
- **Parser**: Handle array declarations and pointer operations
- **AST**: Add array and pointer nodes
- **Codegen**: WASM linear memory operations

### Step 11: Structs

**Goal**: Add struct definitions and member access

**New features**:

- **Lexer**: Recognize `struct`, `.`, `->`
- **Parser**: Handle struct definitions and member access
- **AST**: Add struct-related nodes
- **Codegen**: WASM memory layout for structs

### Step 12: Standard Library Subset

**Goal**: Add basic standard library functions

**New features**:

- **Semantic**: Built-in function declarations
- **Codegen**: Import external functions (printf, malloc, etc.)

### Step 13: Optimization and Cleanup

**Goal**: Optimize generated WASM and add error handling

**New features**:

- **Optimization**: Dead code elimination, constant folding
- **Error handling**: Better error messages and recovery
- **Testing**: Comprehensive test suite
