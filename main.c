#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

static char* read_file(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* content = malloc(size + 1);
  if (!content) {
    fclose(file);
    return NULL;
  }

  fread(content, 1, size, file);
  content[size] = '\0';
  fclose(file);

  return content;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Usage: %s <source.c>\n", argv[0]);
    return 1;
  }

  const char* input_path = argv[1];

  // Read source file
  char* source = read_file(input_path);
  if (!source) {
    printf("Error: Could not read file %s\n", input_path);
    return 1;
  }

  // Create arena for compilation
  Arena* arena = arena_init(1024 * 1024);  // 1MB arena
  if (!arena) {
    printf("Error: Could not create arena\n");
    free(source);
    return 1;
  }

  // Create error list
  ErrorList* errors = error_list_create(arena);
  if (!errors) {
    printf("Error: Could not create error list\n");
    arena_free(arena);
    free(source);
    return 1;
  }

  // Tokenize
  Lexer* lexer = lexer_create(arena, source, input_path, errors);
  if (!lexer) {
    printf("Error: Could not create lexer\n");
    arena_free(arena);
    free(source);
    return 1;
  }

  // Parse
  Parser* parser = parser_create(arena, lexer, errors);
  if (!parser) {
    printf("Error: Could not create parser\n");
    arena_free(arena);
    free(source);
    return 1;
  }

  ASTNode* ast = parser_parse_program(parser);
  
  // Print errors if any occurred
  if (error_list_has_errors(errors)) {
    error_list_print(errors, input_path);
    arena_free(arena);
    free(source);
    return 1;
  }
  
  if (!ast) {
    printf("Error: Parse failed\n");
    arena_free(arena);
    free(source);
    return 1;
  }

  // Generate IR
  IRModule* ir_module = ir_generate(arena, ast);
  if (!ir_module) {
    printf("Error: IR generation failed\n");
    arena_free(arena);
    free(source);
    return 1;
  }

  // Generate WASM
  codegen_emit_wasm(arena, ir_module, "out.wasm");

  printf("Compilation successful. Output written to out.wasm\n");

  // Cleanup
  arena_free(arena);
  free(source);

  return 0;
}
