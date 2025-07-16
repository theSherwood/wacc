#include "compiler.h"
#include <stdio.h>
#include <stdint.h>

// WASM bytecode constants
#define WASM_MAGIC 0x6d736100  // '\0asm'
#define WASM_VERSION 0x00000001

// WASM section types
#define SECTION_TYPE 1
#define SECTION_FUNCTION 3
#define SECTION_CODE 10
#define SECTION_EXPORT 7

// WASM value types
#define WASM_I32_TYPE 0x7f
#define WASM_I64_TYPE 0x7e
#define WASM_F32_TYPE 0x7d
#define WASM_F64_TYPE 0x7c

// WASM opcodes
#define WASM_LOCAL_GET 0x20
#define WASM_LOCAL_SET 0x21
#define WASM_DROP 0x1a
#define WASM_I32_CONST 0x41
#define WASM_I32_EQZ 0x45
#define WASM_I32_EQ 0x46
#define WASM_I32_NE 0x47
#define WASM_I32_LT_S 0x48
#define WASM_I32_LT_U 0x49
#define WASM_I32_GT_S 0x4a
#define WASM_I32_GT_U 0x4b
#define WASM_I32_LE_S 0x4c
#define WASM_I32_LE_U 0x4d
#define WASM_I32_GE_S 0x4e
#define WASM_I32_GE_U 0x4f
#define WASM_I32_ADD 0x6a
#define WASM_I32_SUB 0x6b
#define WASM_I32_MUL 0x6c
#define WASM_I32_DIV_S 0x6d
#define WASM_I32_DIV_U 0x6e
#define WASM_I32_REM_S 0x6f
#define WASM_I32_REM_U 0x70
#define WASM_I32_AND 0x71
#define WASM_I32_OR 0x72
#define WASM_I32_XOR 0x73
#define WASM_RETURN 0x0f
#define WASM_END 0x0b
#define WASM_BLOCK 0x02
#define WASM_LOOP 0x03
#define WASM_IF 0x04
#define WASM_ELSE 0x05
#define WASM_BR_IF 0x0d
#define WASM_BR 0x0c

// WASM export types
#define EXPORT_FUNC 0x00

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} Buffer;

static void buffer_init(Buffer* buf, Arena* arena, size_t initial_capacity) {
    buf->data = arena_alloc(arena, initial_capacity);
    buf->size = 0;
    buf->capacity = initial_capacity;
}

static void buffer_ensure_capacity(Buffer* buf, Arena* arena, size_t needed) {
    if (buf->size + needed > buf->capacity) {
        size_t new_capacity = buf->capacity;
        while (new_capacity < buf->size + needed) {
            new_capacity *= 2;
        }
        uint8_t* new_data = arena_alloc(arena, new_capacity);
        mem_cpy(new_data, buf->data, buf->size);
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
}

static void buffer_write_byte(Buffer* buf, Arena* arena, uint8_t byte) {
    buffer_ensure_capacity(buf, arena, 1);
    buf->data[buf->size++] = byte;
}

static void buffer_write_u32(Buffer* buf, Arena* arena, uint32_t value) {
    buffer_ensure_capacity(buf, arena, 4);
    buf->data[buf->size++] = (uint8_t)(value & 0xff);
    buf->data[buf->size++] = (uint8_t)((value >> 8) & 0xff);
    buf->data[buf->size++] = (uint8_t)((value >> 16) & 0xff);
    buf->data[buf->size++] = (uint8_t)((value >> 24) & 0xff);
}

static void buffer_write_leb128_u32(Buffer* buf, Arena* arena, uint32_t value) {
    while (value >= 0x80) {
        buffer_write_byte(buf, arena, (uint8_t)((value & 0x7f) | 0x80));
        value >>= 7;
    }
    buffer_write_byte(buf, arena, (uint8_t)(value & 0x7f));
}

static void buffer_write_leb128_i32(Buffer* buf, Arena* arena, int32_t value) {
    bool more = true;
    while (more) {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if ((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0)) {
            more = false;
        } else {
            byte |= 0x80;
        }
        buffer_write_byte(buf, arena, byte);
    }
}

static void buffer_write_string(Buffer* buf, Arena* arena, const char* str) {
    size_t len = str_len(str);
    buffer_write_leb128_u32(buf, arena, (uint32_t)len);
    buffer_ensure_capacity(buf, arena, len);
    mem_cpy(buf->data + buf->size, str, len);
    buf->size += len;
}

static void emit_section_header(Buffer* buf, Arena* arena, uint8_t section_type, size_t content_size) {
    buffer_write_byte(buf, arena, section_type);
    buffer_write_leb128_u32(buf, arena, (uint32_t)content_size);
}

static void emit_type_section(Buffer* buf, Arena* arena) {
    Buffer content;
    buffer_init(&content, arena, 64);
    
    // Number of types
    buffer_write_leb128_u32(&content, arena, 1);
    
    // Function type 0: [] -> [i32]
    buffer_write_byte(&content, arena, 0x60);  // func type
    buffer_write_leb128_u32(&content, arena, 0);  // param count
    buffer_write_leb128_u32(&content, arena, 1);  // result count
    buffer_write_byte(&content, arena, WASM_I32_TYPE);  // i32
    
    emit_section_header(buf, arena, SECTION_TYPE, content.size);
    buffer_ensure_capacity(buf, arena, content.size);
    mem_cpy(buf->data + buf->size, content.data, content.size);
    buf->size += content.size;
}

static void emit_function_section(Buffer* buf, Arena* arena) {
    Buffer content;
    buffer_init(&content, arena, 64);
    
    // Number of functions
    buffer_write_leb128_u32(&content, arena, 1);
    
    // Function 0 uses type 0
    buffer_write_leb128_u32(&content, arena, 0);
    
    emit_section_header(buf, arena, SECTION_FUNCTION, content.size);
    buffer_ensure_capacity(buf, arena, content.size);
    mem_cpy(buf->data + buf->size, content.data, content.size);
    buf->size += content.size;
}

static void emit_export_section(Buffer* buf, Arena* arena) {
    Buffer content;
    buffer_init(&content, arena, 64);
    
    // Number of exports
    buffer_write_leb128_u32(&content, arena, 1);
    
    // Export "main" function
    buffer_write_string(&content, arena, "main");
    buffer_write_byte(&content, arena, EXPORT_FUNC);
    buffer_write_leb128_u32(&content, arena, 0);  // function index
    
    emit_section_header(buf, arena, SECTION_EXPORT, content.size);
    buffer_ensure_capacity(buf, arena, content.size);
    mem_cpy(buf->data + buf->size, content.data, content.size);
    buf->size += content.size;
}

static void emit_instruction(Buffer* buf, Arena* arena, Instruction* inst) {
    switch (inst->opcode) {
        case IR_CONST_INT: {
            buffer_write_byte(buf, arena, WASM_I32_CONST);
            buffer_write_leb128_i32(buf, arena, inst->operands[0].value.constant.int_value);
            break;
        }
        case IR_LOAD_LOCAL: {
            buffer_write_byte(buf, arena, WASM_LOCAL_GET);
            buffer_write_leb128_u32(buf, arena, inst->operands[0].value.local_index);
            break;
        }
        case IR_STORE_LOCAL: {
            // The value to store is already on the stack from the expression evaluation
            buffer_write_byte(buf, arena, WASM_LOCAL_SET);
            buffer_write_leb128_u32(buf, arena, inst->operands[0].value.local_index);
            break;
        }
        case IR_NEG: {
            // Negate: x * -1
            buffer_write_byte(buf, arena, WASM_I32_CONST);
            buffer_write_leb128_i32(buf, arena, -1);
            buffer_write_byte(buf, arena, WASM_I32_MUL);
            break;
        }
        case IR_NOT: {
            // Logical not: x == 0
            buffer_write_byte(buf, arena, WASM_I32_EQZ);
            break;
        }
        case IR_BITWISE_NOT: {
            // Bitwise not: x XOR -1
            buffer_write_byte(buf, arena, WASM_I32_CONST);
            buffer_write_leb128_i32(buf, arena, -1);
            buffer_write_byte(buf, arena, WASM_I32_XOR);
            break;
        }
        case IR_ADD: {
            buffer_write_byte(buf, arena, WASM_I32_ADD);
            break;
        }
        case IR_SUB: {
            buffer_write_byte(buf, arena, WASM_I32_SUB);
            break;
        }
        case IR_MUL: {
            buffer_write_byte(buf, arena, WASM_I32_MUL);
            break;
        }
        case IR_DIV: {
            buffer_write_byte(buf, arena, WASM_I32_DIV_S);
            break;
        }
        case IR_MOD: {
            buffer_write_byte(buf, arena, WASM_I32_REM_S);
            break;
        }
        case IR_EQ: {
            buffer_write_byte(buf, arena, WASM_I32_EQ);
            break;
        }
        case IR_NE: {
            buffer_write_byte(buf, arena, WASM_I32_NE);
            break;
        }
        case IR_LT: {
            buffer_write_byte(buf, arena, WASM_I32_LT_S);
            break;
        }
        case IR_GT: {
            buffer_write_byte(buf, arena, WASM_I32_GT_S);
            break;
        }
        case IR_LE: {
            buffer_write_byte(buf, arena, WASM_I32_LE_S);
            break;
        }
        case IR_GE: {
            buffer_write_byte(buf, arena, WASM_I32_GE_S);
            break;
        }
        case IR_LOGICAL_AND: {
            // For logical AND, we need to implement short-circuit evaluation
            // For now, we'll treat it as bitwise AND of the boolean values
            buffer_write_byte(buf, arena, WASM_I32_AND);
            break;
        }
        case IR_LOGICAL_OR: {
            // For logical OR, we need to implement short-circuit evaluation
            // For now, we'll treat it as bitwise OR of the boolean values
            buffer_write_byte(buf, arena, WASM_I32_OR);
            break;
        }
        case IR_DUP: {
            // Duplicate top of stack by loading and storing to a temp local
            // For now, we'll use local 0 as temporary (need to improve this)
            buffer_write_byte(buf, arena, WASM_LOCAL_GET);
            buffer_write_leb128_u32(buf, arena, 0);  // Get current stack top via local
            break;
        }
        case IR_IF: {
            // The condition is already on the stack from expression evaluation
            buffer_write_byte(buf, arena, WASM_IF);
            // Use void type for statement context, i32 for expression context
            if (inst->result_type.kind == TYPE_VOID) {
                buffer_write_byte(buf, arena, 0x40);  // void result type
            } else {
                buffer_write_byte(buf, arena, WASM_I32_TYPE);  // i32 result type for expressions
            }
            break;
        }
        case IR_ELSE: {
            buffer_write_byte(buf, arena, WASM_ELSE);
            break;
        }
        case IR_END: {
            buffer_write_byte(buf, arena, WASM_END);
            break;
        }
        case IR_POP: {
            // Drop top stack value using WASM drop instruction
            buffer_write_byte(buf, arena, WASM_DROP);
            break;
        }
        case IR_RETURN: {
            buffer_write_byte(buf, arena, WASM_RETURN);
            break;
        }
        default:
            // Handle other instructions later
            break;
    }
}

static void emit_region(Buffer* buf, Arena* arena, Region* region) {
    if (!region) return;
    
    if (region->type == REGION_IF) {
        // First emit instructions in this region (includes condition)
        for (size_t i = 0; i < region->instruction_count; i++) {
            emit_instruction(buf, arena, &region->instructions[i]);
        }
        
        // For IF regions, emit the structured control flow
        buffer_write_byte(buf, arena, WASM_IF);
        
        // Use i32 result type for ternary expressions, void for if statements
        if (region->is_expression) {
            buffer_write_byte(buf, arena, WASM_I32_TYPE);  // i32 for ternary
        } else {
            buffer_write_byte(buf, arena, 0x40);  // void for if statements
        }
        
        // Emit then branch
        if (region->data.if_data.then_region) {
            emit_region(buf, arena, region->data.if_data.then_region);
        }
        
        // Emit else branch if present
        if (region->data.if_data.else_region) {
            buffer_write_byte(buf, arena, WASM_ELSE);
            emit_region(buf, arena, region->data.if_data.else_region);
        }
        
        // End the if
        buffer_write_byte(buf, arena, WASM_END);
    } else {
        // For FUNCTION regions, emit child regions first (statements processed first)
        // For other region types, emit instructions first, then child regions
        if (region->type == REGION_FUNCTION) {
            // Emit child regions first (control flow statements)
            for (size_t i = 0; i < region->child_count; i++) {
                emit_region(buf, arena, region->children[i]);
            }
            
            // Then emit function-level instructions
            for (size_t i = 0; i < region->instruction_count; i++) {
                emit_instruction(buf, arena, &region->instructions[i]);
            }
        } else {
            // For other region types, emit instructions normally
            for (size_t i = 0; i < region->instruction_count; i++) {
                emit_instruction(buf, arena, &region->instructions[i]);
            }
            
            // Emit child regions
            for (size_t i = 0; i < region->child_count; i++) {
                emit_region(buf, arena, region->children[i]);
            }
        }
    }
}

static void emit_code_section(Buffer* buf, Arena* arena, IRModule* ir_module) {
    Buffer content;
    buffer_init(&content, arena, 256);
    
    // Number of functions
    buffer_write_leb128_u32(&content, arena, ir_module->function_count);
    
    // Process each function
    for (size_t func_idx = 0; func_idx < ir_module->function_count; func_idx++) {
        Function* func = &ir_module->functions[func_idx];
        
        // Function body
        Buffer func_body;
        buffer_init(&func_body, arena, 128);
        
        // Local declarations: count of local variable groups
        buffer_write_leb128_u32(&func_body, arena, func->local_count > 0 ? 1 : 0);
        
        // Declare all i32 locals in one group
        if (func->local_count > 0) {
            buffer_write_leb128_u32(&func_body, arena, func->local_count);
            buffer_write_byte(&func_body, arena, WASM_I32_TYPE);
        }

        // Generate instructions from structured regions
        if (func->body) {
            emit_region(&func_body, arena, func->body);
        }
        
        // Add default return for functions that return int
        if (func->return_type.kind == TYPE_I32) {
            buffer_write_byte(&func_body, arena, WASM_I32_CONST);
            buffer_write_leb128_i32(&func_body, arena, 0);
            buffer_write_byte(&func_body, arena, WASM_RETURN);
        }
        
        // End instruction
        buffer_write_byte(&func_body, arena, WASM_END);
        
        // Write function body size and body
        buffer_write_leb128_u32(&content, arena, (uint32_t)func_body.size);
        buffer_ensure_capacity(&content, arena, func_body.size);
        mem_cpy(content.data + content.size, func_body.data, func_body.size);
        content.size += func_body.size;
    }
    
    emit_section_header(buf, arena, SECTION_CODE, content.size);
    buffer_ensure_capacity(buf, arena, content.size);
    mem_cpy(buf->data + buf->size, content.data, content.size);
    buf->size += content.size;
}

void codegen_emit_wasm(Arena* arena, IRModule* ir_module, const char* output_path) {
    if (!ir_module || ir_module->function_count == 0) {
        return;
    }
    
    Buffer buf;
    buffer_init(&buf, arena, 1024);
    
    // WASM magic and version
    buffer_write_u32(&buf, arena, WASM_MAGIC);
    buffer_write_u32(&buf, arena, WASM_VERSION);
    
    // Emit sections
    emit_type_section(&buf, arena);
    emit_function_section(&buf, arena);
    emit_export_section(&buf, arena);
    emit_code_section(&buf, arena, ir_module);
    
    // Write to file
    FILE* file = fopen(output_path, "wb");
    if (file) {
        fwrite(buf.data, 1, buf.size, file);
        fclose(file);
    }
}
