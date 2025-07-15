#include "compiler.h"

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
        .end_pos = parser->current_token.start - parser->lexer->source + parser->current_token.length
    };
    
    error_list_add(parser->errors, NULL, error_id, ERROR_SYNTAX, location, message, suggestion, NULL);
}

static void synchronize(Parser* parser) {
    // Skip tokens until we reach a synchronization point
    while (parser->current_token.type != TOKEN_EOF) {
        if (parser->current_token.type == TOKEN_SEMICOLON ||
            parser->current_token.type == TOKEN_OPEN_BRACE ||
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
    return node;
}

// Forward declarations
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_logical_or(Parser* parser);
static ASTNode* parse_logical_and(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_relational(Parser* parser);
static ASTNode* parse_additive(Parser* parser);
static ASTNode* parse_multiplicative(Parser* parser);
static ASTNode* parse_unary(Parser* parser);
static ASTNode* parse_primary(Parser* parser);

static ASTNode* parse_primary(Parser* parser) {
    if (parser->current_token.type == TOKEN_INTEGER_LITERAL) {
        ASTNode* node = create_ast_node(parser, AST_INTEGER_CONSTANT);
        if (!node) return NULL;
        
        // Parse the integer value
        char* endptr;
        node->data.integer_constant.value = str_to_long(parser->current_token.start, &endptr, 10);
        
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
    report_error(parser, ERROR_SYNTAX_EXPECTED_EXPRESSION, "expected expression", "add an integer literal or parenthesized expression");
    synchronize(parser);
    return NULL;
}

static ASTNode* parse_unary(Parser* parser) {
    // Handle unary operators
    if (parser->current_token.type == TOKEN_BANG ||
        parser->current_token.type == TOKEN_TILDE ||
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
    
    while (parser->current_token.type == TOKEN_STAR ||
           parser->current_token.type == TOKEN_SLASH ||
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
    
    while (parser->current_token.type == TOKEN_PLUS ||
           parser->current_token.type == TOKEN_MINUS) {
        
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
    
    while (parser->current_token.type == TOKEN_LT ||
           parser->current_token.type == TOKEN_GT ||
           parser->current_token.type == TOKEN_LT_EQ ||
           parser->current_token.type == TOKEN_GT_EQ) {
        
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
    
    while (parser->current_token.type == TOKEN_EQ_EQ ||
           parser->current_token.type == TOKEN_BANG_EQ) {
        
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

static ASTNode* parse_expression(Parser* parser) {
    return parse_logical_or(parser);
}

static ASTNode* parse_statement(Parser* parser) {
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
    
    // Error: expected statement
    report_error(parser, ERROR_SYNTAX_EXPECTED_STATEMENT, "expected statement", "add a return statement");
    synchronize(parser);
    return NULL;
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
    
    node->data.function.statement = parse_statement(parser);
    if (!node->data.function.statement) {
        // Error already reported by parse_statement
        return NULL;
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
