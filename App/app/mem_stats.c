/*
 * mem_stats.c — improved _sbrk and runtime heap/RAM statistics for K1 firmware.
 *
 * This module provides:
 *   - A safe _sbrk() that checks for heap/stack collision and sets ENOMEM.
 *   - Tracking of the current heap break and its high-water mark.
 *   - Helper functions for reading memory statistics at runtime (used by the
 *     RAM diagnostics view in spectrum.c).
 *
 * Linker symbols used:
 *   _end    — first byte after .bss; the heap begins here.
 *   _estack — top of RAM (= ORIGIN(RAM) + LENGTH(RAM)); initial stack pointer.
 *   _sdata  — start of .data section in RAM.
 *   _ebss   — end of .bss section in RAM.
 */

#include "app/mem_stats.h"

#include <errno.h>
#include <stdint.h>

/* Linker-provided symbols.  Their *addresses* are the values we need. */
extern char    _end;     /* heap base: first byte after .bss            */
extern char    _estack;  /* top of RAM / initial stack pointer           */
extern uint8_t _sdata;   /* start of .data in RAM                       */
extern uint8_t _ebss;    /* end of .bss in RAM                          */

/* Safety margin kept between heap break and stack pointer (bytes). */
#define HEAP_STACK_GUARD  64u

/* Total RAM size — must match MEMORY { RAM } LENGTH in the linker script. */
#define RAM_TOTAL_BYTES   (16u * 1024u)

/* ---- internal state ---------------------------------------------------- */

static char *heap_ptr  = NULL;  /* current heap break                       */
static char *heap_peak = NULL;  /* high-water mark                          */

/* ---- _sbrk ------------------------------------------------------------- */

caddr_t _sbrk(int incr)
{
    if (heap_ptr == NULL) {
        heap_ptr  = &_end;
        heap_peak = &_end;
    }

    char *new_ptr = heap_ptr + incr;

    /* Read the current stack pointer into a local variable. */
    register char *sp asm("sp");

    /*
     * Reject the allocation if the new heap break would intrude on the stack
     * region (including the guard).  Use min(sp, _estack) as the limit —
     * normally sp < _estack (stack grows downward from _estack), but cap at
     * _estack in case sp is somehow invalid.
     */
    char *stack_limit = (sp > &_estack) ? &_estack : sp;

    if (new_ptr + (int)HEAP_STACK_GUARD >= stack_limit) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    char *prev = heap_ptr;
    heap_ptr   = new_ptr;

    if (heap_ptr > heap_peak) {
        heap_peak = heap_ptr;
    }

    return (caddr_t)prev;
}

/* ---- statistics accessors ---------------------------------------------- */

uint32_t MemStats_GetHeapStart(void)
{
    return (uint32_t)(uintptr_t)&_end;
}

uint32_t MemStats_GetHeapCurrent(void)
{
    if (heap_ptr == NULL) {
        return (uint32_t)(uintptr_t)&_end;
    }
    return (uint32_t)(uintptr_t)heap_ptr;
}

uint32_t MemStats_GetHeapUsed(void)
{
    if (heap_ptr == NULL) {
        return 0u;
    }
    return (uint32_t)(heap_ptr - &_end);
}

uint32_t MemStats_GetHeapPeak(void)
{
    if (heap_peak == NULL) {
        return 0u;
    }
    return (uint32_t)(heap_peak - &_end);
}

uint32_t MemStats_GetStackPtr(void)
{
    register char *sp asm("sp");
    return (uint32_t)(uintptr_t)sp;
}

uint32_t MemStats_GetFreeGap(void)
{
    register char *sp asm("sp");
    char *brk = (heap_ptr != NULL) ? heap_ptr : &_end;
    if (sp <= brk) {
        return 0u;
    }
    return (uint32_t)(sp - brk);
}

uint32_t MemStats_GetStaticRAM(void)
{
    return (uint32_t)((uintptr_t)&_ebss - (uintptr_t)&_sdata);
}

uint32_t MemStats_GetRAMTotal(void)
{
    return RAM_TOTAL_BYTES;
}
