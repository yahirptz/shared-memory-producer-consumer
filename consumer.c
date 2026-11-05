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
    if (argc != 3) {
        fprintf(stderr, "Usage: consumer <shm_name> <buffer_size>\n");
        return EXIT_FAILURE;
    }
    const char *shm_name = argv[1];
    const int capacity = atoi(argv[2]);  /* Process-local config. */
    int fd = shm_open(shm_name, O_RDWR, 0);
    if (fd < 0) { perror("consumer shm_open"); return EXIT_FAILURE; }
    size_t bytes = shared_bytes(capacity);
    SharedMemory *shared = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, 0);
    close(fd);
    if (shared == MAP_FAILED) { perror("consumer mmap"); return EXIT_FAILURE; }

    while (1) {
        /* CHECK then READ then UPDATE are deliberately not atomic. */
        int observed_count = shared->count;
        if (observed_count == 0) {
            if (shared->production_done) break;
            sched_yield();  /* Retry when buffer appears empty. */
            continue;
        }
        if (observed_count < 0 || observed_count > capacity) {
            sched_yield();
            continue;
        }

        int slot = shared->head;
        if (slot < 0 || slot >= capacity) slot = 0;
        int value = shared->buffer[slot];
        shared->head = (slot + 1) % capacity;
        shared->count = observed_count - 1;
        /* One write per value, to avoid interleaving buffered printf output. */
        if (dprintf(STDOUT_FILENO, "%d\n", value) < 0) {
            perror("consumer dprintf");
            munmap(shared, bytes);
            return EXIT_FAILURE;
        }
    }

    munmap(shared, bytes);
    return EXIT_SUCCESS;
}
