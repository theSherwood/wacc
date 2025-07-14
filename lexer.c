#include "compiler.h"

Lexer* lexer_create(Arena* arena, const char* source) {
    Lexer* lexer = arena_alloc(arena, sizeof(Lexer));
    if (!lexer) return NULL;
    
    lexer->source = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    return lexer;
}

static void skip_whitespace(Lexer* lexer) {
    while (is_space(*lexer->current)) {
        if (*lexer->current == '\n') {
            lexer->line++;
            lexer->column = 1;
        } else {
            lexer->column++;
        }
        lexer->current++;
    }
}

static void skip_line_comment(Lexer* lexer) {
    // Skip the rest of the line after //
    while (*lexer->current != '\0' && *lexer->current != '\n') {
        lexer->current++;
        lexer->column++;
    }
}

static bool is_identifier_start(char c) {
    return is_alpha(c) || c == '_';
}

static bool is_identifier_char(char c) {
    return is_alnum(c) || c == '_';
}

static TokenType get_keyword_type(const char* start, size_t length) {
    if (length == 3 && str_ncmp(start, "int", 3) == 0) {
        return TOKEN_INT;
    }
    if (length == 6 && str_ncmp(start, "return", 6) == 0) {
        return TOKEN_RETURN;
    }
    return TOKEN_IDENTIFIER;
}

Token lexer_next_token(Lexer* lexer) {
    Token token;
    
    // Skip whitespace and comments
    while (1) {
        skip_whitespace(lexer);
        
        // Check for C99 line comment
        if (*lexer->current == '/' && *(lexer->current + 1) == '/') {
            lexer->current += 2;
            lexer->column += 2;
            skip_line_comment(lexer);
            continue;
        }
        
        break;
    }
    
    token.start = lexer->current;
    token.line = lexer->line;
    token.column = lexer->column;
    
    if (*lexer->current == '\0') {
        token.type = TOKEN_EOF;
        token.length = 0;
        return token;
    }
    
    char c = *lexer->current++;
    lexer->column++;
    
    switch (c) {
        case '(':
            token.type = TOKEN_OPEN_PAREN;
            token.length = 1;
            break;
        case ')':
            token.type = TOKEN_CLOSE_PAREN;
            token.length = 1;
            break;
        case '{':
            token.type = TOKEN_OPEN_BRACE;
            token.length = 1;
            break;
        case '}':
            token.type = TOKEN_CLOSE_BRACE;
            token.length = 1;
            break;
        case ';':
            token.type = TOKEN_SEMICOLON;
            token.length = 1;
            break;
        default:
            if (is_identifier_start(c)) {
                // Identifier or keyword
                while (is_identifier_char(*lexer->current)) {
                    lexer->current++;
                    lexer->column++;
                }
                token.length = lexer->current - token.start;
                token.type = get_keyword_type(token.start, token.length);
            } else if (is_digit(c)) {
                // Integer literal
                while (is_digit(*lexer->current)) {
                    lexer->current++;
                    lexer->column++;
                }
                token.type = TOKEN_INTEGER_LITERAL;
                token.length = lexer->current - token.start;
            } else {
                token.type = TOKEN_ERROR;
                token.length = 1;
            }
            break;
    }
    
    return token;
}
