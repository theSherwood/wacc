#include "compiler.h"
#include <stdio.h>
#include <string.h>

static void ir_emit_instruction(IRFunction* func, Arena* arena, IROpcode opcode, Type result_type, Operand* operands, int operand_count, int result_reg) {
    if (func->instruction_count >= func->capacity) {
        func->capacity = func->capacity ? func->capacity * 2 : 16;
        Instruction* new_instructions = arena_alloc(arena, func->capacity * sizeof(Instruction));
        if (func->instructions) {
            memcpy(new_instructions, func->instructions, func->instruction_count * sizeof(Instruction));
        }
        func->instructions = new_instructions;
    }
    
    Instruction* inst = &func->instructions[func->instruction_count++];
    inst->opcode = opcode;
    inst->result_type = result_type;
    inst->operand_count = operand_count;
    inst->result_reg = result_reg;
    
    for (int i = 0; i < operand_count; i++) {
        inst->operands[i] = operands[i];
    }
}

static Type get_i32_type() {
    Type type;
    type.kind = TYPE_I32;
    type.wasm_type = WASM_I32;
    type.size = 4;
    type.alignment = 4;
    return type;
}

static void ir_generate_expression(IRFunction* func, Arena* arena, ASTNode* expr, int* reg_counter) {
    switch (expr->type) {
        case AST_INTEGER_CONSTANT: {
            Type i32_type = get_i32_type();
            Operand operand;
            operand.type = OPERAND_CONSTANT;
            operand.value_type = i32_type;
            operand.value.constant.int_value = expr->data.integer_constant.value;
            
            ir_emit_instruction(func, arena, IR_CONST_INT, i32_type, &operand, 1, (*reg_counter)++);
            break;
        }
        default:
            // Handle other expression types later
            break;
    }
}

static void ir_generate_statement(IRFunction* func, Arena* arena, ASTNode* stmt, int* reg_counter) {
    switch (stmt->type) {
        case AST_RETURN_STATEMENT: {
            // Generate code for the return expression
            ir_generate_expression(func, arena, stmt->data.return_statement.expression, reg_counter);
            
            // Generate return instruction
            Type i32_type = get_i32_type();
            Operand operand;
            operand.type = OPERAND_REGISTER;
            operand.value_type = i32_type;
            operand.value.reg = (*reg_counter) - 1; // Use the last generated register
            
            ir_emit_instruction(func, arena, IR_RETURN, i32_type, &operand, 1, -1);
            break;
        }
        default:
            // Handle other statement types later
            break;
    }
}

IRModule* ir_generate(Arena* arena, ASTNode* ast) {
    if (!ast || ast->type != AST_PROGRAM) {
        return NULL;
    }
    
    IRModule* module = arena_alloc(arena, sizeof(IRModule));
    module->function_count = 1;
    module->functions = arena_alloc(arena, sizeof(IRFunction));
    
    IRFunction* func = &module->functions[0];
    func->instructions = NULL;
    func->instruction_count = 0;
    func->capacity = 0;
    
    int reg_counter = 0;
    
    // Generate IR for the function
    ASTNode* function_node = ast->data.program.function;
    ir_generate_statement(func, arena, function_node->data.function.statement, &reg_counter);
    
    return module;
}
