#ifndef COMPILER_H
#define COMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Memory management
typedef struct Arena Arena;
Arena* arena_create(size_t size);
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

// Code generation
void codegen_emit_wasm(Arena* arena, ASTNode* ast, const char* output_path);

#endif  // COMPILER_H
