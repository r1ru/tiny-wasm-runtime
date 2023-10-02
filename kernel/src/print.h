#pragma once

/*
Use this macro for output. 
This way, the code becomes libc-independent.
*/

int printf(const char *fmt, ...);
void exit(int status);

#define PRINT_NL "\r\n"
#define SGR_ERR   "\e[1;91m"  // red + bold
#define SGR_WARN  "\e[1;33m"  // yellow + bold
#define SGR_INFO  "\e[0;96m"  // cyan
#define SGR_RESET "\e[0m"     // reset

#define INFO(fmt, ...)                                      \
    do {                                                    \
        printf(                                             \
            SGR_INFO fmt SGR_RESET PRINT_NL,                \
            ##__VA_ARGS__                                   \
        );                                                  \
    } while(0)

#define ERROR(fmt, ...)                                     \
    do {                                                    \
        printf(                                             \
            SGR_ERR "ERROR %s:%d " fmt SGR_RESET PRINT_NL,  \
            __FILE__, __LINE__, ##__VA_ARGS__               \
        );                                                  \
        exit(1);                                            \
    } while(0)
