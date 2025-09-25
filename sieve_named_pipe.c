#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define MAX_PIPES 100
#define PIPE_NAME_PREFIX "/tmp/sieve_pipe_"

typedef struct {
    int *numbers;
    int count;
} NumberList;

// Create unique named pipe
char* create_named_pipe(int stage) {
    char *pipe_name = (char*)malloc(50);
    sprintf(pipe_name, "%s%d", PIPE_NAME_PREFIX, stage);
    unlink(pipe_name); // remove if exists
    if (mkfifo(pipe_name, 0666) == -1) {
        perror("mkfifo");
        exit(1);
    }
    return pipe_name;
}

// Read numbers from already-open fd
int read_numbers_from_fd(int fd, NumberList *list) {
    if (read(fd, &list->count, sizeof(int)) <= 0) {
        return 0;
    }
    if (list->count > 0) {
        list->numbers = realloc(list->numbers, list->count * sizeof(int));
        int bytes = read(fd, list->numbers, list->count * sizeof(int));
        if (bytes <= 0) return 0;
    }
    return 1;
}

// Write numbers to already-open fd
void write_numbers_to_fd(int fd, NumberList *list) {
    write(fd, &list->count, sizeof(int));
    if (list->count > 0) {
        write(fd, list->numbers, list->count * sizeof(int));
    }
}

// Filter out multiples of prime
NumberList* filter_numbers(NumberList *input, int prime) {
    NumberList *output = malloc(sizeof(NumberList));
    output->numbers = malloc(input->count * sizeof(int));
    output->count = 0;

    for (int i = 0; i < input->count; i++) {
        if (input->numbers[i] % prime != 0) {
            output->numbers[output->count++] = input->numbers[i];
        }
    }
    return output;
}

// Filtering stage
void filter_process(int fd_in, int fd_out) {
    NumberList input_list = { malloc(1000 * sizeof(int)), 0 };

    if (read_numbers_from_fd(fd_in, &input_list)) {
        if (input_list.count > 0) {
            int prime = input_list.numbers[0];   // parent prints this, not us

            // Build remaining list without the prime
            NumberList remaining;
            remaining.count = input_list.count - 1;
            remaining.numbers = malloc(remaining.count * sizeof(int));
            for (int i = 1; i < input_list.count; i++) {
                remaining.numbers[i - 1] = input_list.numbers[i];
            }

            // Filter out multiples of prime
            NumberList *filtered = filter_numbers(&remaining, prime);

            // Send back only the filtered list (prime removed!)
            write_numbers_to_fd(fd_out, filtered);

            free(remaining.numbers);
            free(filtered->numbers);
            free(filtered);
        } else {
            // Nothing left
            NumberList empty = { NULL, 0 };
            write_numbers_to_fd(fd_out, &empty);
        }
    }

    free(input_list.numbers);
}

void sieve_named_pipe(int n) {
    printf("=== Sequential Filter Sieve (Fork and Named Pipes) ===\n");
    printf("Finding primes from 2 to %d\n", n);
    printf("Prime numbers found:\n");

    // Initial list
    NumberList *initial = malloc(sizeof(NumberList));
    initial->numbers = malloc((n - 1) * sizeof(int));
    initial->count = 0;
    for (int i = 2; i <= n; i++) {
        initial->numbers[initial->count++] = i;
    }

    int stage = 0;
    int prime_count = 0;
    NumberList *current_list = initial;

    while (current_list->count > 0) {
        int prime = current_list->numbers[0];
        printf("%d ", prime);
        prime_count++;
        if (prime_count % 10 == 0) printf("\n");

        if (current_list->count == 1) break;

        // Create pipes
        char *input_pipe = create_named_pipe(stage * 2);
        char *output_pipe = create_named_pipe(stage * 2 + 1);

        int fd_in = open(input_pipe, O_RDWR);
        int fd_out = open(output_pipe, O_RDWR);
        if (fd_in == -1 || fd_out == -1) {
            perror("open");
            exit(1);
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child: filter stage
            filter_process(fd_in, fd_out);
            close(fd_in);
            close(fd_out);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }

        // Parent: write input, wait, then read output
        write_numbers_to_fd(fd_in, current_list);

        waitpid(pid, NULL, 0);

        NumberList *next_list = malloc(sizeof(NumberList));
        next_list->numbers = malloc(1000 * sizeof(int));
        next_list->count = 0;

        if (!read_numbers_from_fd(fd_out, next_list)) {
            free(next_list->numbers);
            free(next_list);
            break;
        }

        // Cleanup
        if (current_list != initial) {
            free(current_list->numbers);
            free(current_list);
        }

        close(fd_in);
        close(fd_out);
        unlink(input_pipe);
        unlink(output_pipe);
        free(input_pipe);
        free(output_pipe);

        current_list = next_list;
        stage++;
    }

    printf("\nTotal prime numbers found: %d\n", prime_count);
    printf("Total filtering stages: %d\n", stage);

    if (current_list != initial) {
        free(current_list->numbers);
        free(current_list);
    }
    free(initial->numbers);
    free(initial);
}

void cleanup_pipes() {
    char pipe_name[50];
    for (int i = 0; i < MAX_PIPES; i++) {
        sprintf(pipe_name, "%s%d", PIPE_NAME_PREFIX, i);
        unlink(pipe_name);
    }
}

int main() {
    int n = 1000;
    clock_t start, end;

    cleanup_pipes();

    printf("=== Named Pipe Sieve Implementation ===\n");
    printf("Using fork() and named pipes (FIFOs)\n");
    printf("Finding primes from 2 to %d\n\n", n);

    start = clock();
    sieve_named_pipe(n);
    end = clock();

    double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\nTime taken: %f seconds\n", cpu_time_used);

    cleanup_pipes();
    return 0;
}
