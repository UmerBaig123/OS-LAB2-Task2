// sieve_fixed.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>

typedef struct {
    int *numbers;
    int count;
} NumberList;

/* Robust write: ensure all bytes written (handles EINTR & partial writes) */
ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = (const char*)buf;
    size_t left = count;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= (size_t)w;
        p += w;
    }
    return (ssize_t)count;
}

/* Robust read: try to read exactly count bytes (returns bytes read or -1 on error) */
ssize_t read_all(int fd, void *buf, size_t count) {
    char *p = (char*)buf;
    size_t left = count;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) {
            // EOF reached
            return (ssize_t)(count - left);
        }
        left -= (size_t)r;
        p += r;
    }
    return (ssize_t)count;
}

/* Send a NumberList through fd: first int count, then array of ints */
int send_list(int fd, NumberList *list) {
    if (write_all(fd, &list->count, sizeof(int)) != sizeof(int)) return -1;
    if (list->count > 0) {
        size_t bytes = list->count * sizeof(int);
        if (write_all(fd, list->numbers, bytes) != (ssize_t)bytes) return -1;
    }
    return 0;
}

/* Receive a NumberList from fd; caller must ensure list->numbers points to buffer or NULL */
int recv_list(int fd, NumberList *list) {
    int cnt;
    ssize_t r = read_all(fd, &cnt, sizeof(int));
    if (r <= 0) return -1; // EOF or error
    list->count = cnt;
    if (list->count > 0) {
        int *buf = realloc(list->numbers, list->count * sizeof(int));
        if (!buf) return -1;
        list->numbers = buf;
        size_t bytes = list->count * sizeof(int);
        r = read_all(fd, list->numbers, bytes);
        if (r != (ssize_t)bytes) return -1;
    }
    return 0;
}

/* Filter helper: returns newly allocated NumberList (caller frees) */
NumberList *filter_numbers(NumberList *input, int prime) {
    NumberList *out = malloc(sizeof(NumberList));
    if (!out) return NULL;
    out->numbers = malloc(input->count * sizeof(int));
    out->count = 0;
    for (int i = 0; i < input->count; ++i) {
        if (input->numbers[i] % prime != 0) {
            out->numbers[out->count++] = input->numbers[i];
        }
    }
    return out;
}

void sieve_fork_pipe_sequential(int n) {
    printf("=== Pipe-based Sequential Sieve (Fixed) ===\n");
    printf("Finding primes from 2 to %d\n", n);
    printf("Prime numbers found:\n");

    // initial list 2..n
    NumberList *current = malloc(sizeof(NumberList));
    if (!current) { perror("malloc"); exit(1); }
    current->count = n - 1;
    current->numbers = malloc(current->count * sizeof(int));
    if (!current->numbers) { perror("malloc"); exit(1); }
    for (int i = 0; i < current->count; ++i) current->numbers[i] = 2 + i;

    int prime_count = 0;

    while (current->count > 0) {
        // Create pipes: parent->child (p2c) and child->parent (c2p)
        int p2c[2];
        int c2p[2];
        if (pipe(p2c) == -1) { perror("pipe p2c"); exit(1); }
        if (pipe(c2p) == -1) { perror("pipe c2p"); exit(1); }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            /* CHILD */
            // close unused ends
            close(p2c[1]); // parent write end
            close(c2p[0]); // parent read end

            // Child reads the list from parent
            NumberList in;
            in.numbers = malloc(1);
            in.count = 0;
            if (recv_list(p2c[0], &in) != 0) {
                // nothing to do
                free(in.numbers);
                close(p2c[0]);
                close(c2p[1]);
                _exit(0);
            }

            if (in.count <= 0) {
                free(in.numbers);
                close(p2c[0]);
                close(c2p[1]);
                _exit(0);
            }

            // The first number observed by child is its prime
            int prime = in.numbers[0];
            // Print prime exactly once here in the child
            printf("%d ", prime);
            fflush(stdout);

            // Build remaining list (skip first element)
            NumberList remaining;
            remaining.count = in.count - 1;
            remaining.numbers = NULL;
            if (remaining.count > 0) {
                remaining.numbers = malloc(remaining.count * sizeof(int));
                for (int i = 1; i < in.count; ++i) remaining.numbers[i-1] = in.numbers[i];
            }

            // Filter multiples of prime and send filtered list back to parent
            NumberList *filtered = filter_numbers(&remaining, prime);
            // Send filtered (may have count 0)
            send_list(c2p[1], filtered);

            // cleanup
            if (remaining.numbers) free(remaining.numbers);
            free(filtered->numbers);
            free(filtered);
            free(in.numbers);

            // close FDs and exit
            close(p2c[0]);
            close(c2p[1]);
            _exit(0);
        } else {
            /* PARENT */
            // close unused ends
            close(p2c[0]); // child read end
            close(c2p[1]); // child write end

            // Parent sends the current list to the child
            if (send_list(p2c[1], current) != 0) {
                fprintf(stderr, "Failed to send list to child\n");
                close(p2c[1]);
                close(c2p[0]);
                waitpid(pid, NULL, 0);
                break;
            }
            // done writing: close write end so child sees EOF if needed
            close(p2c[1]);

            // Wait for child to finish generating filtered list
            waitpid(pid, NULL, 0);

            // Parent reads filtered list from child
            NumberList next;
            next.numbers = malloc(1);
            next.count = 0;
            if (recv_list(c2p[0], &next) != 0) {
                // no more numbers (or error)
                free(next.numbers);
                close(c2p[0]);
                break;
            }
            close(c2p[0]);

            // Replace current with next (child removed the prime and filtered)
            free(current->numbers);
            free(current);
            current = malloc(sizeof(NumberList));
            current->count = next.count;
            current->numbers = next.numbers;

            prime_count++;
            // continue loop until current->count == 0
        }
    }

    printf("\nTotal primes found (approx): %d\n", prime_count);

    if (current) {
        if (current->numbers) free(current->numbers);
        free(current);
    }
}

int main(void) {
    int n = 1000; // fixed as requested

    clock_t start = clock();
    sieve_fork_pipe_sequential(n);
    clock_t end = clock();

    double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    return 0;
}
