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
            // First number is prime
            int prime = input_list->numbers[0];
            printf("%d ", prime);
            fflush(stdout);
             
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
    
    int pipe_in[2], pipe_out[2];
    pid_t pid;
    int prime_count = 0;
    
    // Create initial pipe
    if (pipe(pipe_in) == -1) {
        perror("pipe");
        exit(1);
    }
    
    // Send initial list to first process
    write_numbers_to_pipe(pipe_in[1], initial);
    close(pipe_in[1]);
    
    NumberList *current_list = (NumberList*)malloc(sizeof(NumberList));
    current_list->numbers = (int*)malloc(1000 * sizeof(int));
    
    while (1) {
        // Read from current pipe
        if (!read_numbers_from_pipe(pipe_in[0], current_list) || current_list->count == 0) {
            break;
        }
        
        // Print the first prime
        int prime = current_list->numbers[0];
        printf("%d ", prime);
        prime_count++;
        if (prime_count % 10 == 0) printf("\n");
        
        // If only one number left, we're done
        if (current_list->count == 1) {
            break;
        }
        
        // Create pipe for next stage
        if (pipe(pipe_out) == -1) {
            perror("pipe");
            exit(1);
        }
        
        pid = fork();
        if (pid == 0) {
            close(pipe_out[0]);
            
            NumberList *remaining = (NumberList*)malloc(sizeof(NumberList));
            remaining->numbers = (int*)malloc(current_list->count * sizeof(int));
            remaining->count = current_list->count - 1;
            
            for (int i = 1; i < current_list->count; i++) {
                remaining->numbers[i-1] = current_list->numbers[i];
            }
            
            NumberList *filtered = filter_numbers(remaining, prime);
            
            write_numbers_to_pipe(pipe_out[1], filtered);
            
            free(remaining->numbers);
            free(remaining);
            free(filtered->numbers);
            free(filtered);
            close(pipe_out[1]);
            exit(0);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }
        
        close(pipe_in[0]); 
        close(pipe_out[1]);
        waitpid(pid, NULL, 0);
        
        pipe_in[0] = pipe_out[0];
    }
    
    printf("\nTotal prime numbers found: %d\n", prime_count);
    
    free(initial->numbers);
    free(initial);
    free(current_list->numbers);
    free(current_list);
    close(pipe_in[0]);
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