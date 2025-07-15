#include "compiler.h"
#include <stdio.h>

static void ir_emit_instruction(IRFunction* func, Arena* arena, IROpcode opcode, Type result_type, Operand* operands, int operand_count, int result_reg) {
    if (func->instruction_count >= func->capacity) {
        func->capacity = func->capacity ? func->capacity * 2 : 16;
        Instruction* new_instructions = arena_alloc(arena, func->capacity * sizeof(Instruction));
        if (func->instructions) {
            mem_cpy(new_instructions, func->instructions, func->instruction_count * sizeof(Instruction));
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
        case AST_UNARY_OP: {
            // Generate IR for the operand first
            ir_generate_expression(func, arena, expr->data.unary_op.operand, reg_counter);
            
            // Get the result register of the operand
            int operand_reg = (*reg_counter) - 1;
            
            Type i32_type = get_i32_type();
            Operand operand;
            operand.type = OPERAND_REGISTER;
            operand.value_type = i32_type;
            operand.value.reg = operand_reg;
            
            // Choose the appropriate IR opcode based on the operator
            IROpcode opcode;
            switch (expr->data.unary_op.operator) {
                case TOKEN_MINUS:
                    opcode = IR_NEG;
                    break;
                case TOKEN_BANG:
                    opcode = IR_NOT;
                    break;
                case TOKEN_TILDE:
                    opcode = IR_BITWISE_NOT;
                    break;
                default:
                    // Should not happen
                    return;
            }
            
            ir_emit_instruction(func, arena, opcode, i32_type, &operand, 1, (*reg_counter)++);
            break;
        }
        case AST_BINARY_OP: {
            // Generate IR for left operand
            ir_generate_expression(func, arena, expr->data.binary_op.left, reg_counter);
            int left_reg = (*reg_counter) - 1;
            
            // Generate IR for right operand
            ir_generate_expression(func, arena, expr->data.binary_op.right, reg_counter);
            int right_reg = (*reg_counter) - 1;
            
            Type i32_type = get_i32_type();
            Operand operands[2];
            
            // Left operand
            operands[0].type = OPERAND_REGISTER;
            operands[0].value_type = i32_type;
            operands[0].value.reg = left_reg;
            
            // Right operand
            operands[1].type = OPERAND_REGISTER;
            operands[1].value_type = i32_type;
            operands[1].value.reg = right_reg;
            
            // Choose the appropriate IR opcode based on the operator
            IROpcode opcode;
            switch (expr->data.binary_op.operator) {
                case TOKEN_PLUS:
                    opcode = IR_ADD;
                    break;
                case TOKEN_MINUS:
                    opcode = IR_SUB;
                    break;
                case TOKEN_STAR:
                    opcode = IR_MUL;
                    break;
                case TOKEN_SLASH:
                    opcode = IR_DIV;
                    break;
                case TOKEN_PERCENT:
                    opcode = IR_MOD;
                    break;
                case TOKEN_EQ_EQ:
                    opcode = IR_EQ;
                    break;
                case TOKEN_BANG_EQ:
                    opcode = IR_NE;
                    break;
                case TOKEN_LT:
                    opcode = IR_LT;
                    break;
                case TOKEN_GT:
                    opcode = IR_GT;
                    break;
                case TOKEN_LT_EQ:
                    opcode = IR_LE;
                    break;
                case TOKEN_GT_EQ:
                    opcode = IR_GE;
                    break;
                case TOKEN_AMP_AMP:
                    opcode = IR_LOGICAL_AND;
                    break;
                case TOKEN_PIPE_PIPE:
                    opcode = IR_LOGICAL_OR;
                    break;
                default:
                    // Should not happen
                    return;
            }
            
            ir_emit_instruction(func, arena, opcode, i32_type, operands, 2, (*reg_counter)++);
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
