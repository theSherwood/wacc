#include <stdio.h>

#include "compiler.h"

// Symbol table for semantic analysis
typedef struct SymbolTable {
  const char** names;
  size_t count;
  size_t capacity;
  struct SymbolTable* parent;  // For nested scopes
} SymbolTable;

// Semantic analysis context
typedef struct SemanticContext {
  Arena* arena;
  ErrorList* errors;
  SymbolTable* current_scope;
  bool in_loop;        // Track if we're currently inside a loop
  const char* source;  // Source code for context
} SemanticContext;

// Symbol table operations
static SymbolTable* symbol_table_create(Arena* arena, SymbolTable* parent) {
  SymbolTable* table = arena_alloc(arena, sizeof(SymbolTable));
  if (!table) return NULL;

  table->names = NULL;
  table->count = 0;
  table->capacity = 0;
  table->parent = parent;
  return table;
}

static bool symbol_table_lookup(SymbolTable* table, const char* name) {
  // Search current scope first
  for (size_t i = 0; i < table->count; i++) {
    size_t name_len = str_len(name);
    size_t table_name_len = str_len(table->names[i]);
    if (name_len == table_name_len && str_ncmp(table->names[i], name, name_len) == 0) {
      return true;
    }
  }

  // Search parent scopes
  if (table->parent) {
    return symbol_table_lookup(table->parent, name);
  }

  return false;
}

static bool symbol_table_lookup_current_scope(SymbolTable* table, const char* name) {
  // Only search current scope (for redefinition checking)
  for (size_t i = 0; i < table->count; i++) {
    size_t name_len = str_len(name);
    size_t table_name_len = str_len(table->names[i]);
    if (name_len == table_name_len && str_ncmp(table->names[i], name, name_len) == 0) {
      return true;
    }
  }
  return false;
}

static void symbol_table_add(Arena* arena, SymbolTable* table, const char* name) {
  if (table->count >= table->capacity) {
    table->capacity = table->capacity ? table->capacity * 2 : 8;
    const char** new_names = arena_alloc(arena, table->capacity * sizeof(const char*));
    if (table->names) {
      mem_cpy(new_names, table->names, table->count * sizeof(const char*));
    }
    table->names = new_names;
  }

  table->names[table->count++] = name;
}

// Error reporting helper
static void report_semantic_error(SemanticContext* ctx, int error_id, const char* message, const char* suggestion,
                                  int line, int column) {
  if (!ctx->errors) return;

  SourceLocation location = {.filename = "source",  // We don't have access to filename here, but it's set by main
                             .line = line,
                             .column = column,
                             .start_pos = 0,
                             .end_pos = 0};

  const char* context = get_source_context(ctx->arena, ctx->source, line);
  error_list_add(ctx->errors, ctx->arena, error_id, ERROR_SEMANTIC, location, message, suggestion, context);
}

// Forward declarations
static bool analyze_expression(SemanticContext* ctx, ASTNode* expr);
static bool analyze_statement(SemanticContext* ctx, ASTNode* stmt);

static bool analyze_expression(SemanticContext* ctx, ASTNode* expr) {
  if (!expr) return true;

  switch (expr->type) {
    case AST_INTEGER_CONSTANT: {
      return true;  // Integer constants are always valid
    }

    case AST_VARIABLE_REF: {
      // Check if variable is declared
      if (!symbol_table_lookup(ctx->current_scope, expr->data.variable_ref.name)) {
        report_semantic_error(ctx, ERROR_SEM_UNDEFINED_VARIABLE, "undeclared variable",
                              "declare the variable before using it", expr->line, expr->column);
        return false;
      }
      return true;
    }

    case AST_UNARY_OP: {
      return analyze_expression(ctx, expr->data.unary_op.operand);
    }

    case AST_BINARY_OP: {
      bool left_ok = analyze_expression(ctx, expr->data.binary_op.left);
      bool right_ok = analyze_expression(ctx, expr->data.binary_op.right);
      return left_ok && right_ok;
    }

    case AST_ASSIGNMENT: {
      // Check if variable is declared before assignment
      if (!symbol_table_lookup(ctx->current_scope, expr->data.assignment.name)) {
        report_semantic_error(ctx, ERROR_SEM_UNDEFINED_VARIABLE, "undeclared variable in assignment",
                              "declare the variable before assigning to it", expr->line, expr->column);
        return false;
      }

      // Analyze the assigned value
      return analyze_expression(ctx, expr->data.assignment.value);
    }

    default:
      return true;
  }
}

static bool analyze_statement(SemanticContext* ctx, ASTNode* stmt) {
  if (!stmt) return true;

  switch (stmt->type) {
    case AST_RETURN_STATEMENT: {
      return analyze_expression(ctx, stmt->data.return_statement.expression);
    }

    case AST_VARIABLE_DECL: {
      const char* name = stmt->data.variable_decl.name;

      // Check for redefinition in current scope
      if (symbol_table_lookup_current_scope(ctx->current_scope, name)) {
        report_semantic_error(ctx, ERROR_SEM_REDEFINITION, "variable redefinition", "use a different variable name",
                              stmt->line, stmt->column);
        return false;
      }

      // Add variable to symbol table
      symbol_table_add(ctx->arena, ctx->current_scope, name);

      // Analyze initializer if present
      if (stmt->data.variable_decl.initializer) {
        return analyze_expression(ctx, stmt->data.variable_decl.initializer);
      }

      return true;
    }

    case AST_ASSIGNMENT: {
      return analyze_expression(ctx, stmt);
    }

    case AST_COMPOUND_STATEMENT: {
      // Create a new scope for the compound statement
      SymbolTable* previous_scope = ctx->current_scope;
      ctx->current_scope = symbol_table_create(ctx->arena, previous_scope);

      if (!ctx->current_scope) {
        ctx->current_scope = previous_scope;
        return false;
      }

      bool success = true;

      // Analyze all statements in the compound statement
      for (int i = 0; i < stmt->data.compound_statement.statement_count; i++) {
        if (!analyze_statement(ctx, stmt->data.compound_statement.statements[i])) {
          success = false;
          // Continue analyzing other statements to collect all errors
        }
      }

      // Restore the previous scope
      ctx->current_scope = previous_scope;

      return success;
    }

    case AST_IF_STATEMENT: {
      bool success = true;

      // Analyze the condition
      if (!analyze_expression(ctx, stmt->data.if_statement.condition)) {
        success = false;
      }

      // Check if the then statement is a variable declaration without braces
      if (stmt->data.if_statement.then_statement && stmt->data.if_statement.then_statement->type == AST_VARIABLE_DECL) {
        report_semantic_error(
            ctx, ERROR_SEM_DEPENDENT_STATEMENT_ASSIGNMENT, "variable declaration cannot be used as dependent statement",
            "use braces {} to create a compound statement", stmt->data.if_statement.then_statement->line,
            stmt->data.if_statement.then_statement->column);
        success = false;
      }

      // Check if the else statement is a variable declaration without braces
      if (stmt->data.if_statement.else_statement && stmt->data.if_statement.else_statement->type == AST_VARIABLE_DECL) {
        report_semantic_error(
            ctx, ERROR_SEM_DEPENDENT_STATEMENT_ASSIGNMENT, "variable declaration cannot be used as dependent statement",
            "use braces {} to create a compound statement", stmt->data.if_statement.else_statement->line,
            stmt->data.if_statement.else_statement->column);
        success = false;
      }

      // Analyze the then statement
      if (!analyze_statement(ctx, stmt->data.if_statement.then_statement)) {
        success = false;
      }

      // Analyze the else statement if present
      if (stmt->data.if_statement.else_statement && !analyze_statement(ctx, stmt->data.if_statement.else_statement)) {
        success = false;
      }

      return success;
    }

    case AST_DO_WHILE_STATEMENT:
    case AST_WHILE_STATEMENT: {
      bool success = true;

      // Analyze the condition
      if (!analyze_expression(ctx, stmt->data.while_statement.condition)) {
        success = false;
      }

      // Set loop context and analyze the body
      bool previous_in_loop = ctx->in_loop;
      ctx->in_loop = true;

      if (!analyze_statement(ctx, stmt->data.while_statement.body)) {
        success = false;
      }

      // Restore previous loop context
      ctx->in_loop = previous_in_loop;

      return success;
    }

    case AST_BREAK_STATEMENT: {
      // Check if we're inside a loop
      if (!ctx->in_loop) {
        report_semantic_error(ctx, ERROR_SEM_BREAK_OUTSIDE_LOOP, "break statement not within a loop",
                              "use break only inside while loops", stmt->line, stmt->column);
        return false;
      }

      return true;
    }

    case AST_CONTINUE_STATEMENT: {
      // Check if we're inside a loop
      if (!ctx->in_loop) {
        report_semantic_error(ctx, ERROR_SEM_CONTINUE_OUTSIDE_LOOP, "continue statement not within a loop",
                              "use continue only inside while loops", stmt->line, stmt->column);
        return false;
      }

      return true;
    }

    default:
      // Other statement types (like expression statements)
      return analyze_expression(ctx, stmt);
  }
}

// Main semantic analysis function
bool semantic_analyze(Arena* arena, ASTNode* ast, ErrorList* errors, const char* source) {
  if (!ast || ast->type != AST_PROGRAM) {
    return false;
  }

  // Create semantic context
  SemanticContext ctx = {0};
  ctx.arena = arena;
  ctx.errors = errors;
  ctx.current_scope = symbol_table_create(arena, NULL);
  ctx.in_loop = false;
  ctx.source = source;

  if (!ctx.current_scope) {
    return false;
  }

  // Analyze the function
  ASTNode* function_node = ast->data.program.function;
  if (!function_node || function_node->type != AST_FUNCTION) {
    return false;
  }

  bool success = true;

  // Analyze all statements in the function
  for (int i = 0; i < function_node->data.function.statement_count; i++) {
    if (!analyze_statement(&ctx, function_node->data.function.statements[i])) {
      success = false;
      // Continue analyzing other statements to collect all errors
    }
  }

  return success;
}
