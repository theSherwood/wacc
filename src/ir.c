#include <stdio.h>

#include "compiler.h"

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

static void symbol_table_add(Arena* arena, SymbolTable* table, const char* name, Type type, int index,
                             bool is_stack_based) {
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
  region->is_expression = false;
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

// Stack-based instruction emission
static void emit_instruction(IRContext* ctx, IROpcode opcode, Type result_type, Operand* operands, int operand_count) {
  Instruction inst = {0};
  inst.opcode = opcode;
  inst.result_type = result_type;
  inst.operand_count = operand_count;

  for (int i = 0; i < operand_count && i < 3; i++) {
    inst.operands[i] = operands[i];
  }

  region_add_instruction(ctx->arena, ctx->current_region, inst);
}

static void emit_region_instruction(IRContext* ctx, Region* region, Type (*func_ptr)()) {
  Operand operand = {0};
  operand.type = OPERAND_REGION;
  operand.value_type = func_ptr();
  operand.value.region = region;

  emit_instruction(ctx, IR_REGION, func_ptr(), &operand, 1);
}

static void add_function_local(IRContext* ctx, Type type, const char* name) {
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
  func->locals[func->local_count].name = name;
  func->locals[func->local_count].type = type;
  func->locals[func->local_count].index = local_index;
  func->locals[func->local_count].is_stack_based = false;  // Use WASM local for now
  func->local_count++;
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
      // Logical operations with short-circuit evaluation were transformed
      // into ternaries at an earlier stage.

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
      if_region->is_expression = true;
      Region* old_region = ctx->current_region;
      ctx->current_region = if_region;

      // Generate condition expression in the if region (pushes value to stack)
      ir_generate_expression(ctx, expr->data.ternary_expression.condition);

      // Generate true branch
      Region* then_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
      if_region->data.if_data.then_region = then_region;

      ctx->current_region = then_region;
      ir_generate_expression(ctx, expr->data.ternary_expression.true_expression);
      ctx->current_region = if_region;

      // Generate false branch
      Region* else_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
      if_region->data.if_data.else_region = else_region;

      ctx->current_region = else_region;
      ir_generate_expression(ctx, expr->data.ternary_expression.false_expression);

      ctx->current_region = old_region;
      emit_region_instruction(ctx, if_region, create_void_type);

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
      int local_index = ctx->current_function->local_count;
      add_function_local(ctx, var_type, stmt->data.variable_decl.name);

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
      Region* old_region = ctx->current_region;
      ctx->current_region = if_region;

      // Generate condition expression in the if region (pushes value to stack)
      ir_generate_expression(ctx, stmt->data.if_statement.condition);

      // Generate then branch
      Region* then_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
      if_region->data.if_data.then_region = then_region;
      // emit_region_instruction(ctx, then_region, create_void_type);

      ctx->current_region = then_region;
      ir_generate_statement(ctx, stmt->data.if_statement.then_statement);

      // Generate else branch if present
      if (stmt->data.if_statement.else_statement) {
        ctx->current_region = if_region;
        Region* else_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, if_region);
        if_region->data.if_data.else_region = else_region;
        // emit_region_instruction(ctx, else_region, create_void_type);

        ctx->current_region = else_region;
        ir_generate_statement(ctx, stmt->data.if_statement.else_statement);
      }

      ctx->current_region = old_region;
      emit_region_instruction(ctx, if_region, create_void_type);

      break;
    }

    case AST_DO_WHILE_STATEMENT:
    case AST_WHILE_STATEMENT: {
      // Create LOOP region for the while statement
      Region* loop_region = region_create(ctx->arena, REGION_LOOP, ctx->next_region_id++, ctx->current_region);
      if (stmt->type == AST_DO_WHILE_STATEMENT) loop_region->data.loop_data.is_do_while = true;

      // Generate condition expression and body in the loop region
      Region* condition_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, loop_region);
      loop_region->data.loop_data.condition = condition_region;
      ctx->current_region = condition_region;
      ir_generate_expression(ctx, stmt->data.while_statement.condition);

      // Create body region
      Region* body_region = region_create(ctx->arena, REGION_BLOCK, ctx->next_region_id++, loop_region);
      loop_region->data.loop_data.body = body_region;
      ctx->current_region = body_region;
      ir_generate_statement(ctx, stmt->data.while_statement.body);

      ctx->current_region = loop_region->parent;

      emit_region_instruction(ctx, loop_region, create_void_type);

      break;
    }

    case AST_FOR_STATEMENT: {
      //

      break;
    }

    case AST_BREAK_STATEMENT: {
      emit_instruction(ctx, IR_BREAK, create_i32_type(), NULL, 0);
      break;
    }

    case AST_CONTINUE_STATEMENT: {
      emit_instruction(ctx, IR_CONTINUE, create_i32_type(), NULL, 0);
      break;
    }

    case AST_COMPOUND_STATEMENT: {
      // Create a new scope for the compound statement
      SymbolTable* previous_scope = ctx->current_scope;
      ctx->current_scope = symbol_table_create(ctx->arena, previous_scope);

      if (!ctx->current_scope) {
        ctx->current_scope = previous_scope;
        return;
      }

      // Generate IR for all statements in the compound statement
      for (int i = 0; i < stmt->data.compound_statement.statement_count; i++) {
        ir_generate_statement(ctx, stmt->data.compound_statement.statements[i]);
      }

      // Restore previous scope
      ctx->current_scope = previous_scope;

      break;
    }

    default:
      ir_generate_expression(ctx, stmt);

      break;
  }
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

  return module;
}

// IR printing functions
static const char* ir_opcode_to_string(IROpcode opcode) {
  switch (opcode) {
    case IR_CONST_INT:
      return "const.i32";
    case IR_ADD:
      return "i32.add";
    case IR_SUB:
      return "i32.sub";
    case IR_MUL:
      return "i32.mul";
    case IR_DIV:
      return "i32.div_s";
    case IR_MOD:
      return "i32.rem_s";
    case IR_NEG:
      return "i32.neg";
    case IR_NOT:
      return "i32.eqz";
    case IR_BITWISE_NOT:
      return "i32.xor";
    case IR_EQ:
      return "i32.eq";
    case IR_NE:
      return "i32.ne";
    case IR_LT:
      return "i32.lt_s";
    case IR_GT:
      return "i32.gt_s";
    case IR_LE:
      return "i32.le_s";
    case IR_GE:
      return "i32.ge_s";
    case IR_LOGICAL_AND:
      return "i32.and";
    case IR_LOGICAL_OR:
      return "i32.or";
    case IR_LOAD_LOCAL:
      return "local.get";
    case IR_STORE_LOCAL:
      return "local.set";
    case IR_RETURN:
      return "return";
    case IR_BREAK:
      return "br";
    case IR_POP:
      return "drop";
    case IR_DUP:
      return "dup";
    case IR_BLOCK:
      return "block";
    case IR_LOOP:
      return "loop";
    case IR_IF:
      return "if";
    case IR_ELSE:
      return "else";
    case IR_END:
      return "end";
    case IR_REGION:
      return "";
    default:
      return "unknown";
  }
}

static void ir_print_region(Region* region, int indent);

static void ir_print_operand(Operand* operand, int indent) {
  switch (operand->type) {
    case OPERAND_REGION:
      ir_print_region(operand->value.region, indent);
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

static void print_indent(int indent) {
  for (int i = 0; i < indent; i++) {
    printf("  ");
  }
}

static void ir_print_instruction(Instruction* inst, int indent) {
  print_indent(indent);

  printf("%s", ir_opcode_to_string(inst->opcode));

  for (int i = 0; i < inst->operand_count; i++) {
    if (inst->opcode != IR_REGION) printf(" ");
    ir_print_operand(&inst->operands[i], indent);
  }

  if (inst->opcode != IR_REGION) printf("\n");
}

static void ir_print_instructions(Region* region, int indent) {
  for (size_t i = 0; i < region->instruction_count; i++) {
    ir_print_instruction(&region->instructions[i], indent + 1);
  }
}

static void ir_print_region(Region* region, int indent) {
  if (!region) return;

  switch (region->type) {
    case REGION_FUNCTION:
      ir_print_instructions(region, indent - 1);
      return;
    case REGION_BLOCK:
      printf("block:\n");
      ir_print_instructions(region, indent);
      break;
    case REGION_LOOP: {
      if (region->data.loop_data.is_do_while) {
        printf("loop (do-while):\n");
        print_indent(indent + 1);
        ir_print_region(region->data.loop_data.body, indent + 1);
        print_indent(indent + 1);
        ir_print_region(region->data.loop_data.condition, indent + 1);
      } else {
        printf("loop:\n");
        print_indent(indent + 1);
        ir_print_region(region->data.loop_data.condition, indent + 1);
        print_indent(indent + 1);
        ir_print_region(region->data.loop_data.body, indent + 1);
      }
      break;
    }
    case REGION_IF: {
      if (region->is_expression)
        printf("if (expr):\n");
      else
        printf("if:\n");
      ir_print_instructions(region, indent);
      print_indent(indent + 1);
      ir_print_region(region->data.if_data.then_region, indent + 1);
      if (region->data.if_data.else_region) {
        print_indent(indent + 1);
        ir_print_region(region->data.if_data.else_region, indent + 1);
      }
      break;
    }
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
    print_indent(1);
    printf("locals: ");
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
