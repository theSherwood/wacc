#include "compiler.h"
#include <stdio.h>

// Symbol table for scoped variable lookup
typedef struct SymbolTable {
    LocalVariable* variables;
    size_t count;
    size_t capacity;
    struct SymbolTable* parent;  // For nested scopes
} SymbolTable;

// IR Generation context
typedef struct IRContext {
    Arena* arena;
    Function* current_function;
    Region* current_region;
    SymbolTable* current_scope;
    int next_register;
    int next_region_id;
} IRContext;

// Helper functions for type creation
static Type create_i32_type() {
    Type type = {0};
    type.kind = TYPE_I32;
    type.wasm_type = WASM_I32;
    type.size = 4;
    type.alignment = 4;
    return type;
}

static Type create_void_type() {
    Type type = {0};
    type.kind = TYPE_VOID;
    type.wasm_type = WASM_I32;  // WASM doesn't have void, use i32
    type.size = 0;
    type.alignment = 1;
    return type;
}

// Symbol table operations
static SymbolTable* symbol_table_create(Arena* arena, SymbolTable* parent) {
    SymbolTable* table = arena_alloc(arena, sizeof(SymbolTable));
    table->variables = NULL;
    table->count = 0;
    table->capacity = 0;
    table->parent = parent;
    return table;
}

static LocalVariable* symbol_table_find(SymbolTable* table, const char* name) {
    // Search current scope first
    for (size_t i = 0; i < table->count; i++) {
        if (str_ncmp(table->variables[i].name, name, str_len(name)) == 0 &&
            str_len(table->variables[i].name) == str_len(name)) {
            return &table->variables[i];
        }
    }
    
    // Search parent scopes
    if (table->parent) {
        return symbol_table_find(table->parent, name);
    }
    
    return NULL;
}

static void symbol_table_add(Arena* arena, SymbolTable* table, const char* name, Type type, int index, bool is_stack_based) {
    if (table->count >= table->capacity) {
        table->capacity = table->capacity ? table->capacity * 2 : 8;
        LocalVariable* new_vars = arena_alloc(arena, table->capacity * sizeof(LocalVariable));
        if (table->variables) {
            mem_cpy(new_vars, table->variables, table->count * sizeof(LocalVariable));
        }
        table->variables = new_vars;
    }
    
    table->variables[table->count].name = name;
    table->variables[table->count].type = type;
    table->variables[table->count].index = index;
    table->variables[table->count].is_stack_based = is_stack_based;
    table->count++;
}

// Region operations
static Region* region_create(Arena* arena, RegionType type, int id, Region* parent) {
    Region* region = arena_alloc(arena, sizeof(Region));
    region->type = type;
    region->id = id;
    region->instructions = NULL;
    region->instruction_count = 0;
    region->instruction_capacity = 0;
    region->children = NULL;
    region->child_count = 0;
    region->child_capacity = 0;
    region->parent = parent;
    return region;
}

static void region_add_instruction(Arena* arena, Region* region, Instruction inst) {
    if (region->instruction_count >= region->instruction_capacity) {
        region->instruction_capacity = region->instruction_capacity ? region->instruction_capacity * 2 : 16;
        Instruction* new_instructions = arena_alloc(arena, region->instruction_capacity * sizeof(Instruction));
        if (region->instructions) {
            mem_cpy(new_instructions, region->instructions, region->instruction_count * sizeof(Instruction));
        }
        region->instructions = new_instructions;
    }
    
    region->instructions[region->instruction_count++] = inst;
}

static void region_add_child(Arena* arena, Region* parent, Region* child) {
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
        Region** new_children = arena_alloc(arena, parent->child_capacity * sizeof(Region*));
        if (parent->children) {
            mem_cpy(new_children, parent->children, parent->child_count * sizeof(Region*));
        }
        parent->children = new_children;
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

// Stack-based instruction emission
static void emit_instruction(IRContext* ctx, IROpcode opcode, Type result_type, Operand* operands, int operand_count) {
    Instruction inst = {0};
    inst.opcode = opcode;
    inst.result_type = result_type;
    inst.operand_count = operand_count;
    inst.result_reg = -1;  // Stack-based, no register assignment
    
    for (int i = 0; i < operand_count && i < 3; i++) {
        inst.operands[i] = operands[i];
    }
    
    region_add_instruction(ctx->arena, ctx->current_region, inst);
}

// Expression generation - generates stack-based operations
static void ir_generate_expression(IRContext* ctx, ASTNode* expr) {
    switch (expr->type) {
        case AST_INTEGER_CONSTANT: {
            Operand operand = {0};
            operand.type = OPERAND_CONSTANT;
            operand.value_type = create_i32_type();
            operand.value.constant.int_value = expr->data.integer_constant.value;
            
            emit_instruction(ctx, IR_CONST_INT, create_i32_type(), &operand, 1);
            break;
        }
        
        case AST_VARIABLE_REF: {
            LocalVariable* var = symbol_table_find(ctx->current_scope, expr->data.variable_ref.name);
            if (var) {
                Operand operand = {0};
                operand.type = OPERAND_LOCAL;
                operand.value_type = var->type;
                operand.value.local_index = var->index;
                
                emit_instruction(ctx, IR_LOAD_LOCAL, var->type, &operand, 1);
            }
            break;
        }
        
        case AST_UNARY_OP: {
            // Generate operand first (pushes value to stack)
            ir_generate_expression(ctx, expr->data.unary_op.operand);
            
            // Emit unary operation (pops operand, pushes result)
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
                    return;
            }
            
            emit_instruction(ctx, opcode, create_i32_type(), NULL, 0);
            break;
        }
        
        case AST_BINARY_OP: {
            // Handle logical operations with short-circuit evaluation
            if (expr->data.binary_op.operator == TOKEN_AMP_AMP) {
                // Logical AND: if (left) return right; else return left;
                // For now, fall back to regular binary operation
                ir_generate_expression(ctx, expr->data.binary_op.left);
                ir_generate_expression(ctx, expr->data.binary_op.right);
                emit_instruction(ctx, IR_LOGICAL_AND, create_i32_type(), NULL, 0);
                break;
            }
            else if (expr->data.binary_op.operator == TOKEN_PIPE_PIPE) {
                // Logical OR: if (left) return left; else return right;
                // For now, fall back to regular binary operation
                ir_generate_expression(ctx, expr->data.binary_op.left);
                ir_generate_expression(ctx, expr->data.binary_op.right);
                emit_instruction(ctx, IR_LOGICAL_OR, create_i32_type(), NULL, 0);
                break;
            }
            
            // Regular binary operations - generate both operands first
            ir_generate_expression(ctx, expr->data.binary_op.left);
            ir_generate_expression(ctx, expr->data.binary_op.right);
            
            // Emit binary operation (pops two operands, pushes result)
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
                default:
                    return;
            }
            
            emit_instruction(ctx, opcode, create_i32_type(), NULL, 0);
            break;
        }
        
        case AST_ASSIGNMENT: {
            // Generate the value expression (pushes to stack)
            ir_generate_expression(ctx, expr->data.assignment.value);
            
            // Find the variable
            LocalVariable* var = symbol_table_find(ctx->current_scope, expr->data.assignment.name);
            if (var) {
                Operand operands[2];
                
                // First operand: local variable index
                operands[0].type = OPERAND_LOCAL;
                operands[0].value_type = var->type;
                operands[0].value.local_index = var->index;
                
                // Store instruction (pops value from stack, stores in local)
                emit_instruction(ctx, IR_STORE_LOCAL, var->type, operands, 1);
                
                // Push the stored value back to stack (assignment returns the value)
                emit_instruction(ctx, IR_LOAD_LOCAL, var->type, operands, 1);
            }
            break;
        }
        
        case AST_TERNARY_EXPRESSION: {
            // Create IF region for ternary
            Region* if_region = region_create(ctx->arena, REGION_IF, ctx->next_region_id++, ctx->current_region);
            region_add_child(ctx->arena, ctx->current_region, if_region);
            
            // Generate condition expression in the if region (pushes value to stack)
            ctx->current_region = if_region;
            ir_generate_expression(ctx, expr->data.ternary_expression.condition);
            ctx->current_region = if_region->parent;
            
            // Generate true branch
            Region* then_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
            if_region->data.if_data.then_region = then_region;
            region_add_child(ctx->arena, if_region, then_region);
            
            Region* old_region = ctx->current_region;
            ctx->current_region = then_region;
            ir_generate_expression(ctx, expr->data.ternary_expression.true_expression);
            ctx->current_region = old_region;
            
            // Generate false branch
            Region* else_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
            if_region->data.if_data.else_region = else_region;
            region_add_child(ctx->arena, if_region, else_region);
            
            ctx->current_region = else_region;
            ir_generate_expression(ctx, expr->data.ternary_expression.false_expression);
            ctx->current_region = old_region;
            
            break;
        }
        
        default:
            break;
    }
}

// Statement generation
static void ir_generate_statement(IRContext* ctx, ASTNode* stmt) {
    switch (stmt->type) {
        case AST_RETURN_STATEMENT: {
            // Generate the return expression (pushes value to stack)
            ir_generate_expression(ctx, stmt->data.return_statement.expression);
            
            // Emit return instruction (pops value from stack and returns it)
            emit_instruction(ctx, IR_RETURN, create_i32_type(), NULL, 0);
            break;
        }
        
        case AST_VARIABLE_DECL: {
            Type var_type = create_i32_type();  // Assuming all vars are i32 for now
            
            // Add variable to function locals
            Function* func = ctx->current_function;
            if (func->local_count >= func->local_capacity) {
                func->local_capacity = func->local_capacity ? func->local_capacity * 2 : 8;
                LocalVariable* new_locals = arena_alloc(ctx->arena, func->local_capacity * sizeof(LocalVariable));
                if (func->locals) {
                    mem_cpy(new_locals, func->locals, func->local_count * sizeof(LocalVariable));
                }
                func->locals = new_locals;
            }
            
            int local_index = func->local_count;
            func->locals[func->local_count].name = stmt->data.variable_decl.name;
            func->locals[func->local_count].type = var_type;
            func->locals[func->local_count].index = local_index;
            func->locals[func->local_count].is_stack_based = false;  // Use WASM local for now
            func->local_count++;
            
            // Add to symbol table
            symbol_table_add(ctx->arena, ctx->current_scope, stmt->data.variable_decl.name, var_type, local_index, false);
            
            // Handle initializer if present
            if (stmt->data.variable_decl.initializer) {
                // Generate initializer expression (pushes value to stack)
                ir_generate_expression(ctx, stmt->data.variable_decl.initializer);
                
                // Store in local variable (pops value from stack)
                Operand operand = {0};
                operand.type = OPERAND_LOCAL;
                operand.value_type = var_type;
                operand.value.local_index = local_index;
                
                emit_instruction(ctx, IR_STORE_LOCAL, var_type, &operand, 1);
            }
            break;
        }
        
        case AST_ASSIGNMENT: {
            // Generate the assignment expression
            ir_generate_expression(ctx, stmt);
            
            // Pop the result (we don't need it for statement context)
            emit_instruction(ctx, IR_POP, create_i32_type(), NULL, 0);
            break;
        }
        
        case AST_BINARY_OP: {
            // Generate the binary operation expression
            ir_generate_expression(ctx, stmt);
            
            // Pop the result (we don't need it for statement context)
            emit_instruction(ctx, IR_POP, create_i32_type(), NULL, 0);
            break;
        }
        
        case AST_IF_STATEMENT: {
            // Create IF region
            Region* if_region = region_create(ctx->arena, REGION_IF, ctx->next_region_id++, ctx->current_region);
            region_add_child(ctx->arena, ctx->current_region, if_region);
            
            // Generate condition expression in the if region (pushes value to stack)
            ctx->current_region = if_region;
            ir_generate_expression(ctx, stmt->data.if_statement.condition);
            ctx->current_region = if_region->parent;
            
            // Generate then branch
            Region* then_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
            if_region->data.if_data.then_region = then_region;
            region_add_child(ctx->arena, if_region, then_region);
            
            Region* old_region = ctx->current_region;
            ctx->current_region = then_region;
            ir_generate_statement(ctx, stmt->data.if_statement.then_statement);
            ctx->current_region = old_region;
            
            // Generate else branch if present
            if (stmt->data.if_statement.else_statement) {
                Region* else_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
                if_region->data.if_data.else_region = else_region;
                region_add_child(ctx->arena, if_region, else_region);
                
                ctx->current_region = else_region;
                ir_generate_statement(ctx, stmt->data.if_statement.else_statement);
                ctx->current_region = old_region;
            }
            
            break;
        }
        
        default:
            break;
    }
}

// Helper function to check if a region contains a return statement
static bool region_has_return(Region* region) {
    if (!region) return false;
    
    // Check instructions in this region
    for (size_t i = 0; i < region->instruction_count; i++) {
        if (region->instructions[i].opcode == IR_RETURN) {
            return true;
        }
    }
    
    // Check child regions recursively
    for (size_t i = 0; i < region->child_count; i++) {
        if (region_has_return(region->children[i])) {
            return true;
        }
    }
    
    return false;
}

// Main IR generation function
IRModule* ir_generate(Arena* arena, ASTNode* ast) {
    if (!ast || ast->type != AST_PROGRAM) {
        return NULL;
    }
    
    IRModule* module = arena_alloc(arena, sizeof(IRModule));
    module->functions = NULL;
    module->function_count = 0;
    module->function_capacity = 0;
    
    // Create IR context
    IRContext ctx = {0};
    ctx.arena = arena;
    ctx.next_register = 0;
    ctx.next_region_id = 0;
    
    // Process the function
    ASTNode* function_node = ast->data.program.function;
    
    // Allocate function
    if (module->function_count >= module->function_capacity) {
        module->function_capacity = 4;
        module->functions = arena_alloc(arena, module->function_capacity * sizeof(Function));
    }
    
    Function* func = &module->functions[module->function_count++];
    func->name = function_node->data.function.name;
    func->return_type = create_i32_type();
    func->parameters = NULL;
    func->param_count = 0;
    func->locals = NULL;
    func->local_count = 0;
    func->local_capacity = 0;
    func->max_stack_size = 0;
    
    // Create function body region
    func->body = region_create(arena, REGION_FUNCTION, ctx.next_region_id++, NULL);
    
    // Set context
    ctx.current_function = func;
    ctx.current_region = func->body;
    ctx.current_scope = symbol_table_create(arena, NULL);
    
    // Generate IR for all statements
    for (int i = 0; i < function_node->data.function.statement_count; i++) {
        ir_generate_statement(&ctx, function_node->data.function.statements[i]);
    }
    
    // Add implicit return 0 if the function doesn't have an explicit return
    bool has_return = region_has_return(func->body);
    
    if (!has_return) {
        // Add implicit return 0
        Instruction const_inst = {0};
        const_inst.opcode = IR_CONST_INT;
        const_inst.result_type = create_i32_type();
        const_inst.operand_count = 1;
        const_inst.operands[0].type = OPERAND_CONSTANT;
        const_inst.operands[0].value.constant.int_value = 0;
        const_inst.result_reg = -1;
        region_add_instruction(arena, func->body, const_inst);
        
        Instruction return_inst = {0};
        return_inst.opcode = IR_RETURN;
        return_inst.result_type = create_void_type();
        return_inst.operand_count = 0;
        return_inst.result_reg = -1;
        region_add_instruction(arena, func->body, return_inst);
    }
    
    return module;
}

// IR printing functions
static const char* ir_opcode_to_string(IROpcode opcode) {
    switch (opcode) {
        case IR_CONST_INT: return "const.i32";
        case IR_ADD: return "i32.add";
        case IR_SUB: return "i32.sub";
        case IR_MUL: return "i32.mul";
        case IR_DIV: return "i32.div_s";
        case IR_MOD: return "i32.rem_s";
        case IR_NEG: return "i32.neg";
        case IR_NOT: return "i32.eqz";
        case IR_BITWISE_NOT: return "i32.xor";
        case IR_EQ: return "i32.eq";
        case IR_NE: return "i32.ne";
        case IR_LT: return "i32.lt_s";
        case IR_GT: return "i32.gt_s";
        case IR_LE: return "i32.le_s";
        case IR_GE: return "i32.ge_s";
        case IR_LOGICAL_AND: return "i32.and";
        case IR_LOGICAL_OR: return "i32.or";
        case IR_LOAD_LOCAL: return "local.get";
        case IR_STORE_LOCAL: return "local.set";
        case IR_RETURN: return "return";
        case IR_POP: return "drop";
        case IR_DUP: return "dup";
        case IR_BLOCK: return "block";
        case IR_LOOP: return "loop";
        case IR_IF: return "if";
        case IR_ELSE: return "else";
        case IR_END: return "end";
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
            printf("$%d", operand->value.local_index);
            break;
        case OPERAND_GLOBAL:
            printf("g%d", operand->value.global_index);
            break;
        case OPERAND_MEMORY:
            printf("mem[%d]", operand->value.memory_offset);
            break;
        case OPERAND_LABEL:
            printf("label%d", operand->value.label_id);
            break;
    }
}

static void ir_print_instruction(Instruction* inst, int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    printf("%s", ir_opcode_to_string(inst->opcode));
    
    for (int i = 0; i < inst->operand_count; i++) {
        printf(" ");
        ir_print_operand(&inst->operands[i]);
    }
    
    printf("\n");
}

static void ir_print_region(Region* region, int indent) {
    if (!region) return;
    
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    switch (region->type) {
        case REGION_FUNCTION:
            printf("function:\n");
            break;
        case REGION_BLOCK:
            printf("block:\n");
            break;
        case REGION_LOOP:
            printf("loop:\n");
            break;
        case REGION_IF:
            printf("if:\n");
            break;
    }
    
    // Print instructions
    for (size_t i = 0; i < region->instruction_count; i++) {
        ir_print_instruction(&region->instructions[i], indent + 1);
    }
    
    // Print children
    for (size_t i = 0; i < region->child_count; i++) {
        ir_print_region(region->children[i], indent + 1);
    }
}

void ir_print(IRModule* ir_module) {
    if (!ir_module) {
        printf("(null IR module)\n");
        return;
    }
    
    printf("=== IR (Stack-based) ===\n");
    
    for (size_t i = 0; i < ir_module->function_count; i++) {
        Function* func = &ir_module->functions[i];
        printf("function %s(", func->name ? func->name : "anonymous");
        
        for (size_t j = 0; j < func->param_count; j++) {
            if (j > 0) printf(", ");
            printf("$%d", func->parameters[j].index);
        }
        
        printf(") -> ");
        printf("i32");  // Assuming return type is i32
        printf(" {\n");
        
        // Print locals
        printf("  locals: ");
        for (size_t j = 0; j < func->local_count; j++) {
            if (j > 0) printf(", ");
            printf("$%d:%s", func->locals[j].index, func->locals[j].name);
        }
        printf("\n");
        
        // Print function body
        if (func->body) {
            ir_print_region(func->body, 1);
        }
        
        printf("}\n\n");
    }
    
    printf("======================\n");
}
