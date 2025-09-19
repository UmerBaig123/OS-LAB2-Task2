#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
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

// Function to create a unique named pipe
char* create_named_pipe(int stage) {
    char *pipe_name = (char*)malloc(50);
    sprintf(pipe_name, "%s%d", PIPE_NAME_PREFIX, stage);
    
    // Remove existing pipe if it exists
    unlink(pipe_name);
    
    // Create named pipe
    if (mkfifo(pipe_name, 0666) == -1) {
        perror("mkfifo");
        exit(1);
    }
    
    return pipe_name;
}

// Function to read numbers from named pipe
int read_numbers_from_named_pipe(const char *pipe_name, NumberList *list) {
    int fd = open(pipe_name, O_RDONLY);
    if (fd == -1) {
        perror("open read");
        return 0;
    }
    
    // Read count first
    if (read(fd, &list->count, sizeof(int)) <= 0) {
        close(fd);
        return 0;
    }
    
    if (list->count <= 0) {
        close(fd);
        return 0; // Empty list signal
    }
    
    // Read the actual numbers
    int bytes_read = read(fd, list->numbers, list->count * sizeof(int));
    close(fd);
    return bytes_read > 0;
}

// Function to write numbers to named pipe
void write_numbers_to_named_pipe(const char *pipe_name, NumberList *list) {
    int fd = open(pipe_name, O_WRONLY);
    if (fd == -1) {
        perror("open write");
        exit(1);
    }
    
    // Write count first
    write(fd, &list->count, sizeof(int));
    
    // Write numbers if any
    if (list->count > 0) {
        write(fd, list->numbers, list->count * sizeof(int));
    }
    
    close(fd);
}

// Filter function - removes multiples of prime
NumberList* filter_numbers(NumberList *input, int prime) {
    NumberList *output = (NumberList*)malloc(sizeof(NumberList));
    output->numbers = (int*)malloc(input->count * sizeof(int));
    output->count = 0;
    
    for (int i = 0; i < input->count; i++) {
        if (input->numbers[i] % prime != 0) {
            output->numbers[output->count++] = input->numbers[i];
        }
    }
    
    return output;
}

// Process function for filtering stage
void filter_process(const char *input_pipe, const char *output_pipe, int stage) {
    NumberList *input_list = (NumberList*)malloc(sizeof(NumberList));
    input_list->numbers = (int*)malloc(1000 * sizeof(int));
    
    // Read from input pipe
    if (read_numbers_from_named_pipe(input_pipe, input_list)) {
        if (input_list->count > 0) {
            // First number is prime
            int prime = input_list->numbers[0];
            printf("Stage %d: Prime found = %d\n", stage, prime);
            fflush(stdout);
            
            // Create list with remaining numbers (skip first)
            NumberList *remaining = (NumberList*)malloc(sizeof(NumberList));
            remaining->numbers = (int*)malloc(input_list->count * sizeof(int));
            remaining->count = input_list->count - 1;
            
            for (int i = 1; i < input_list->count; i++) {
                remaining->numbers[i-1] = input_list->numbers[i];
            }
            
            // Filter out multiples of prime
            NumberList *filtered = filter_numbers(remaining, prime);
            
            printf("Stage %d: Filtered %d numbers, %d remaining\n", 
                   stage, remaining->count - filtered->count, filtered->count);
            
            // Write filtered list to output pipe
            write_numbers_to_named_pipe(output_pipe, filtered);
            
            free(remaining->numbers);
            free(remaining);
            free(filtered->numbers);
            free(filtered);
        } else {
            // Empty list - write empty list to output
            NumberList empty = {NULL, 0};
            write_numbers_to_named_pipe(output_pipe, &empty);
        }
    }
    
    free(input_list->numbers);
    free(input_list);
    
    // Clean up input pipe
    unlink(input_pipe);
}

void sieve_named_pipe(int n) {
    printf("=== Sequential Filter Sieve (Fork and Named Pipes) ===\n");
    printf("Finding primes from 2 to %d\n", n);
    printf("Prime numbers found:\n");
    
    // Initialize the first list with numbers from 2 to n
    NumberList *initial = (NumberList*)malloc(sizeof(NumberList));
    initial->numbers = (int*)malloc((n - 1) * sizeof(int));
    initial->count = 0;
    
    for (int i = 2; i <= n; i++) {
        initial->numbers[initial->count++] = i;
    }
    
    // Create first named pipe and write initial data
    char *current_pipe = create_named_pipe(0);
    write_numbers_to_named_pipe(current_pipe, initial);
    
    int stage = 0;
    int prime_count = 0;
    pid_t *child_pids = (pid_t*)malloc(MAX_PIPES * sizeof(pid_t));
    int child_count = 0;
    
    while (1) {
        // Read from current pipe
        NumberList *current_list = (NumberList*)malloc(sizeof(NumberList));
        current_list->numbers = (int*)malloc(1000 * sizeof(int));
        
        if (!read_numbers_from_named_pipe(current_pipe, current_list) || current_list->count == 0) {
            free(current_list->numbers);
            free(current_list);
            break;
        }
        
        // Get the first prime
        int prime = current_list->numbers[0];
        printf("%d ", prime);
        prime_count++;
        if (prime_count % 10 == 0) printf("\n");
        
        // If only one number left, we're done
        if (current_list->count == 1) {
            free(current_list->numbers);
            free(current_list);
            break;
        }
        
        // Create next stage pipe
        char *next_pipe = create_named_pipe(stage + 1);
        
        // Fork a process to handle this filtering stage
        pid_t pid = fork();
        if (pid == 0) {
            // Child process - filter the numbers
            filter_process(current_pipe, next_pipe, stage);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        } else {
            // Parent process - store child PID
            child_pids[child_count++] = pid;
        }
        
        // Move to next stage
        free(current_list->numbers);
        free(current_list);
        free(current_pipe);
        current_pipe = next_pipe;
        stage++;
        
        // Wait for current child to finish before proceeding
        waitpid(pid, NULL, 0);
    }
    
    printf("\nTotal prime numbers found: %d\n", prime_count);
    printf("Total filtering stages: %d\n", stage);
    
    // Clean up remaining pipe
    unlink(current_pipe);
    free(current_pipe);
    
    // Wait for any remaining child processes
    for (int i = 0; i < child_count; i++) {
        int status;
        waitpid(child_pids[i], &status, WNOHANG);
    }
    
    free(child_pids);
    free(initial->numbers);
    free(initial);
}

// Cleanup function to remove any leftover named pipes
void cleanup_pipes() {
    char pipe_name[50];
    for (int i = 0; i < MAX_PIPES; i++) {
        sprintf(pipe_name, "%s%d", PIPE_NAME_PREFIX, i);
        unlink(pipe_name); // Remove if exists (ignore errors)
    }
}

int main() {
    int n = 1000; // Fixed to 1000 as per requirement
    clock_t start, end;
    double cpu_time_used;
    
    // Cleanup any existing pipes
    cleanup_pipes();
    
    printf("=== Named Pipe Sieve Implementation ===\n");
    printf("Using fork() and named pipes (FIFOs)\n");
    printf("Finding primes from 2 to %d\n\n", n);
    
    start = clock();
    sieve_named_pipe(n);
    end = clock();
    
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("\nTime taken: %f seconds\n", cpu_time_used);
    
    // Final cleanup
    cleanup_pipes();
    
    return 0;
}