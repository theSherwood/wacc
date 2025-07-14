#ifndef COMPILER_H
#define COMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Memory management
typedef struct Arena Arena;
Arena* arena_init(size_t size);
void arena_free(Arena* arena);
void* arena_alloc(Arena* arena, size_t size);

// Token types
typedef enum {
  TOKEN_EOF,
  TOKEN_INT,
  TOKEN_IDENTIFIER,
  TOKEN_RETURN,
  TOKEN_OPEN_PAREN,
  TOKEN_CLOSE_PAREN,
  TOKEN_OPEN_BRACE,
  TOKEN_CLOSE_BRACE,
  TOKEN_SEMICOLON,
  TOKEN_INTEGER_LITERAL,
  TOKEN_ERROR
} TokenType;

typedef struct Token {
  TokenType type;
  const char* start;
  size_t length;
  int line;
  int column;
} Token;

// Lexer
typedef struct Lexer {
  const char* source;
  const char* current;
  int line;
  int column;
} Lexer;

Lexer* lexer_create(Arena* arena, const char* source);
Token lexer_next_token(Lexer* lexer);

// AST nodes
typedef enum { AST_PROGRAM, AST_FUNCTION, AST_RETURN_STATEMENT, AST_INTEGER_CONSTANT } ASTNodeType;

typedef struct ASTNode {
  ASTNodeType type;
  union {
    struct {
      struct ASTNode* function;
    } program;
    struct {
      const char* name;
      struct ASTNode* statement;
    } function;
    struct {
      struct ASTNode* expression;
    } return_statement;
    struct {
      int value;
    } integer_constant;
  } data;
} ASTNode;

// Parser
typedef struct Parser {
  Lexer* lexer;
  Token current_token;
  Arena* arena;
} Parser;

Parser* parser_create(Arena* arena, Lexer* lexer);
ASTNode* parser_parse_program(Parser* parser);

// IR types
typedef enum {
    WASM_I32,
    WASM_I64,
    WASM_F32,
    WASM_F64,
    WASM_FUNCREF,
    WASM_EXTERNREF
} WASMType;

typedef enum {
    TYPE_VOID,
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_F32, TYPE_F64,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_FUNCTION
} TypeKind;

typedef struct Type {
    TypeKind kind;
    WASMType wasm_type;
    size_t size;
    size_t alignment;
} Type;

typedef enum {
    // Constants
    IR_CONST_INT,
    // Control Flow
    IR_RETURN,
    // Arithmetic
    IR_ADD, IR_SUB, IR_MUL, IR_DIV,
    // Memory
    IR_LOAD_LOCAL, IR_STORE_LOCAL,
    // Stack operations
    IR_PUSH, IR_POP
} IROpcode;

typedef enum {
    OPERAND_REGISTER,
    OPERAND_CONSTANT,
    OPERAND_LOCAL
} OperandType;

typedef struct {
    int int_value;
    float float_value;
} ConstantValue;

typedef struct {
    OperandType type;
    Type value_type;
    union {
        int reg;
        ConstantValue constant;
        int local_index;
    } value;
} Operand;

typedef struct {
    IROpcode opcode;
    Type result_type;
    Operand operands[3];
    int operand_count;
    int result_reg;
} Instruction;

typedef struct {
    Instruction* instructions;
    size_t instruction_count;
    size_t capacity;
} IRFunction;

typedef struct {
    IRFunction* functions;
    size_t function_count;
} IRModule;

// IR generation
IRModule* ir_generate(Arena* arena, ASTNode* ast);

// Code generation
void codegen_emit_wasm(Arena* arena, IRModule* ir_module, const char* output_path);

#endif  // COMPILER_H
