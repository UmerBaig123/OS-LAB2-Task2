// sieve_pipe.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

typedef struct {
    int *numbers;
    int count;
} NumberList;

/* Helper: robust write (handles partial writes) */
ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = buf;
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

/* Helper: robust read (handles partial reads) */
ssize_t read_all(int fd, void *buf, size_t count) {
    char *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) {
            // EOF
            return (ssize_t)(count - left);
        }
        left -= (size_t)r;
        p += r;
    }
    return (ssize_t)count;
}

int send_list_fd(int fd, NumberList *list) {
    if (write_all(fd, &list->count, sizeof(int)) != sizeof(int)) return -1;
    if (list->count > 0) {
        if (write_all(fd, list->numbers, list->count * sizeof(int)) != (ssize_t)(list->count * sizeof(int)))
            return -1;
    }
    return 0;
}

int recv_list_fd(int fd, NumberList *list) {
    int cnt;
    ssize_t rd = read_all(fd, &cnt, sizeof(int));
    if (rd <= 0) return -1; // EOF or error
    list->count = cnt;
    if (list->count > 0) {
        list->numbers = realloc(list->numbers, list->count * sizeof(int));
        if (!list->numbers) return -1;
        rd = read_all(fd, list->numbers, list->count * sizeof(int));
        if (rd != (ssize_t)(list->count * sizeof(int))) return -1;
    }
    return 0;
}

/* Filter out multiples of prime from input (input contains only the "remaining" numbers,
   i.e., first element is not part of this list when called). */
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

/* Child routine: read full list from read_fd, treat first element as prime,
   filter the rest with that prime, write filtered list to write_fd, exit. */
void child_stage(int read_fd, int write_fd) {
    NumberList in;
    in.numbers = malloc(1); in.count = 0;

    if (recv_list_fd(read_fd, &in) != 0) {
        // no data or error -> send empty and exit
        NumberList empty = { NULL, 0 };
        send_list_fd(write_fd, &empty);
        free(in.numbers);
        exit(0);
    }

    if (in.count == 0) {
        NumberList empty = { NULL, 0 };
        send_list_fd(write_fd, &empty);
        free(in.numbers);
        exit(0);
    }

    int prime = in.numbers[0];

    // Build remaining: skip first element
    NumberList remaining;
    remaining.count = in.count - 1;
    remaining.numbers = NULL;
    if (remaining.count > 0) {
        remaining.numbers = malloc(remaining.count * sizeof(int));
        for (int i = 1; i < in.count; ++i) remaining.numbers[i-1] = in.numbers[i];
    }

    NumberList *filtered = filter_numbers(&remaining, prime);

    // Send filtered to parent
    send_list_fd(write_fd, filtered);

    // cleanup
    free(in.numbers);
    if (remaining.numbers) free(remaining.numbers);
    if (filtered) {
        if (filtered->numbers) free(filtered->numbers);
        free(filtered);
    }

    exit(0);
}

void sieve_pipe(int n) {
    printf("=== Pipe-based Sequential Sieve ===\n");
    printf("Finding primes from 2 to %d\n", n);
    printf("Prime numbers found:\n");

    NumberList *current = malloc(sizeof(NumberList));
    current->count = n - 1;
    current->numbers = malloc(current->count * sizeof(int));
    for (int i = 0; i < current->count; ++i) current->numbers[i] = 2 + i;

    int stage = 0;
    int printed = 0;

    while (current->count > 0) {
        int prime = current->numbers[0];
        printf("%d ", prime);
        printed++;
        if (printed % 10 == 0) printf("\n");

        if (current->count == 1) break;

        // Create two pipes: parent->child (p_in), child->parent (p_out)
        int p_in[2];
        int p_out[2];
        if (pipe(p_in) == -1 || pipe(p_out) == -1) {
            perror("pipe");
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // Child: will read from p_in[0], write to p_out[1]
            close(p_in[1]);   // close write end of parent->child
            close(p_out[0]);  // close read end of child->parent

            child_stage(p_in[0], p_out[1]);

            // child_stage exits
        } else {
            // Parent: write to p_in[1], read from p_out[0]
            close(p_in[0]);   // close read end of parent->child
            close(p_out[1]);  // close write end of child->parent

            // Send current list to child
            if (send_list_fd(p_in[1], current) != 0) {
                // failed to write
                close(p_in[1]);
                close(p_out[0]);
                waitpid(pid, NULL, 0);
                break;
            }
            close(p_in[1]); // done writing -> important for EOF in child

            // Wait for child to finish producing filtered list
            waitpid(pid, NULL, 0);

            NumberList *next = malloc(sizeof(NumberList));
            next->numbers = malloc(1);
            next->count = 0;

            if (recv_list_fd(p_out[0], next) != 0) {
                // error or EOF -> stop
                close(p_out[0]);
                free(next->numbers);
                free(next);
                break;
            }

            close(p_out[0]);

            // Cleanup old current
            free(current->numbers);
            free(current);

            current = next;
            stage++;
        }
    }

    printf("\nTotal primes found: (printed approximate) \n");
    // final cleanup
    if (current) {
        if (current->numbers) free(current->numbers);
        free(current);
    }
}

int main(void) {
    int n = 1000;
    sieve_pipe(n);
    return 0;
}
