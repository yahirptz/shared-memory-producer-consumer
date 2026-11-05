#define _GNU_SOURCE
#include "shared.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __linux__
#include <sched.h>
#endif

static void usage(const char *name) {
    fprintf(stderr,
            "Usage: %s <buffer_size> <num_producers> <num_consumers> <upper_limit>\n"
            "  buffer_size, num_producers, num_consumers: positive integers\n"
            "  upper_limit: integer from 0 through %d\n",
            name, INT_MAX - 1);
}

static int parse_int(const char *text, int minimum, int maximum, int *out) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' ||
        value < minimum || value > maximum) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static double timeval_seconds(struct timeval value) {
    return (double)value.tv_sec + (double)value.tv_usec / 1000000.0;
}

static double usage_seconds(const struct rusage *usage) {
    return timeval_seconds(usage->ru_utime) + timeval_seconds(usage->ru_stime);
}

static double elapsed_seconds(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

static long available_cores(void) {
#ifdef __linux__
    cpu_set_t allowed;
    if (sched_getaffinity(0, sizeof allowed, &allowed) == 0) {
        int count = CPU_COUNT(&allowed);
        if (count > 0) return count;
    }
#endif
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? count : 1;
}

static int wait_and_measure(pid_t pid, double *child_cpu_seconds) {
    int status = 0;
    struct rusage used;
    pid_t result;
    do {
        result = wait4(pid, &status, 0, &used);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        perror("wait4");
        return -1;
    }
    *child_cpu_seconds += usage_seconds(&used);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Child PID %ld failed (status %d).\n", (long)pid, status);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int capacity, producers, consumers, upper_limit;
    if (argc != 5 ||
        parse_int(argv[1], 1, INT_MAX, &capacity) != 0 ||
        parse_int(argv[2], 1, INT_MAX, &producers) != 0 ||
        parse_int(argv[3], 1, INT_MAX, &consumers) != 0 ||
        parse_int(argv[4], 0, INT_MAX - 1, &upper_limit) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if ((size_t)capacity > (SIZE_MAX - sizeof(SharedMemory)) / sizeof(int)) {
        fprintf(stderr, "Buffer size is too large.\n");
        return EXIT_FAILURE;
    }
    const size_t mapping_bytes = shared_bytes(capacity);
    if (mapping_bytes > LONG_MAX) {
        fprintf(stderr, "Shared-memory region is too large.\n");
        return EXIT_FAILURE;
    }

    char shm_name[80];
    snprintf(shm_name, sizeof shm_name, "/cs425_pc_%ld", (long)getpid());
    int fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) { perror("shm_open"); return EXIT_FAILURE; }
    if (ftruncate(fd, (off_t)mapping_bytes) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(shm_name);
        return EXIT_FAILURE;
    }
    SharedMemory *shared = mmap(NULL, mapping_bytes,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shared == MAP_FAILED) {
        perror("mmap");
        shm_unlink(shm_name);
        return EXIT_FAILURE;
    }
    memset(shared, 0, mapping_bytes);

    pid_t *pids_p = calloc((size_t)producers, sizeof *pids_p);
    pid_t *pids_c = calloc((size_t)consumers, sizeof *pids_c);
    if (!pids_p || !pids_c) {
        perror("calloc");
        free(pids_p); free(pids_c);
        munmap(shared, mapping_bytes);
        shm_unlink(shm_name);
        return EXIT_FAILURE;
    }

    struct rusage self_start, self_end;
    struct timespec start, end;
    if (getrusage(RUSAGE_SELF, &self_start) == -1 ||
        clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime/getrusage");
        free(pids_p); free(pids_c);
        munmap(shared, mapping_bytes); shm_unlink(shm_name);
        return EXIT_FAILURE;
    }

    int launched_p = 0, launched_c = 0, failed = 0;
    for (int i = 0; i < producers; ++i) {
        pid_t pid = fork();
        if (pid == -1) { perror("fork producer"); failed = 1; break; }
        if (pid == 0) {
            execlp("./producer", "producer", shm_name, argv[1], argv[4], (char *)NULL);
            perror("execlp producer");
            _exit(127);
        }
        pids_p[launched_p++] = pid;
    }
    if (!failed) {
        for (int i = 0; i < consumers; ++i) {
            pid_t pid = fork();
            if (pid == -1) { perror("fork consumer"); failed = 1; break; }
            if (pid == 0) {
                execlp("./consumer", "consumer", shm_name, argv[1], (char *)NULL);
                perror("execlp consumer");
                _exit(127);
            }
            pids_c[launched_c++] = pid;
        }
    }
    if (failed) {  /* Failure cleanup is not a coordination mechanism. */
        for (int i = 0; i < launched_p; ++i) kill(pids_p[i], SIGTERM);
        for (int i = 0; i < launched_c; ++i) kill(pids_c[i], SIGTERM);
    }

    double child_cpu_seconds = 0.0;
    for (int i = 0; i < launched_p; ++i) {
        if (wait_and_measure(pids_p[i], &child_cpu_seconds) != 0) failed = 1;
    }
    /* Only the parent changes this flag, after every producer has exited. */
    shared->production_done = 1;
    for (int i = 0; i < launched_c; ++i) {
        if (wait_and_measure(pids_c[i], &child_cpu_seconds) != 0) failed = 1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1 ||
        getrusage(RUSAGE_SELF, &self_end) == -1) {
        perror("clock_gettime/getrusage");
        failed = 1;
        end = start;
        self_end = self_start;
    }
    double seconds = elapsed_seconds(start, end);
    double parent_cpu_seconds = usage_seconds(&self_end) - usage_seconds(&self_start);
    double total_cpu_seconds = parent_cpu_seconds + child_cpu_seconds;
    long cores = available_cores();
    double cpu_percent = seconds > 0.0 ?
        100.0 * total_cpu_seconds / ((double)cores * seconds) : 0.0;

    /* stderr keeps numeric consumer output on stdout for race checks. */
    fprintf(stderr,
            "METRICS elapsed_seconds=%.6f cpu_percent=%.6f cores=%ld "
            "total_cpu_seconds=%.6f final_count=%d failed=%d\n",
            seconds, cpu_percent, cores, total_cpu_seconds,
            shared->count, failed);

    free(pids_p); free(pids_c);
    munmap(shared, mapping_bytes);
    shm_unlink(shm_name);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
