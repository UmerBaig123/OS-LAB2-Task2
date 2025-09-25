#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

typedef struct {
    int *numbers;
    int count;
} NumberList;
 
int read_numbers_from_pipe(int pipe_fd, NumberList *list) {
    if (read(pipe_fd, &list->count, sizeof(int)) <= 0) {
        return 0; // No more data
    }
    
    if (list->count <= 0) {
        return 0;
    }
    int bytes_read = read(pipe_fd, list->numbers, list->count * sizeof(int));
    return bytes_read > 0;
}

// Function to write numbers to pipe
void write_numbers_to_pipe(int pipe_fd, NumberList *list) {
    write(pipe_fd, &list->count, sizeof(int));
    if (list->count > 0) {
        write(pipe_fd, list->numbers, list->count * sizeof(int));
    }
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
 
void process_stage(int input_pipe, int output_pipe) {
    NumberList *input_list = (NumberList*)malloc(sizeof(NumberList));
    input_list->numbers = (int*)malloc(1000 * sizeof(int));
    
    if (read_numbers_from_pipe(input_pipe, input_list)) {
        if (input_list->count > 0) {
            // First number is prime - but don't print here, let parent handle printing
            int prime = input_list->numbers[0];
             
            NumberList *remaining = (NumberList*)malloc(sizeof(NumberList));
            remaining->numbers = (int*)malloc(input_list->count * sizeof(int));
            remaining->count = input_list->count - 1;
            
            for (int i = 1; i < input_list->count; i++) {
                remaining->numbers[i-1] = input_list->numbers[i];
            }
             
            NumberList *filtered = filter_numbers(remaining, prime);
             
            write_numbers_to_pipe(output_pipe, filtered);
            
            free(remaining->numbers);
            free(remaining);
            free(filtered->numbers);
            free(filtered);
        }
    }
    
    free(input_list->numbers);
    free(input_list);
    close(input_pipe);
    close(output_pipe);
}

void sieve_fork_pipe_sequential(int n) {
    printf("Prime numbers up to %d:\n", n);
    
    // Initialize the first list with numbers from 2 to n
    NumberList *initial = (NumberList*)malloc(sizeof(NumberList));
    initial->numbers = (int*)malloc((n - 1) * sizeof(int));
    initial->count = 0;
    
    for (int i = 2; i <= n; i++) {
        initial->numbers[initial->count++] = i;
    }
    
    NumberList *current_list = initial;
    int prime_count = 0;
    
    while (current_list->count > 0) {
        // Get the first number as prime
        int prime = current_list->numbers[0];
        printf("%d ", prime);
        prime_count++;
        if (prime_count % 10 == 0) printf("\n");
        
        // If only one number left, we're done
        if (current_list->count == 1) {
            break;
        }
        
        // Create pipe for filtering
        int pipe_fd[2];
        if (pipe(pipe_fd) == -1) {
            perror("pipe");
            exit(1);
        }
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child process - filter the numbers
            close(pipe_fd[0]); // Close read end
            
            // Create remaining numbers (skip the prime)
            NumberList *remaining = (NumberList*)malloc(sizeof(NumberList));
            remaining->numbers = (int*)malloc(current_list->count * sizeof(int));
            remaining->count = current_list->count - 1;
            
            for (int i = 1; i < current_list->count; i++) {
                remaining->numbers[i-1] = current_list->numbers[i];
            }
            
            // Filter out multiples of prime
            NumberList *filtered = filter_numbers(remaining, prime);
            
            // Send filtered results back
            write_numbers_to_pipe(pipe_fd[1], filtered);
            
            free(remaining->numbers);
            free(remaining);
            free(filtered->numbers);
            free(filtered);
            close(pipe_fd[1]);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }
        
        // Parent process
        close(pipe_fd[1]); // Close write end
        waitpid(pid, NULL, 0); // Wait for child to complete
        
        // Read filtered results
        NumberList *new_list = (NumberList*)malloc(sizeof(NumberList));
        new_list->numbers = (int*)malloc(n * sizeof(int));
        
        if (read_numbers_from_pipe(pipe_fd[0], new_list) && new_list->count > 0) {
            // Free old list if it's not the initial one
            if (current_list != initial) {
                free(current_list->numbers);
                free(current_list);
            }
            current_list = new_list;
        } else {
            // No more numbers to process
            free(new_list->numbers);
            free(new_list);
            break;
        }
        
        close(pipe_fd[0]);
    }
    
    printf("\nTotal prime numbers found: %d\n", prime_count);
    
    // Clean up
    if (current_list == initial) {
        free(initial->numbers);
        free(initial);
    } else {
        free(initial->numbers);
        free(initial);
        free(current_list->numbers);
        free(current_list);
    }
}

int main() {
    int n = 1000; // Fixed to 1000 as per requirement
    clock_t start, end;
    double cpu_time_used;
    
    printf("=== Sequential Filter Sieve (Fork and Pipe) ===\n");
    printf("Finding primes from 2 to %d\n", n);
    
    start = clock();
    sieve_fork_pipe_sequential(n);
    end = clock();
    
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
    return 0;
}