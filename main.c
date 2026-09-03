#pragma GCC optimize("O0") // Force predictable stack behavior

#include <stdio.h>

#define CONCAT_INDIRECT(a, b) a##b
#define CONCAT(a, b) CONCAT_INDIRECT(a, b)

#define defer(code) \
    do { \
        /* Allocate a unique, writable 8-byte buffer in the data section */ \
        static char CONCAT(defer_buf_, __LINE__)[8] ; \
        \
        /* 1. Intercept the return address and rewrite it */ \
        __asm__ volatile ( \
            ".intel_syntax noprefix\n\t" \
            "mov rax, [rbp + 8]\n\t"              /* Get current return address */ \
            "mov rbx, %0\n\t"                     /* Load address of our local data buffer */ \
            "mov [rbx], rax\n\t"                  /* Save return address into the buffer */ \
            \
            /* 2. Overwrite stack return address with our defer entry point */ \
            "lea rax, [rip + 1f]\n\t" \
            "mov [rbp + 8], rax\n\t" \
            \
            /* 3. Skip the defer block during normal sequential execution */ \
            "jmp 2f\n\t" \
            \
            /* 4. The Defer Entry Point (Hit only when function tries to RET) */ \
            "1:\n\t" \
            ".att_syntax\n\t" \
            : \
            : "r"(CONCAT(defer_buf_, __LINE__)) \
            : "rax", "rbx", "memory" \
        ); \
        \
        /* 5. Execute the user's C code */ \
        code; \
        \
        /* 6. Jump back to the address stored in our dedicated buffer */ \
        __asm__ volatile ( \
            ".intel_syntax noprefix\n\t" \
            "mov rbx, %0\n\t"                     /* Load address of our local data buffer */ \
            "mov rax, [rbx]\n\t"                  /* Load the saved address */ \
            "jmp rax\n\t"                         /* Jump to it */ \
            \
            /* 7. Normal execution continuation point */ \
            "2:\n\t" \
            ".att_syntax\n\t" \
            : \
            : "r"(CONCAT(defer_buf_, __LINE__)) \
            : "rax", "rbx", "memory" \
        ); \
    } while(0)

void test_function() {
    printf("1. Function Body Started\n");

    defer({
        printf("4. First Defer Executing (Registered Last)\n");
    });

    defer({
        printf("3. Second Defer Executing (Registered First)\n");
    });

    printf("2. Function Body Ending (About to hit RET)\n");
}

int main() {
    test_function();
    printf("5. Back in Main safely!\n");
    return 0;
}
