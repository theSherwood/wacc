#include <stdio.h>

#include "compiler.h"

#define MAX_STATEMENTS 256

Parser* parser_create(Arena* arena, Lexer* lexer, ErrorList* errors) {
  Parser* parser = arena_alloc(arena, sizeof(Parser));
  if (!parser) return NULL;

  parser->lexer = lexer;
  parser->arena = arena;
  parser->errors = errors;
  parser->current_token = lexer_next_token(lexer);
  return parser;
}

static void advance_token(Parser* parser) {
  parser->current_token = lexer_next_token(parser->lexer);
}

static void report_error(Parser* parser, int error_id, const char* message, const char* suggestion) {
  if (!parser->errors) return;

  SourceLocation location = {
      .filename = parser->lexer->filename,
      .line = parser->current_token.line,
      .column = parser->current_token.column,
      .start_pos = parser->current_token.start - parser->lexer->source,
      .end_pos = parser->current_token.start - parser->lexer->source + parser->current_token.length};

  error_list_add(parser->errors, NULL, error_id, ERROR_SYNTAX, location, message, suggestion, NULL);
}

static void synchronize(Parser* parser) {
  // Skip tokens until we reach a synchronization point
  while (parser->current_token.type != TOKEN_EOF) {
    if (parser->current_token.type == TOKEN_SEMICOLON || parser->current_token.type == TOKEN_OPEN_BRACE ||
        parser->current_token.type == TOKEN_CLOSE_BRACE) {
      return;
    }
    advance_token(parser);
  }
}

static bool match_token(Parser* parser, TokenType type) {
  if (parser->current_token.type == type) {
    advance_token(parser);
    return true;
  }
  return false;
}

static ASTNode* create_ast_node(Parser* parser, ASTNodeType type) {
  ASTNode* node = arena_alloc(parser->arena, sizeof(ASTNode));
  if (!node) return NULL;
  node->type = type;
  node->line = parser->current_token.line;
  node->column = parser->current_token.column;
  return node;
}

// Forward declarations
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_assignment_expression(Parser* parser);
static ASTNode* parse_ternary_expression(Parser* parser);
static ASTNode* parse_logical_or(Parser* parser);
static ASTNode* parse_logical_and(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_relational(Parser* parser);
static ASTNode* parse_additive(Parser* parser);
static ASTNode* parse_multiplicative(Parser* parser);
static ASTNode* parse_unary(Parser* parser);
static ASTNode* parse_primary(Parser* parser);
static ASTNode* parse_statement(Parser* parser);
static ASTNode* parse_if_statement(Parser* parser);
static ASTNode* parse_compound_statement(Parser* parser);

static ASTNode* parse_primary(Parser* parser) {
  if (parser->current_token.type == TOKEN_INTEGER_LITERAL) {
    ASTNode* node = create_ast_node(parser, AST_INTEGER_CONSTANT);
    if (!node) return NULL;

    // Parse the integer value
    char* endptr;
    node->data.integer_constant.value = str_to_long(parser->current_token.start, &endptr, 10);

    advance_token(parser);

    if (parser->current_token.type == TOKEN_OPEN_PAREN) {
        report_error(parser, 3006, "missing operator before parenthesis", "insert an operator like `+` or `*`");
        advance_token(parser); // Advance past the problematic token
        return NULL;
    }

    return node;
  }

    if (parser->current_token.type == TOKEN_IDENTIFIER) {
        // Treat identifiers as variable references
        ASTNode* node = create_ast_node(parser, AST_VARIABLE_REF);
        if (!node) return NULL;

        // Copy identifier name
        size_t name_len = parser->current_token.length;
        char* name = arena_alloc(parser->arena, name_len + 1);
        if (!name) return NULL;
        str_ncpy(name, parser->current_token.start, name_len);
        name[name_len] = '\0';
        node->data.variable_ref.name = name;

        advance_token(parser);
        return node;
    }

  // Handle parentheses
  if (match_token(parser, TOKEN_OPEN_PAREN)) {
    ASTNode* expr = parse_expression(parser);
    if (!expr) return NULL;

    if (!match_token(parser, TOKEN_CLOSE_PAREN)) {
      report_error(parser, ERROR_SYNTAX_MISSING_PAREN, "expected ')'", "add closing parenthesis");
      return NULL;
    }

    return expr;
  }

  // Error: expected primary expression
  report_error(parser, ERROR_SYNTAX_EXPECTED_EXPRESSION, "expected expression",
               "add an integer literal or parenthesized expression");
  synchronize(parser);
  return NULL;
}

static ASTNode* parse_unary(Parser* parser) {
  // Handle unary operators
  if (parser->current_token.type == TOKEN_BANG || parser->current_token.type == TOKEN_TILDE ||
      parser->current_token.type == TOKEN_MINUS) {
    TokenType op = parser->current_token.type;
    advance_token(parser);

    ASTNode* operand = parse_unary(parser);  // Right-associative
    if (!operand) return NULL;

    ASTNode* node = create_ast_node(parser, AST_UNARY_OP);
    if (!node) return NULL;

    node->data.unary_op.operator = op;
    node->data.unary_op.operand = operand;

    return node;
  }

  return parse_primary(parser);
}

static ASTNode* parse_multiplicative(Parser* parser) {
  ASTNode* left = parse_unary(parser);
  if (!left) return NULL;

  while (parser->current_token.type == TOKEN_STAR || parser->current_token.type == TOKEN_SLASH ||
         parser->current_token.type == TOKEN_PERCENT) {
    TokenType op = parser->current_token.type;
    advance_token(parser);

    ASTNode* right = parse_unary(parser);
    if (!right) return NULL;

    ASTNode* node = create_ast_node(parser, AST_BINARY_OP);
    if (!node) return NULL;

    node->data.binary_op.operator = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;

    left = node;
  }

  return left;
}

static ASTNode* parse_additive(Parser* parser) {
  ASTNode* left = parse_multiplicative(parser);
  if (!left) return NULL;

  while (parser->current_token.type == TOKEN_PLUS || parser->current_token.type == TOKEN_MINUS) {
    TokenType op = parser->current_token.type;
    advance_token(parser);

    ASTNode* right = parse_multiplicative(parser);
    if (!right) return NULL;

    ASTNode* node = create_ast_node(parser, AST_BINARY_OP);
    if (!node) return NULL;

    node->data.binary_op.operator = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;

    left = node;
  }

  return left;
}

static ASTNode* parse_relational(Parser* parser) {
  ASTNode* left = parse_additive(parser);
  if (!left) return NULL;

  while (parser->current_token.type == TOKEN_LT || parser->current_token.type == TOKEN_GT ||
         parser->current_token.type == TOKEN_LT_EQ || parser->current_token.type == TOKEN_GT_EQ) {
    TokenType op = parser->current_token.type;
    advance_token(parser);

    ASTNode* right = parse_additive(parser);
    if (!right) return NULL;

    ASTNode* node = create_ast_node(parser, AST_BINARY_OP);
    if (!node) return NULL;

    node->data.binary_op.operator = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;

    left = node;
  }

  return left;
}

static ASTNode* parse_equality(Parser* parser) {
  ASTNode* left = parse_relational(parser);
  if (!left) return NULL;

  while (parser->current_token.type == TOKEN_EQ_EQ || parser->current_token.type == TOKEN_BANG_EQ) {
    TokenType op = parser->current_token.type;
    advance_token(parser);

    ASTNode* right = parse_relational(parser);
    if (!right) return NULL;

    ASTNode* node = create_ast_node(parser, AST_BINARY_OP);
    if (!node) return NULL;

    node->data.binary_op.operator = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;

    left = node;
  }

  return left;
}

static ASTNode* parse_logical_and(Parser* parser) {
  ASTNode* left = parse_equality(parser);
  if (!left) return NULL;

  while (parser->current_token.type == TOKEN_AMP_AMP) {
    TokenType op = parser->current_token.type;
    advance_token(parser);

    ASTNode* right = parse_equality(parser);
    if (!right) return NULL;

    ASTNode* node = create_ast_node(parser, AST_BINARY_OP);
    if (!node) return NULL;

    node->data.binary_op.operator = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;

    left = node;
  }

  return left;
}

static ASTNode* parse_logical_or(Parser* parser) {
  ASTNode* left = parse_logical_and(parser);
  if (!left) return NULL;

  while (parser->current_token.type == TOKEN_PIPE_PIPE) {
    TokenType op = parser->current_token.type;
    advance_token(parser);

    ASTNode* right = parse_logical_and(parser);
    if (!right) return NULL;

    ASTNode* node = create_ast_node(parser, AST_BINARY_OP);
    if (!node) return NULL;

    node->data.binary_op.operator = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;

    left = node;
  }

  return left;
}

static ASTNode* parse_ternary_expression(Parser* parser) {
    ASTNode* condition = parse_logical_or(parser);
    if (!condition) return NULL;

    if (match_token(parser, TOKEN_QUESTION)) {
        ASTNode* true_expr = parse_expression(parser);
        if (!true_expr) return NULL;

        if (!match_token(parser, TOKEN_COLON)) {
            report_error(parser, ERROR_SYNTAX_EXPECTED_TOKEN, "expected ':' in ternary expression", "add ':'");
            return NULL;
        }

        ASTNode* false_expr = parse_ternary_expression(parser); // Right-associative
        if (!false_expr) return NULL;

        ASTNode* node = create_ast_node(parser, AST_TERNARY_EXPRESSION);
        if (!node) return NULL;

        node->data.ternary_expression.condition = condition;
        node->data.ternary_expression.true_expression = true_expr;
        node->data.ternary_expression.false_expression = false_expr;

        return node;
    }

    return condition;
}

static ASTNode* parse_assignment_expression(Parser* parser) {
    ASTNode* left = parse_ternary_expression(parser);

    if (match_token(parser, TOKEN_EQ)) {
        ASTNode* right = parse_assignment_expression(parser); // Right-associative
        if (!right) return NULL;

        // Check if the left side is a valid assignment target (l-value)
        if (left->type != AST_VARIABLE_REF) {
            report_error(parser, ERROR_SEM_INVALID_ASSIGNMENT, "invalid assignment target", "target must be a variable");
            return NULL;
        }

        ASTNode* node = create_ast_node(parser, AST_ASSIGNMENT);
        if (!node) return NULL;

        node->data.assignment.name = left->data.variable_ref.name;
        node->data.assignment.value = right;
        return node;
    }

    return left;
}

static ASTNode* parse_expression(Parser* parser) {
  return parse_assignment_expression(parser);
}

static ASTNode* parse_declaration(Parser* parser) {
    if (!match_token(parser, TOKEN_INT)) {
        report_error(parser, ERROR_SYNTAX_EXPECTED_TOKEN, "expected 'int' type specifier", "add 'int'");
        return NULL;
    }

    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        report_error(parser, ERROR_SYNTAX_EXPECTED_TOKEN, "expected identifier after type", "add a variable name");
        return NULL;
    }

    ASTNode* node = create_ast_node(parser, AST_VARIABLE_DECL);
    if (!node) return NULL;

    // Copy identifier name
    size_t name_len = parser->current_token.length;
    char* name = arena_alloc(parser->arena, name_len + 1);
    if (!name) return NULL;
    str_ncpy(name, parser->current_token.start, name_len);
    name[name_len] = '\0';
    node->data.variable_decl.name = name;

    advance_token(parser);

    // Check for optional initializer
    if (match_token(parser, TOKEN_EQ)) {
        node->data.variable_decl.initializer = parse_expression(parser);
        if (!node->data.variable_decl.initializer) return NULL;
    } else {
        node->data.variable_decl.initializer = NULL;
    }

    if (!match_token(parser, TOKEN_SEMICOLON)) {
        report_error(parser, ERROR_SYNTAX_MISSING_SEMICOLON, "expected ';' after declaration", "add a semicolon");
        return NULL;
    }

    return node;
}


static ASTNode* parse_if_statement(Parser* parser) {
    if (!match_token(parser, TOKEN_IF)) {
        report_error(parser, ERROR_SYNTAX_EXPECTED_TOKEN, "expected 'if'", "add 'if' keyword");
        return NULL;
    }

    if (!match_token(parser, TOKEN_OPEN_PAREN)) {
        report_error(parser, ERROR_SYNTAX_MISSING_PAREN, "expected '(' after 'if'", "add '('");
        return NULL;
    }

    ASTNode* condition = parse_expression(parser);
    if (!condition) return NULL;

    if (!match_token(parser, TOKEN_CLOSE_PAREN)) {
        report_error(parser, ERROR_SYNTAX_MISSING_PAREN, "expected ')' after if condition", "add ')'");
        return NULL;
    }

    ASTNode* then_statement = parse_statement(parser);
    if (!then_statement) return NULL;

    ASTNode* else_statement = NULL;
    if (match_token(parser, TOKEN_ELSE)) {
        else_statement = parse_statement(parser);
        if (!else_statement) return NULL;
    }

    ASTNode* node = create_ast_node(parser, AST_IF_STATEMENT);
    if (!node) return NULL;

    node->data.if_statement.condition = condition;
    node->data.if_statement.then_statement = then_statement;
    node->data.if_statement.else_statement = else_statement;

    return node;
}

static ASTNode* parse_while_statement(Parser* parser) {
    if (!match_token(parser, TOKEN_WHILE)) {
        report_error(parser, ERROR_SYNTAX_EXPECTED_TOKEN, "expected 'while'", "add 'while' keyword");
        return NULL;
    }

    if (!match_token(parser, TOKEN_OPEN_PAREN)) {
        report_error(parser, ERROR_SYNTAX_MISSING_PAREN, "expected '(' after 'while'", "add '('");
        return NULL;
    }

    ASTNode* condition = parse_expression(parser);
    if (!condition) return NULL;

    if (!match_token(parser, TOKEN_CLOSE_PAREN)) {
        report_error(parser, ERROR_SYNTAX_MISSING_PAREN, "expected ')' after while condition", "add ')'");
        return NULL;
    }

    ASTNode* body = parse_statement(parser);
    if (!body) return NULL;

    ASTNode* node = create_ast_node(parser, AST_WHILE_STATEMENT);
    if (!node) return NULL;

    node->data.while_statement.condition = condition;
    node->data.while_statement.body = body;

    return node;
}

static ASTNode* parse_compound_statement(Parser* parser) {
    if (!match_token(parser, TOKEN_OPEN_BRACE)) {
        report_error(parser, ERROR_SYNTAX_MISSING_BRACE, "expected '{'", "add opening brace");
        return NULL;
    }

    ASTNode* node = create_ast_node(parser, AST_COMPOUND_STATEMENT);
    if (!node) return NULL;

    // Allocate space for statements
    node->data.compound_statement.statements = arena_alloc(parser->arena, sizeof(ASTNode*) * MAX_STATEMENTS);
    node->data.compound_statement.statement_count = 0;

    // Parse statements until we hit the closing brace
    while (parser->current_token.type != TOKEN_CLOSE_BRACE && parser->current_token.type != TOKEN_EOF) {
        Token prev_token = parser->current_token;  // Save current position
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            if (node->data.compound_statement.statement_count >= MAX_STATEMENTS) {
                report_error(parser, ERROR_SYNTAX_UNEXPECTED_TOKEN, "too many statements in block", "reduce number of statements");
                return NULL;
            }
            node->data.compound_statement.statements[node->data.compound_statement.statement_count++] = stmt;
        } else {
            // Error in parsing statement, synchronize and continue
            synchronize(parser);
            // If we haven't advanced, force advance to avoid infinite loop
            if (parser->current_token.start == prev_token.start) {
                advance_token(parser);
            }
        }
    }

    if (!match_token(parser, TOKEN_CLOSE_BRACE)) {
        report_error(parser, ERROR_SYNTAX_MISSING_BRACE, "expected '}'", "add closing brace");
        return NULL;
    }

    return node;
}

static ASTNode* parse_statement(Parser* parser) {
  if (parser->current_token.type == TOKEN_INT) {
        return parse_declaration(parser);
  }

  if (parser->current_token.type == TOKEN_IF) {
  return parse_if_statement(parser);
  }

  if (parser->current_token.type == TOKEN_WHILE) {
  return parse_while_statement(parser);
  }

   if (parser->current_token.type == TOKEN_OPEN_BRACE) {
        return parse_compound_statement(parser);
   }

  if (match_token(parser, TOKEN_RETURN)) {
    ASTNode* node = create_ast_node(parser, AST_RETURN_STATEMENT);
    if (!node) return NULL;

    node->data.return_statement.expression = parse_expression(parser);
    if (!node->data.return_statement.expression) {
      // Error already reported by parse_expression
      return NULL;
    }

    if (!match_token(parser, TOKEN_SEMICOLON)) {
      report_error(parser, ERROR_SYNTAX_MISSING_SEMICOLON, "expected ';'", "add a semicolon");
      synchronize(parser);
      return NULL;
    }

    return node;
  }
  
    if (parser->current_token.type == TOKEN_IDENTIFIER && 
        parser->current_token.length == 7 && 
        str_ncmp(parser->current_token.start, "return0", 7) == 0) {
        report_error(parser, ERROR_SYNTAX_UNEXPECTED_TOKEN, "unexpected identifier", "did you mean 'return 0'?");
        synchronize(parser);
        return NULL;
    }
   // Fallback to expression statement (for assignments)
    ASTNode* expr = parse_expression(parser);
    if (!expr) return NULL;

    if (!match_token(parser, TOKEN_SEMICOLON)) {
        report_error(parser, ERROR_SYNTAX_MISSING_SEMICOLON, "expected ';' after expression", "add a semicolon");
        return NULL;
    }

    return expr;
}

static ASTNode* parse_function(Parser* parser) {
  if (!match_token(parser, TOKEN_INT)) {
    report_error(parser, ERROR_SYNTAX_EXPECTED_TOKEN, "expected 'int'", "add 'int' keyword");
    synchronize(parser);
    return NULL;
  }

  if (parser->current_token.type != TOKEN_IDENTIFIER) {
    report_error(parser, ERROR_SYNTAX_EXPECTED_TOKEN, "expected function name", "add a function name");
    synchronize(parser);
    return NULL;
  }

  ASTNode* node = create_ast_node(parser, AST_FUNCTION);
  if (!node) return NULL;

  // Copy function name
  size_t name_len = parser->current_token.length;
  char* name = arena_alloc(parser->arena, name_len + 1);
  if (!name) return NULL;
  str_ncpy(name, parser->current_token.start, name_len);
  name[name_len] = '\0';
  node->data.function.name = name;

  advance_token(parser);

  if (!match_token(parser, TOKEN_OPEN_PAREN)) {
    report_error(parser, ERROR_SYNTAX_MISSING_PAREN, "expected '('", "add opening parenthesis");
    synchronize(parser);
    return NULL;
  }

  if (!match_token(parser, TOKEN_CLOSE_PAREN)) {
    report_error(parser, ERROR_SYNTAX_MISSING_PAREN, "expected ')'", "add closing parenthesis");
    synchronize(parser);
    return NULL;
  }

  if (!match_token(parser, TOKEN_OPEN_BRACE)) {
    report_error(parser, ERROR_SYNTAX_MISSING_BRACE, "expected '{'", "add opening brace");
    synchronize(parser);
    return NULL;
  }

  // Parse statements
    node->data.function.statements = arena_alloc(parser->arena, sizeof(ASTNode*) * MAX_STATEMENTS);
    node->data.function.statement_count = 0;
    while (parser->current_token.type != TOKEN_CLOSE_BRACE && parser->current_token.type != TOKEN_EOF) {
        Token prev_token = parser->current_token;  // Save current position
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            node->data.function.statements[node->data.function.statement_count++] = stmt;
        } else {
            // Error in parsing statement, synchronize and continue
            synchronize(parser);
            // If we haven't advanced, force advance to avoid infinite loop
            if (parser->current_token.start == prev_token.start) {
                advance_token(parser);
            }
        }
    }

  if (!match_token(parser, TOKEN_CLOSE_BRACE)) {
    report_error(parser, ERROR_SYNTAX_MISSING_BRACE, "expected '}'", "add closing brace");
    synchronize(parser);
    return NULL;
  }

  return node;
}

ASTNode* parser_parse_program(Parser* parser) {
  ASTNode* node = create_ast_node(parser, AST_PROGRAM);
  if (!node) return NULL;

  node->data.program.function = parse_function(parser);
  if (!node->data.program.function) {
    // Error already reported by parse_function
    return NULL;
  }

  if (parser->current_token.type != TOKEN_EOF) {
    report_error(parser, ERROR_SYNTAX_UNEXPECTED_TOKEN, "expected end of file", "remove extra tokens");
    return NULL;
  }

  return node;
}

static const char* token_type_to_string(TokenType type) {
  switch (type) {
    case TOKEN_BANG:
      return "!";
    case TOKEN_TILDE:
      return "~";
    case TOKEN_MINUS:
      return "-";
    case TOKEN_PLUS:
      return "+";
    case TOKEN_STAR:
      return "*";
    case TOKEN_SLASH:
      return "/";
    case TOKEN_PERCENT:
      return "%";
    case TOKEN_EQ_EQ:
      return "==";
    case TOKEN_BANG_EQ:
      return "!=";
    case TOKEN_LT:
      return "<";
    case TOKEN_GT:
      return ">";
    case TOKEN_LT_EQ:
      return "<=";
    case TOKEN_GT_EQ:
      return ">=";
    case TOKEN_AMP_AMP:
      return "&&";
    case TOKEN_PIPE_PIPE:
      return "||";
    default:
      return "unknown";
  }
}

static void ast_print_indent(int depth) {
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }
}

static void ast_print_node(ASTNode* node, int depth) {
  if (!node) {
    ast_print_indent(depth);
    printf("(null)\n");
    return;
  }

  ast_print_indent(depth);

  switch (node->type) {
    case AST_PROGRAM:
      printf("Program\n");
      ast_print_node(node->data.program.function, depth + 1);
      break;

    case AST_FUNCTION:
      printf("Function: %s\n", node->data.function.name);
      for (int i = 0; i < node->data.function.statement_count; i++) {
        ast_print_node(node->data.function.statements[i], depth + 1);
      }
      break;

    case AST_RETURN_STATEMENT:
      printf("Return\n");
      ast_print_node(node->data.return_statement.expression, depth + 1);
      break;

    case AST_INTEGER_CONSTANT:
      printf("Integer: %d\n", node->data.integer_constant.value);
      break;

    case AST_UNARY_OP:
      printf("Unary: %s\n", token_type_to_string(node->data.unary_op.operator));
      ast_print_node(node->data.unary_op.operand, depth + 1);
      break;

    case AST_BINARY_OP:
      printf("Binary: %s\n", token_type_to_string(node->data.binary_op.operator));
      ast_print_node(node->data.binary_op.left, depth + 1);
      ast_print_node(node->data.binary_op.right, depth + 1);
      break;

    case AST_VARIABLE_DECL:
            printf("Variable Declaration: %s\n", node->data.variable_decl.name);
            if (node->data.variable_decl.initializer) {
                ast_print_node(node->data.variable_decl.initializer, depth + 1);
            }
            break;

    case AST_VARIABLE_REF:
        printf("Variable Reference: %s\n", node->data.variable_ref.name);
        break;

    case AST_ASSIGNMENT:
        printf("Assignment: %s\n", node->data.assignment.name);
        ast_print_node(node->data.assignment.value, depth + 1);
        break;

    case AST_IF_STATEMENT:
        printf("If Statement\n");
        ast_print_indent(depth + 1);
        printf("Condition:\n");
        ast_print_node(node->data.if_statement.condition, depth + 2);
        ast_print_indent(depth + 1);
        printf("Then:\n");
        ast_print_node(node->data.if_statement.then_statement, depth + 2);
        if (node->data.if_statement.else_statement) {
            ast_print_indent(depth + 1);
            printf("Else:\n");
            ast_print_node(node->data.if_statement.else_statement, depth + 2);
        }
        break;

    case AST_TERNARY_EXPRESSION:
        printf("Ternary Expression\n");
        ast_print_indent(depth + 1);
        printf("Condition:\n");
        ast_print_node(node->data.ternary_expression.condition, depth + 2);
        ast_print_indent(depth + 1);
        printf("True:\n");
        ast_print_node(node->data.ternary_expression.true_expression, depth + 2);
        ast_print_indent(depth + 1);
        printf("False:\n");
        ast_print_node(node->data.ternary_expression.false_expression, depth + 2);
        break;

    case AST_COMPOUND_STATEMENT:
        printf("Compound Statement\n");
        for (int i = 0; i < node->data.compound_statement.statement_count; i++) {
            ast_print_node(node->data.compound_statement.statements[i], depth + 1);
        }
        break;

    default:
      printf("Unknown node type\n");
      break;
  }
}

void ast_print(ASTNode* ast) {
  printf("=== AST ===\n");
  ast_print_node(ast, 0);
  printf("===========\n");
}
