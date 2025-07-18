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

typedef enum { ERROR_LEXICAL, ERROR_SYNTAX, ERROR_SEMANTIC, ERROR_CODEGEN, WARNING } ErrorLevel;

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
#define ERROR_LEX_INVALID_CHARACTER 1001
#define ERROR_LEX_UNTERMINATED_STRING 1002
#define ERROR_LEX_UNTERMINATED_COMMENT 1003
#define ERROR_LEX_INVALID_ESCAPE_SEQUENCE 1004
#define ERROR_LEX_NUMBER_TOO_LARGE 1005

#define ERROR_SYNTAX_EXPECTED_TOKEN 2001
#define ERROR_SYNTAX_UNEXPECTED_TOKEN 2002
#define ERROR_SYNTAX_MISSING_SEMICOLON 2003
#define ERROR_SYNTAX_MISSING_BRACE 2004
#define ERROR_SYNTAX_MISSING_PAREN 2005
#define ERROR_SYNTAX_MALFORMED_EXPRESSION 2006
#define ERROR_SYNTAX_EXPECTED_FUNCTION 2007
#define ERROR_SYNTAX_EXPECTED_STATEMENT 2008
#define ERROR_SYNTAX_EXPECTED_EXPRESSION 2009
#define ERROR_SYNTAX_MISSING_OPERATOR 2010

#define ERROR_SEM_UNDEFINED_VARIABLE 3001
#define ERROR_SEM_UNDEFINED_FUNCTION 3002
#define ERROR_SEM_TYPE_MISMATCH 3003
#define ERROR_SEM_REDEFINITION 3004
#define ERROR_SEM_INVALID_ASSIGNMENT 3005
#define ERROR_SEM_INVALID_CALL 3006
#define ERROR_SEM_BREAK_OUTSIDE_LOOP 3007
#define ERROR_SEM_CONTINUE_OUTSIDE_LOOP 3008
#define ERROR_SEM_DEPENDENT_STATEMENT_ASSIGNMENT 3009

#define ERROR_CODEGEN_WASM_LIMIT_EXCEEDED 4001
#define ERROR_CODEGEN_INVALID_MEMORY_ACCESS 4002
#define ERROR_CODEGEN_UNSUPPORTED_OPERATION 4003

// Memory management
typedef struct Arena Arena;
Arena* arena_init();
void arena_free(Arena* arena);
void* arena_alloc(Arena* arena, size_t size);

// Error handling functions
ErrorList* error_list_create(Arena* arena);
void error_list_add(ErrorList* errors, Arena* arena, int id, ErrorLevel level, SourceLocation location,
                    const char* message, const char* suggestion, const char* context);
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
  TOKEN_BANG,       // !
  TOKEN_TILDE,      // ~
  TOKEN_MINUS,      // -
  TOKEN_PLUS,       // +
  TOKEN_STAR,       // *
  TOKEN_SLASH,      // /
  TOKEN_PERCENT,    // %
  TOKEN_EQ,         // =
  TOKEN_EQ_EQ,      // ==
  TOKEN_BANG_EQ,    // !=
  TOKEN_LT,         // <
  TOKEN_GT,         // >
  TOKEN_LT_EQ,      // <=
  TOKEN_GT_EQ,      // >=
  TOKEN_AMP_AMP,    // &&
  TOKEN_PIPE_PIPE,  // ||
  TOKEN_IF,         // if
  TOKEN_ELSE,       // else
  TOKEN_DO,         // do
  TOKEN_WHILE,      // while
  TOKEN_BREAK,      // break
  TOKEN_CONTINUE,   // continue
  TOKEN_QUESTION,   // ?
  TOKEN_COLON,      // :
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
typedef enum {
  AST_PROGRAM,
  AST_FUNCTION,
  AST_RETURN_STATEMENT,
  AST_INTEGER_CONSTANT,
  AST_UNARY_OP,
  AST_BINARY_OP,
  AST_VARIABLE_DECL,
  AST_VARIABLE_REF,
  AST_ASSIGNMENT,
  AST_IF_STATEMENT,
  AST_DO_STATEMENT,
  AST_WHILE_STATEMENT,
  AST_BREAK_STATEMENT,
  AST_CONTINUE_STATEMENT,
  AST_TERNARY_EXPRESSION,
  AST_COMPOUND_STATEMENT
} ASTNodeType;

typedef struct ASTNode {
  ASTNodeType type;
  int line;
  int column;
  union {
    struct {
      struct ASTNode* function;
    } program;
    struct {
      const char* name;
      struct ASTNode** statements;
      int statement_count;
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
    struct {
      TokenType operator;
      struct ASTNode* left;
      struct ASTNode* right;
    } binary_op;
    struct {
      const char* name;
      struct ASTNode* initializer;
    } variable_decl;
    struct {
      const char* name;
    } variable_ref;
    struct {
      const char* name;
      struct ASTNode* value;
    } assignment;
    struct {
      struct ASTNode* condition;
      struct ASTNode* then_statement;
      struct ASTNode* else_statement;  // Can be NULL
    } if_statement;
    struct {
      struct ASTNode* body;
      struct ASTNode* condition;
    } do_while_statement;
    struct {
      struct ASTNode* condition;
      struct ASTNode* body;
    } while_statement;
    struct {
      // Break statements have no additional data
    } break_statement;
    struct {
      struct ASTNode* condition;
      struct ASTNode* true_expression;
      struct ASTNode* false_expression;
    } ternary_expression;
    struct {
      struct ASTNode** statements;
      int statement_count;
    } compound_statement;
  } data;
} ASTNode;

// Parser
typedef struct Parser {
  Lexer* lexer;
  Token current_token;
  Arena* arena;
  ErrorList* errors;
  ASTNode* ast;
} Parser;

Parser* parser_create(Arena* arena, Lexer* lexer, ErrorList* errors);
ASTNode* parser_parse_program(Parser* parser);

// AST debugging
void ast_print(ASTNode* ast);

// IR types - WASM native types
typedef enum { WASM_I32, WASM_I64, WASM_F32, WASM_F64, WASM_FUNCREF, WASM_EXTERNREF } WASMType;

// C99 types (for optimization and validation)
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
  TYPE_POINTER,  // Lowered to i32 (linear memory offset)
  TYPE_ARRAY,    // Lowered to pointer + size info
  TYPE_STRUCT,   // Lowered to multiple values or memory
  TYPE_FUNCTION  // Lowered to function table index
} TypeKind;

typedef struct Type {
  TypeKind kind;
  WASMType wasm_type;  // Corresponding WASM type
  size_t size;         // Size in bytes
  size_t alignment;    // Alignment requirement
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

typedef struct Region Region;

// IR Opcodes - Stack-based WASM-oriented instructions
typedef enum {
  // Arithmetic
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,
  IR_MOD,
  IR_NEG,
  IR_NOT,
  IR_BITWISE_AND,
  IR_BITWISE_OR,
  IR_BITWISE_XOR,
  IR_BITWISE_NOT,

  // Comparison
  IR_EQ,
  IR_NE,
  IR_LT,
  IR_LE,
  IR_GT,
  IR_GE,

  // Logical
  IR_LOGICAL_AND,
  IR_LOGICAL_OR,
  IR_LOGICAL_NOT,

  // Memory
  IR_LOAD,
  IR_STORE,
  IR_LOAD_GLOBAL,
  IR_STORE_GLOBAL,
  IR_ALLOCA,
  IR_LOAD_LOCAL,
  IR_STORE_LOCAL,
  IR_STACK_SAVE,
  IR_STACK_RESTORE,
  IR_MEMCPY,
  IR_MEMSET,

  // Control Flow
  IR_BLOCK,
  IR_LOOP,
  IR_IF,
  IR_ELSE,
  IR_END,
  IR_BREAK,
  IR_CONTINUE,
  IR_RETURN,
  IR_CALL,
  IR_CALL_INDIRECT,

  // Constants
  IR_CONST_INT,
  IR_CONST_FLOAT,
  IR_CONST_STRING,

  // Type conversions
  IR_CAST,
  IR_TRUNCATE,
  IR_EXTEND,

  // Stack operations (for expression evaluation)
  IR_PUSH,
  IR_POP,
  IR_DUP,

  // Region
  // This lets us define regions of code that can be ordered between and
  // among other instructions
  IR_REGION,

  // WASM-specific
  IR_UNREACHABLE,
  IR_NOP,
  IR_SELECT
} IROpcode;

// Operand types
typedef enum {
  OPERAND_REGION,    // Region value
  OPERAND_CONSTANT,  // Immediate value
  OPERAND_LOCAL,     // Local variable
  OPERAND_GLOBAL,    // Global variable
  OPERAND_MEMORY,    // Memory address
  OPERAND_LABEL      // Branch target
} OperandType;

typedef struct {
  int int_value;
  float float_value;
} ConstantValue;

typedef struct {
  OperandType type;
  Type value_type;
  union {
    ConstantValue constant;
    int local_index;
    int global_index;
    int memory_offset;
    int label_id;
    Region* region;
  } value;
} Operand;


// Stack-based instruction
typedef struct {
  IROpcode opcode;
  Type result_type;
  Operand operands[3];  // Maximum 3 operands
  int operand_count;
} Instruction;

// Structured control flow regions
typedef enum {
  REGION_BLOCK,    // Linear sequence of instructions
  REGION_LOOP,     // Loop construct
  REGION_IF,       // If-then-else construct
  REGION_FUNCTION  // Function body
} RegionType;

struct Region {
  RegionType type;
  TypeKind kind;
  int id;
  bool is_expression;  // True if this is an expression context (e.g., ternary)
  Instruction* instructions;
  size_t instruction_count;
  size_t instruction_capacity;
  struct Region** children;  // Nested regions
  size_t child_count;
  size_t child_capacity;
  struct Region* parent;    // Parent region
  struct Region* function;  // Function region

  // For control flow
  union {
    struct {
      struct Region* then_region;
      struct Region* else_region;
    } if_data;
    struct {
      struct Region* body;
    } loop_data;
    struct {
      struct Region* locals;
    } function_data;
  } data;
};

// Local variable
typedef struct {
  const char* name;
  Type type;
  int index;
  bool is_stack_based;  // True if allocated on simulated stack vs WASM local
} LocalVariable;

// Function parameter
typedef struct {
  const char* name;
  Type type;
  int index;
} Parameter;

// IR Function with structured regions
typedef struct {
  const char* name;
  Type return_type;
  Parameter* parameters;
  size_t param_count;
  Region* body;  // Function body as root region
  LocalVariable* locals;
  size_t local_count;
  size_t local_capacity;
  int max_stack_size;  // For stack simulation
} Function;

// IR Module
typedef struct {
  Function* functions;
  size_t function_count;
  size_t function_capacity;
} IRModule;

// Semantic analysis
bool semantic_analyze(Arena* arena, ASTNode* ast, ErrorList* errors, const char* source);

// IR generation
IRModule* ir_generate(Arena* arena, ASTNode* ast);

// IR debugging
void ir_print(IRModule* ir_module);

// Code generation
void codegen_emit_wasm(Arena* arena, IRModule* ir_module, const char* output_path);

#endif  // COMPILER_H
