#include "compiler.h"
#include <stdio.h>

ErrorList* error_list_create(Arena* arena) {
    ErrorList* errors = arena_alloc(arena, sizeof(ErrorList));
    if (!errors) return NULL;
    
    errors->capacity = 16;
    errors->errors = arena_alloc(arena, sizeof(CompilerError) * errors->capacity);
    if (!errors->errors) return NULL;
    
    errors->count = 0;
    errors->has_fatal_errors = false;
    
    return errors;
}

static void error_list_resize(ErrorList* errors, Arena* arena) {
    size_t new_capacity = errors->capacity * 2;
    CompilerError* new_errors = arena_alloc(arena, sizeof(CompilerError) * new_capacity);
    if (!new_errors) return;
    
    mem_cpy(new_errors, errors->errors, sizeof(CompilerError) * errors->count);
    errors->errors = new_errors;
    errors->capacity = new_capacity;
}

void error_list_add(ErrorList* errors, Arena* arena, int id, ErrorLevel level, 
                   SourceLocation location, const char* message, const char* suggestion, 
                   const char* context) {
    if (!errors) return;
    
    if (errors->count >= errors->capacity) {
        error_list_resize(errors, arena);
    }
    
    if (errors->count >= errors->capacity) {
        return; // Failed to resize
    }
    
    CompilerError* error = &errors->errors[errors->count++];
    error->id = id;
    error->level = level;
    error->location = location;
    error->message = message;
    error->suggestion = suggestion;
    error->context = context;
    
    if (level == ERROR_LEXICAL || level == ERROR_SYNTAX || level == ERROR_SEMANTIC || level == ERROR_CODEGEN) {
        errors->has_fatal_errors = true;
    }
}

void error_list_print(ErrorList* errors, const char* filename) {
    if (!errors) return;
    
    for (size_t i = 0; i < errors->count; i++) {
        CompilerError* error = &errors->errors[i];
        const char* level_str;
        
        switch (error->level) {
            case ERROR_LEXICAL: level_str = "error"; break;
            case ERROR_SYNTAX: level_str = "error"; break;
            case ERROR_SEMANTIC: level_str = "error"; break;
            case ERROR_CODEGEN: level_str = "error"; break;
            case WARNING: level_str = "warning"; break;
            default: level_str = "error"; break;
        }
        
        // Print error in format: filename.c:line:column: id: error_id error: message
        printf("%s:%d:%d: id: %d %s: %s\n", 
               filename, error->location.line, error->location.column,
               error->id, level_str, error->message);
        
        // Print source context if available
        if (error->context) {
            printf("   %s\n", error->context);
            
            // Print caret indicator
            printf("   ");
            for (int j = 0; j < error->location.column - 1; j++) {
                printf(" ");
            }
            printf("^\n");
        }
        
        // Print suggestion if available
        if (error->suggestion) {
            printf("note: %s\n", error->suggestion);
        }

        printf("\n");
    }
}

bool error_list_has_errors(ErrorList* errors) {
    return errors && errors->has_fatal_errors;
}

// Helper function to get source context line
const char* get_source_context(Arena* arena, const char* source, int line) {
    if (!source) return NULL;
    
    const char* current = source;
    int current_line = 1;
    
    // Find the start of the target line
    while (*current && current_line < line) {
        if (*current == '\n') {
            current_line++;
        }
        current++;
    }
    
    if (current_line != line) {
        return NULL; // Line not found
    }
    
    // Find the end of the line
    const char* line_start = current;
    while (*current && *current != '\n') {
        current++;
    }
    
    // Copy the line
    size_t length = current - line_start;
    char* context = arena_alloc(arena, length + 1);
    if (!context) return NULL;
    
    str_ncpy(context, line_start, length);
    context[length] = '\0';
    
    return context;
}
