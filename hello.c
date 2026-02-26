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

void format_to_str(char* out, const char* fmt, va_list args) {
    for(out[0] = 0; *fmt != '\0'; fmt++) {
        if (*fmt != '%') {
            strncat(out, fmt, 1);
        } else {
            fmt++;
            if (*fmt == 's') {
                strcat(out, va_arg(args, char*));
            } else if (*fmt == 'd') {
                itoa(va_arg(args, int), out + strlen(out), 10);
            } else if (*fmt == 'c') {
                // terminal_write("arrive here\n\r", 15);
                char c = (char)va_arg(args, int);
                size_t len = strlen(out);
                out[len] = c;
                out[len + 1] = '\0';
            } else if (*fmt == 'x') {
                int val = va_arg(args, int);

                // base case: input is int 0
                if (val == 0) {
                    char res[2];
                    if (res) strcpy(res, "0");
                    strcat(out, res);
                    continue;
                }

                // convert decimal to hexadecimal
                int reminder = 0, i = 1;
                char hex_buffer[100];
                while (val != 0) {
                    reminder = val % 16;
                    
                    if (reminder >= 10 & reminder <= 15) {
                        reminder += 55;
                    } else {
                        reminder += 48;
                    }

                    hex_buffer[i++] = (char)reminder;
                    val = val / 16;
                }

                // reverse the char buffer 
                char str[i];
                for (int j = 0; j <= i - 1; ++j) {
                    str[j] = hex_buffer[i - j];
                    terminal_write(&str[j], 1);
                }
                str[i] = '\0';

                strcat(out, str);
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
/*
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
*/

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

    return 0;
}
