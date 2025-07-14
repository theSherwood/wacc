#include "compiler.h"
#include <stdio.h>
#include <string.h>

static void emit_wasm_function(FILE* file, ASTNode* function_node) {
    fprintf(file, "  (func $%s (result i32)\n", function_node->data.function.name);
    
    // Generate code for the statement
    ASTNode* stmt = function_node->data.function.statement;
    if (stmt->type == AST_RETURN_STATEMENT) {
        ASTNode* expr = stmt->data.return_statement.expression;
        if (expr->type == AST_INTEGER_CONSTANT) {
            fprintf(file, "    i32.const %d\n", expr->data.integer_constant.value);
        }
    }
    
    fprintf(file, "  )\n");
}

void codegen_emit_wasm(Arena* arena, ASTNode* ast, const char* output_path) {
    (void)arena; // Unused parameter
    if (!ast || ast->type != AST_PROGRAM) {
        return;
    }
    
    FILE* file = fopen(output_path, "w");
    if (!file) {
        return;
    }
    
    // Write WASM module header
    fprintf(file, "(module\n");
    
    // Generate function
    ASTNode* function = ast->data.program.function;
    emit_wasm_function(file, function);
    
    // Export the function if it's main
    if (strcmp(function->data.function.name, "main") == 0) {
        fprintf(file, "  (export \"main\" (func $main))\n");
    }
    
    // Close module
    fprintf(file, ")\n");
    
    fclose(file);
}
