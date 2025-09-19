#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

typedef struct {
    int *numbers;
    int count;
    int capacity;
} NumberList;

NumberList* create_list(int capacity) {
    NumberList *list = (NumberList*)malloc(sizeof(NumberList));
    list->numbers = (int*)malloc(capacity * sizeof(int));
    list->count = 0;
    list->capacity = capacity;
    return list;
}

void free_list(NumberList *list) {
    free(list->numbers);
    free(list);
}

NumberList* filter_numbers(NumberList *input, int prime) {
    NumberList *output = create_list(input->count);
    
    for (int i = 0; i < input->count; i++) {
        if (input->numbers[i] % prime != 0) {
            output->numbers[output->count++] = input->numbers[i];
        }
    }
    
    return output;
}

void sieve_sequential_filter(int n) {
    // Initialize list with numbers from 2 to n
    NumberList *current = create_list(n - 1);
    for (int i = 2; i <= n; i++) {
        current->numbers[current->count++] = i;
    }
    
    printf("Prime numbers up to %d:\n", n);
    int prime_count = 0;
    
    while (current->count > 0) {
        // Pick the first number (it's prime)
        int prime = current->numbers[0];
        printf("%d ", prime);
        prime_count++;
        if (prime_count % 10 == 0) printf("\n");
        
        // Create new list with remaining numbers (skip first element)
        NumberList *remaining = create_list(current->count - 1);
        for (int i = 1; i < current->count; i++) {
            remaining->numbers[remaining->count++] = current->numbers[i];
        }
        
        // Filter out multiples of the prime
        NumberList *filtered = filter_numbers(remaining, prime);
        
        // Clean up and move to next iteration
        free_list(current);
        free_list(remaining);
        current = filtered;
    }
    
    printf("\nTotal prime numbers found: %d\n", prime_count);
    free_list(current);
}

int main() {
    int n = 1000; // Fixed to 1000 as per requirement
    clock_t start, end;
    double cpu_time_used;
    
    printf("=== Sequential Filter Sieve (Iterative) ===\n");
    printf("Finding primes from 2 to %d\n", n);
    
    start = clock();
    sieve_sequential_filter(n);
    end = clock();
    
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time taken: %f seconds\n", cpu_time_used);
    
    return 0;
}