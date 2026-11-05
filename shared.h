#ifndef SHARED_H
#define SHARED_H

#include <stddef.h>
#include <stdint.h>

/*
 * IMPORTANT: volatile only forces ordinary memory reads/writes here.
 * It does NOT make the read/check/write sequences atomic and it does
 * NOT fix races.  The assignment explicitly prohibits synchronization.
 */
typedef struct {
    volatile int count;              /* Believed number of buffered items */
    volatile int head;               /* Next location to read */
    volatile int tail;               /* Next location to write */
    volatile int next_value;         /* Next integer producers try to produce */
    volatile int production_done;    /* Parent sets after all producers exit */
    int buffer[];                    /* Flexible array: capacity integers */
} SharedMemory;

static inline size_t shared_bytes(int capacity) {
    return sizeof(SharedMemory) + (size_t)capacity * sizeof(int);
}

#endif
