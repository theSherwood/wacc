#include "compiler.h"
#include <string.h>
#include <stdlib.h>

Parser* parser_create(Arena* arena, Lexer* lexer) {
    Parser* parser = arena_alloc(arena, sizeof(Parser));
    if (!parser) return NULL;
    
    parser->lexer = lexer;
    parser->arena = arena;
    parser->current_token = lexer_next_token(lexer);
    return parser;
}

static void advance_token(Parser* parser) {
    parser->current_token = lexer_next_token(parser->lexer);
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

static ASTNode* parse_expression(Parser* parser) {
    // For now, only handle integer constants
    if (parser->current_token.type == TOKEN_INTEGER_LITERAL) {
        ASTNode* node = create_ast_node(parser, AST_INTEGER_CONSTANT);
        if (!node) return NULL;
        
        // Parse the integer value
        char* endptr;
        node->data.integer_constant.value = strtol(parser->current_token.start, &endptr, 10);
        
        advance_token(parser);
        return node;
    }
    
    return NULL; // Error: expected expression
}

static ASTNode* parse_statement(Parser* parser) {
    if (match_token(parser, TOKEN_RETURN)) {
        ASTNode* node = create_ast_node(parser, AST_RETURN_STATEMENT);
        if (!node) return NULL;
        
        node->data.return_statement.expression = parse_expression(parser);
        if (!node->data.return_statement.expression) {
            return NULL; // Error: expected expression
        }
        
        if (!match_token(parser, TOKEN_SEMICOLON)) {
            return NULL; // Error: expected semicolon
        }
        
        return node;
    }
    
    return NULL; // Error: expected statement
}

static ASTNode* parse_function(Parser* parser) {
    if (!match_token(parser, TOKEN_INT)) {
        return NULL; // Error: expected 'int'
    }
    
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        return NULL; // Error: expected function name
    }
    
    ASTNode* node = create_ast_node(parser, AST_FUNCTION);
    if (!node) return NULL;
    
    // Copy function name
    size_t name_len = parser->current_token.length;
    char* name = arena_alloc(parser->arena, name_len + 1);
    if (!name) return NULL;
    strncpy(name, parser->current_token.start, name_len);
    name[name_len] = '\0';
    node->data.function.name = name;
    
    advance_token(parser);
    
    if (!match_token(parser, TOKEN_OPEN_PAREN)) {
        return NULL; // Error: expected '('
    }
    
    if (!match_token(parser, TOKEN_CLOSE_PAREN)) {
        return NULL; // Error: expected ')'
    }
    
    if (!match_token(parser, TOKEN_OPEN_BRACE)) {
        return NULL; // Error: expected '{'
    }
    
    node->data.function.statement = parse_statement(parser);
    if (!node->data.function.statement) {
        return NULL; // Error: expected statement
    }
    
    if (!match_token(parser, TOKEN_CLOSE_BRACE)) {
        return NULL; // Error: expected '}'
    }
    
    return node;
}

ASTNode* parser_parse_program(Parser* parser) {
    ASTNode* node = create_ast_node(parser, AST_PROGRAM);
    if (!node) return NULL;
    
    node->data.program.function = parse_function(parser);
    if (!node->data.program.function) {
        return NULL; // Error: expected function
    }
    
    if (parser->current_token.type != TOKEN_EOF) {
        return NULL; // Error: expected end of file
    }
    
    return node;
}
