#include "compiler.h"
#include <stdio.h>

// Simple symbol table for local variables
typedef struct {
    const char* name;
    int index;
    Type type;
} Symbol;

static Symbol* find_symbol(Symbol* table, int count, const char* name) {
    for (int i = 0; i < count; i++) {
        if (str_ncmp(table[i].name, name, str_len(name)) == 0) {
            return &table[i];
        }
    }
    return NULL;
}

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

static void ir_generate_expression(IRFunction* func, Arena* arena, ASTNode* expr, int* reg_counter, Symbol* symbol_table, int symbol_count) {
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
        case AST_VARIABLE_REF: { // Used for variable access
            Symbol* symbol = find_symbol(symbol_table, symbol_count, expr->data.variable_ref.name);
            if (symbol) {
                Operand operand;
                operand.type = OPERAND_LOCAL;
                operand.value_type = symbol->type;
                operand.value.local_index = symbol->index;
                ir_emit_instruction(func, arena, IR_LOAD_LOCAL, symbol->type, &operand, 1, (*reg_counter)++);
            } else {
                // Error: undefined variable handled in semantic analysis
            }
            break;
        }
        case AST_UNARY_OP: {
            // Generate IR for the operand first
            ir_generate_expression(func, arena, expr->data.unary_op.operand, reg_counter, symbol_table, symbol_count);
            
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
            ir_generate_expression(func, arena, expr->data.binary_op.left, reg_counter, symbol_table, symbol_count);
            int left_reg = (*reg_counter) - 1;
            
            // Generate IR for right operand
            ir_generate_expression(func, arena, expr->data.binary_op.right, reg_counter, symbol_table, symbol_count);
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

static void ir_generate_statement(IRFunction* func, Arena* arena, ASTNode* stmt, int* reg_counter, Symbol* symbol_table, int* symbol_count, int* local_counter) {
    switch (stmt->type) {
        case AST_RETURN_STATEMENT: {
            // Generate code for the return expression
            ir_generate_expression(func, arena, stmt->data.return_statement.expression, reg_counter, symbol_table, *symbol_count);
            
            // Generate return instruction
            Type i32_type = get_i32_type();
            Operand operand;
            operand.type = OPERAND_REGISTER;
            operand.value_type = i32_type;
            operand.value.reg = (*reg_counter) - 1; // Use the last generated register
            
            ir_emit_instruction(func, arena, IR_RETURN, i32_type, &operand, 1, -1);
            break;
        }
        case AST_VARIABLE_DECL: {
            Type type = get_i32_type(); // Assuming all variables are i32 for now
            ir_emit_instruction(func, arena, IR_ALLOCA, type, NULL, 0, -1); // No result register for alloca
            
            // Add to symbol table
            symbol_table[*symbol_count].name = stmt->data.variable_decl.name;
            symbol_table[*symbol_count].index = (*local_counter)++;
            symbol_table[*symbol_count].type = type;
            (*symbol_count)++;
            
            if (stmt->data.variable_decl.initializer) {
                ir_generate_expression(func, arena, stmt->data.variable_decl.initializer, reg_counter, symbol_table, *symbol_count);
                
                Symbol* symbol = find_symbol(symbol_table, *symbol_count, stmt->data.variable_decl.name);
                Operand operands[2];
                operands[0].type = OPERAND_LOCAL;
                operands[0].value.local_index = symbol->index;
                operands[0].value_type = symbol->type;
                
                operands[1].type = OPERAND_REGISTER;
                operands[1].value.reg = (*reg_counter) - 1;
                operands[1].value_type = get_i32_type();

                ir_emit_instruction(func, arena, IR_STORE_LOCAL, get_i32_type(), operands, 2, -1);
            }
            break;
        }
        case AST_ASSIGNMENT: {
            ir_generate_expression(func, arena, stmt->data.assignment.value, reg_counter, symbol_table, *symbol_count);
            
            Symbol* symbol = find_symbol(symbol_table, *symbol_count, stmt->data.assignment.name);
             if (symbol) {
                Operand operands[2];
                operands[0].type = OPERAND_LOCAL;
                operands[0].value.local_index = symbol->index;
                operands[0].value_type = symbol->type;
                
                operands[1].type = OPERAND_REGISTER;
                operands[1].value.reg = (*reg_counter) - 1;
                operands[1].value_type = get_i32_type();
                
                ir_emit_instruction(func, arena, IR_STORE_LOCAL, get_i32_type(), operands, 2, -1);
            } else {
                // Error: undefined variable
            }
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
    int local_counter = 0;
    Symbol symbol_table[256]; // Max 256 local variables per function
    int symbol_count = 0;
    
    // Generate IR for the function
    ASTNode* function_node = ast->data.program.function;
    for (int i = 0; i < function_node->data.function.statement_count; i++) {
        ir_generate_statement(func, arena, function_node->data.function.statements[i], &reg_counter, symbol_table, &symbol_count, &local_counter);
    }
    
    return module;
}

static const char* ir_opcode_to_string(IROpcode opcode) {
    switch (opcode) {
        case IR_CONST_INT: return "const_int";
        case IR_RETURN: return "return";
        case IR_ADD: return "add";
        case IR_SUB: return "sub";
        case IR_MUL: return "mul";
        case IR_DIV: return "div";
        case IR_MOD: return "mod";
        case IR_EQ: return "eq";
        case IR_NE: return "ne";
        case IR_LT: return "lt";
        case IR_GT: return "gt";
        case IR_LE: return "le";
        case IR_GE: return "ge";
        case IR_LOGICAL_AND: return "and";
        case IR_LOGICAL_OR: return "or";
        case IR_NEG: return "neg";
        case IR_NOT: return "not";
        case IR_BITWISE_NOT: return "bitnot";
        case IR_ALLOCA: return "alloca";
        case IR_LOAD_LOCAL: return "load_local";
        case IR_STORE_LOCAL: return "store_local";
        case IR_PUSH: return "push";
        case IR_POP: return "pop";
        default: return "unknown";
    }
}

static void ir_print_operand(Operand* operand) {
    switch (operand->type) {
        case OPERAND_REGISTER:
            printf("r%d", operand->value.reg);
            break;
        case OPERAND_CONSTANT:
            printf("%d", operand->value.constant.int_value);
            break;
        case OPERAND_LOCAL:
            printf("local%d", operand->value.local_index);
            break;
    }
}

static void ir_print_instruction(Instruction* inst) {
    printf("  ");
    
    if (inst->result_reg >= 0) {
        printf("r%d = ", inst->result_reg);
    }
    
    printf("%s", ir_opcode_to_string(inst->opcode));
    
    for (int i = 0; i < inst->operand_count; i++) {
        printf(i == 0 ? " " : ", ");
        ir_print_operand(&inst->operands[i]);
    }
    
    printf("\n");
}

void ir_print(IRModule* ir_module) {
    if (!ir_module) {
        printf("(null IR module)\n");
        return;
    }
    
    printf("=== IR ===\n");
    
    for (size_t i = 0; i < ir_module->function_count; i++) {
        IRFunction* func = &ir_module->functions[i];
        printf("function%zu:\n", i);
        
        for (size_t j = 0; j < func->instruction_count; j++) {
            ir_print_instruction(&func->instructions[j]);
        }
        
        printf("\n");
    }
    
    printf("==========\n");
}
