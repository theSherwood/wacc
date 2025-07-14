#include "compiler.h"

// Character classification functions
bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

// String functions
size_t str_len(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

int str_ncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

void str_ncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
}

void mem_cpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

// Integer parsing function
long str_to_long(const char* str, char** endptr, int base) {
    // Simple implementation for base 10 only
    if (base != 10) {
        if (endptr) *endptr = (char*)str;
        return 0;
    }
    
    long result = 0;
    bool negative = false;
    const char* ptr = str;
    
    // Skip whitespace
    while (is_space(*ptr)) {
        ptr++;
    }
    
    // Handle sign
    if (*ptr == '-') {
        negative = true;
        ptr++;
    } else if (*ptr == '+') {
        ptr++;
    }
    
    // Parse digits
    while (is_digit(*ptr)) {
        result = result * 10 + (*ptr - '0');
        ptr++;
    }
    
    if (endptr) {
        *endptr = (char*)ptr;
    }
    
    return negative ? -result : result;
}
