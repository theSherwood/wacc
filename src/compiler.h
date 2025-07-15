#ifndef COMPILER_H
#define COMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Error handling system
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
    int id;
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

// Error IDs - grouped by category
#define ERROR_LEX_INVALID_CHARACTER        1001
#define ERROR_LEX_UNTERMINATED_STRING      1002
#define ERROR_LEX_UNTERMINATED_COMMENT     1003
#define ERROR_LEX_INVALID_ESCAPE_SEQUENCE  1004
#define ERROR_LEX_NUMBER_TOO_LARGE         1005

#define ERROR_SYNTAX_EXPECTED_TOKEN        2001
#define ERROR_SYNTAX_UNEXPECTED_TOKEN      2002
#define ERROR_SYNTAX_MISSING_SEMICOLON     2003
#define ERROR_SYNTAX_MISSING_BRACE         2004
#define ERROR_SYNTAX_MISSING_PAREN         2005
#define ERROR_SYNTAX_MALFORMED_EXPRESSION  2006
#define ERROR_SYNTAX_EXPECTED_FUNCTION     2007
#define ERROR_SYNTAX_EXPECTED_STATEMENT    2008
#define ERROR_SYNTAX_EXPECTED_EXPRESSION   2009

#define ERROR_SEM_UNDEFINED_VARIABLE       3001
#define ERROR_SEM_UNDEFINED_FUNCTION       3002
#define ERROR_SEM_TYPE_MISMATCH            3003
#define ERROR_SEM_REDEFINITION             3004
#define ERROR_SEM_INVALID_ASSIGNMENT       3005

#define ERROR_CODEGEN_WASM_LIMIT_EXCEEDED   4001
#define ERROR_CODEGEN_INVALID_MEMORY_ACCESS 4002
#define ERROR_CODEGEN_UNSUPPORTED_OPERATION 4003

// Memory management
typedef struct Arena Arena;
Arena* arena_init();
void arena_free(Arena* arena);
void* arena_alloc(Arena* arena, size_t size);

// Error handling functions
ErrorList* error_list_create(Arena* arena);
void error_list_add(ErrorList* errors, Arena* arena, int id, ErrorLevel level, 
                   SourceLocation location, const char* message, const char* suggestion, 
                   const char* context);
void error_list_print(ErrorList* errors, const char* filename);
bool error_list_has_errors(ErrorList* errors);
const char* get_source_context(Arena* arena, const char* source, int line);

// Utility functions (stdlib replacements)
bool is_space(char c);
bool is_alpha(char c);
bool is_digit(char c);
bool is_alnum(char c);
size_t str_len(const char* str);
int str_ncmp(const char* s1, const char* s2, size_t n);
void str_ncpy(char* dest, const char* src, size_t n);
void mem_cpy(void* dest, const void* src, size_t n);
long str_to_long(const char* str, char** endptr, int base);

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
  TOKEN_BANG,        // !
  TOKEN_TILDE,       // ~
  TOKEN_MINUS,       // -
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
  ErrorList* errors;
  const char* filename;
} Lexer;

Lexer* lexer_create(Arena* arena, const char* source, const char* filename, ErrorList* errors);
Token lexer_next_token(Lexer* lexer);

// AST nodes
typedef enum { AST_PROGRAM, AST_FUNCTION, AST_RETURN_STATEMENT, AST_INTEGER_CONSTANT, AST_UNARY_OP } ASTNodeType;

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
    struct {
      TokenType operator;
      struct ASTNode* operand;
    } unary_op;
  } data;
} ASTNode;

// Parser
typedef struct Parser {
  Lexer* lexer;
  Token current_token;
  Arena* arena;
  ErrorList* errors;
} Parser;

Parser* parser_create(Arena* arena, Lexer* lexer, ErrorList* errors);
ASTNode* parser_parse_program(Parser* parser);

// IR types
typedef enum { WASM_I32, WASM_I64, WASM_F32, WASM_F64, WASM_FUNCREF, WASM_EXTERNREF } WASMType;

typedef enum {
  TYPE_VOID,
  TYPE_I8,
  TYPE_I16,
  TYPE_I32,
  TYPE_I64,
  TYPE_U8,
  TYPE_U16,
  TYPE_U32,
  TYPE_U64,
  TYPE_F32,
  TYPE_F64,
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
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,
  // Unary operations
  IR_NEG,          // -
  IR_NOT,          // !
  IR_BITWISE_NOT,  // ~
  // Memory
  IR_LOAD_LOCAL,
  IR_STORE_LOCAL,
  // Stack operations
  IR_PUSH,
  IR_POP
} IROpcode;

typedef enum { OPERAND_REGISTER, OPERAND_CONSTANT, OPERAND_LOCAL } OperandType;

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
