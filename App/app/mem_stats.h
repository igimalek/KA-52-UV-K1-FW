#ifndef MEM_STATS_H
#define MEM_STATS_H

#include <stdint.h>
#include <sys/types.h>

/*
 * _sbrk — heap syscall for newlib malloc.
 *
 * Uses linker symbols to locate the heap base (_end) and the top of RAM
 * (_estack). Detects heap/stack collision, sets errno = ENOMEM and returns
 * (caddr_t)-1 when the allocation would intrude on the stack region.
 * Tracks current heap break and high-water mark so that statistics can be
 * queried at runtime.
 */
caddr_t _sbrk(int incr);

/* Return the address where the heap begins (linker symbol _end). */
uint32_t MemStats_GetHeapStart(void);

/* Return the current heap break address (next byte that would be allocated). */
uint32_t MemStats_GetHeapCurrent(void);

/* Return the number of bytes currently consumed by the heap since its start. */
uint32_t MemStats_GetHeapUsed(void);

/* Return the peak (high-water mark) heap usage in bytes. */
uint32_t MemStats_GetHeapPeak(void);

/* Return the current stack pointer value. */
uint32_t MemStats_GetStackPtr(void);

/* Return the free gap in bytes between the current heap break and the stack pointer. */
uint32_t MemStats_GetFreeGap(void);

/* Return the size of static RAM (.data + .bss) in bytes. */
uint32_t MemStats_GetStaticRAM(void);

/* Return the total RAM size in bytes (16 KB for this device). */
uint32_t MemStats_GetRAMTotal(void);

#endif /* MEM_STATS_H */
