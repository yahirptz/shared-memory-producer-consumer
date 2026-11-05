#define _POSIX_C_SOURCE 200809L
#include "shared.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sched.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: producer <shm_name> <buffer_size> <upper_limit>\n");
        return EXIT_FAILURE;
    }
    const char *shm_name = argv[1];
    const int capacity = atoi(argv[2]);   /* Passed by main through execlp. */
    const int upper_limit = atoi(argv[3]); /* Process-local config, not shared. */

    int fd = shm_open(shm_name, O_RDWR, 0);
    if (fd < 0) { perror("producer shm_open"); return EXIT_FAILURE; }
    size_t bytes = shared_bytes(capacity);
    SharedMemory *shared = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, 0);
    close(fd);
    if (shared == MAP_FAILED) { perror("producer mmap"); return EXIT_FAILURE; }

    while (1) {
        /* Each producer reads the SAME shared integer. */
        int value = shared->next_value;
        if (value > upper_limit) break;

        /* CHECK then WRITE are deliberately not atomic. */
        int observed_count = shared->count;
        if (observed_count == capacity) {
            sched_yield();  /* Retry when the bounded buffer appears full. */
            continue;
        }
        if (observed_count < 0 || observed_count > capacity) {
            sched_yield();
            continue;
        }

        int slot = shared->tail;
        if (slot < 0 || slot >= capacity) slot = 0;
        shared->buffer[slot] = value;
        shared->tail = (slot + 1) % capacity;
        shared->count = observed_count + 1;
        shared->next_value = value + 1;
        /* Two producers can use the same slot/value/count: intentional race. */
    }

    munmap(shared, bytes);
    return EXIT_SUCCESS;
}
