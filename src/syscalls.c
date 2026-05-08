/**
 * @file syscalls.c
 * @brief System call stubs for embedded systems
 * 
 * Provides minimal implementations of system calls required by newlib
 */

#include <errno.h>
#include <sys/types.h>
#include <stdint.h>

/* Symbols provided by the linker script */
extern char _end;

/* STM32F401xE has 96KB of RAM starting at 0x20000000 */
#define RAM_START   0x20000000
#define RAM_SIZE    0x18000    /* 96KB */
#define HEAP_END    (RAM_START + RAM_SIZE - 0x400)  /* Reserve 1KB for stack */

/**
 * @brief Increase program data space (brk call)
 * 
 * @param incr The amount of memory to allocate
 * @return The old heap pointer on success, (void*)-1 on error
 * 
 * This is called by malloc() and other memory allocation functions.
 */
caddr_t _sbrk(int incr)
{
    static char *heap_ptr = NULL;
    char *base;

    if (heap_ptr == NULL) {
        heap_ptr = &_end;
    }

    base = heap_ptr;

    /* Bounds check: prevent heap from growing beyond available memory */
    if ((uint32_t)(heap_ptr + incr) > HEAP_END) {
        errno = ENOMEM;
        return (caddr_t) - 1;
    }

    heap_ptr += incr;
    return (caddr_t) base;
}

/**
 * @brief Exit the application
 * 
 * @param code Exit code (unused in embedded systems)
 */
void _exit(int code)
{
    (void)code;
    /* Hang in an infinite loop */
    while (1) {
        __asm__("nop");
    }
}

/**
 * @brief Get current process ID (stub)
 * 
 * @return Always returns 1
 */
int _getpid(void)
{
    return 1;
}

/**
 * @brief Send signal (stub)
 * 
 * @param pid Process ID
 * @param sig Signal number
 * @return Always returns -1 (error)
 */
int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}
