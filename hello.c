void terminal_write(const char *str, int len) {
    for (int i = 0; i < len; i++) {
        *(char*)(0x10000000) = str[i];
    }
}

/* Uncomment line11 - line39
 * when implementing formatted output
 */

#include <stdlib.h>  // for itoa() and utoa()
#include <string.h>  // for strlen() and strcat()
#include <stdarg.h>  // for va_start(), va_end(), va_arg() and va_copy()
#include <stddef.h>  // for size_t
#include <stdint.h>  // for uintptr_t, uint8_t

void ulltoa(char* dst, size_t len, unsigned long long x) 
{
    if (len == 0) return;

    char str_buffer[24];
    int i = 23;
    str_buffer[i] = '\0';

    do {
        unsigned long long rem = x % 10;
        str_buffer[--i] = rem + '0';
        x /= 10;
    } while (x > 0);

    // use pointer and memcpy
    const char* src = &str_buffer[i];
    size_t avail = len - 1;
    size_t n = 24 - i;
    
    if (n > avail) n = avail;
    memcpy(dst, src, n);
    dst[n] = '\0';
    return;
}

void format_to_str(char* out, const char* fmt, va_list args) {
    out[0] = '\0';  // Fixed: initialize properly

    for (; *fmt != '\0'; fmt++) {
        if (*fmt != '%') {
            strncat(out, fmt, 1);
        } else {
            fmt++;

            if (*fmt == 's') 
            {
                strcat(out, va_arg(args, char*));
            } 
            else if (*fmt == 'd') 
            {
                itoa(va_arg(args, int), out + strlen(out), 10);
            } 
            else if (*fmt == 'c') 
            {
                // terminal_write("arrive here\n\r", 15);
                char c = (char)va_arg(args, int);
                size_t len = strlen(out);
                out[len] = c;
                out[len + 1] = '\0';
            } 
            else if (*fmt == 'x') 
            {
                unsigned int val = va_arg(args, unsigned int);

                if (val == 0) {
                    strcat(out, "0");
                    continue;
                }

                // convert decimal to hexadecimal
                char hex_buffer[100];
                int i = 0;
                while (val != 0) {
                    int remainder = val % 16;
                    
                    if (remainder >= 10) {
                        hex_buffer[i++] = 'a' + (remainder - 10);
                    } else {
                        hex_buffer[i++] = '0' + remainder;
                    }

                    val = val / 16;
                }

                // reverse the char buffer 
                while (i > 0) {
                    --i;
                    size_t len = strlen(out);
                    out[len]   = hex_buffer[i];
                    out[len + 1] = '\0';
                }
            } 
            else if (*fmt == 'u') 
            {
                utoa(va_arg(args, unsigned int), out + strlen(out), 10);
            } 
            else if (*fmt == 'p') 
            {
                const char hex_prefix[3] = "0x";
                strcat(out, hex_prefix);
                
                // Handle pointer as unsigned long
                unsigned long ptr_val = (unsigned long)va_arg(args, void*);
                utoa(ptr_val, out + strlen(out), 16);
            } 
            else if (strncmp(fmt, "llu", 3) == 0) 
            {
                unsigned long long val = (unsigned long long)va_arg(args, unsigned long long);

                if (val == 0) {
                    strcat(out, "0");
                } else {
                    char buf[32];
                    ulltoa(buf, sizeof(buf), val);
                    strcat(out, buf);
                }
                fmt += 2;
            }
        }
    }
}

int printf(const char* format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    format_to_str(buf, format, args);
    va_end(args);
    terminal_write(buf, strlen(buf));

    return 0;
}


/* Uncomment line46 - line57
 * when implementing dynamic memory allocation
 */

extern char __heap_start, __heap_end;
static char* brk = &__heap_start;
char* _sbrk(int size) {
    if (brk + size > (char*)&__heap_end) {
        terminal_write("_sbrk: heap grows too large\r\n", 29);
        return NULL;
    }

    char* old_brk = brk;
    brk += size;
    return old_brk;
}

// ---- Custom allocator hooks----
//
// `custom_malloc()` / `custom_free()` are stable entrypoints used by the rest
// of this file. Implement your allocator in `custom_malloc()` and
// `custom_free()` below. 

#ifndef RUN_ALLOCATOR_TESTS
#define RUN_ALLOCATOR_TESTS 1
#endif

#ifndef CUSTOM_ALLOCATOR_HAS_REAL_FREE
#define CUSTOM_ALLOCATOR_HAS_REAL_FREE 0
#endif

typedef struct Block {
    size_t size;
    int free; // 1 if available, 0 if occupied
    struct Block* next;
} Block;

#define BLOCK_SIZE ((sizeof(Block) + 7) & ~7)

Block* free_list_head = NULL;

Block* find_free_block(size_t size) {
    Block* current = free_list_head;
    while (current && !(current->free && current->size >= size)) {
        current = current->next;
    }
    return current;
}

void* custom_malloc(size_t size) {
    if (size <= 0) return NULL;

    size = (size + 7) & ~7;

    // 1. Try to find an existing free block
    Block* block = find_free_block(size);
    if (block) {
        block->free = 0;
        return (void*)(block + 1);
    }

    // 2. No block found, request memory from OS
    size_t total_size = BLOCK_SIZE + size;
    total_size = (total_size + 7) & ~7;

    block = (Block*)_sbrk(total_size);
    if (block == NULL) return NULL;

    block->size = total_size - BLOCK_SIZE; 
    block->free = 0;
    block->next = free_list_head;
    free_list_head = block;

    return (void*)(block + 1);
}

void custom_free(void* ptr) {
    if (!ptr) return;

    Block* block = (Block*)ptr - 1;
    block->free = 1;
}

/* 
 * Below are the tests for custom_malloc and custom_free
 */

static int heap_contains(const void* p) {
    uintptr_t x = (uintptr_t)p;
    return x >= (uintptr_t)&__heap_start && x < (uintptr_t)&__heap_end;
}

static int alloc_assert(int cond, const char* msg) {
    if (cond) return 1;
    printf("[alloc-test] FAIL: %s\n\r", (char*)msg);
    return 0;
}

static void allocator_selftest(void) {
    int ok = 1;

    printf("[alloc-test] heap: %p .. %p\n\r", &__heap_start, &__heap_end);

    {
        void* a = custom_malloc(16);
        void* b = custom_malloc(32);
        void* c = custom_malloc(128);
        ok &= alloc_assert(a != NULL && b != NULL && c != NULL, "smoke alloc != NULL");
        ok &= alloc_assert(a != b && b != c && a != c, "distinct pointers");
        ok &= alloc_assert(heap_contains(a) && heap_contains(b) && heap_contains(c), "pointers within heap");
        ok &= alloc_assert((((uintptr_t)a & 7u) == 0u) && (((uintptr_t)b & 7u) == 0u) && (((uintptr_t)c & 7u) == 0u),
                           "8-byte alignment");
    }

    {
        uint8_t* p = (uint8_t*)custom_malloc(256);
        ok &= alloc_assert(p != NULL, "alloc 256");
        if (p) {
            for (size_t i = 0; i < 256; i++) p[i] = (uint8_t)(i ^ 0xA5u);
            for (size_t i = 0; i < 256; i++) ok &= alloc_assert(p[i] == (uint8_t)(i ^ 0xA5u), "read/write integrity");
        }
    }

#if CUSTOM_ALLOCATOR_HAS_REAL_FREE
    {
        uintptr_t heap_bytes = (uintptr_t)&__heap_end - (uintptr_t)&__heap_start;
        size_t block = 2048;
        size_t iters = (size_t)(heap_bytes / (uintptr_t)block) + 64;

        for (size_t i = 0; i < iters; i++) {
            uint8_t* p = (uint8_t*)custom_malloc(block);
            if (!p) {
                ok = 0;
                printf("[alloc-test] FAIL: free/reuse loop ran out at iter=%u\n\r", (unsigned)i);
                break;
            }
            p[0] = 0x11;
            p[block - 1] = 0x22;
            custom_free(p);
        }

        if (ok) printf("[alloc-test] free/reuse loop OK (iters=%u, block=%u)\n\r",
                       (unsigned)iters, (unsigned)block);
    }
#endif

    if (ok) printf("[alloc-test] PASS\n\r");
    else printf("[alloc-test] FAIL (see above)\n\r");
}

int main() {
    char* msg = "Hello, World!\n\r";
    terminal_write(msg, 15);

    /* Uncomment this line of code
     * when implementing formatted output
     */
    printf("%s-%d is awesome!\n\r", "egos", 2000);
    printf("%c is character $\n\r", '$');
    printf("%c is character 0\n\r", (char)48);
    printf("%x is integer 1234 in hexadecimal\n\r", 1234);
    printf("%u is the maximum of unsigned int\n\r", (unsigned int)0xFFFFFFFF);
    printf("%p is the hexadecimal address of the hello-world string\n\r", msg);
    printf("%llu is the maximum of unsigned long long\n\r", 0xFFFFFFFFFFFFFFFFULL);

#if RUN_ALLOCATOR_TESTS
    allocator_selftest();
#endif

    return 0;
}
