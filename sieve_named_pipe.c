#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_NUM 1000

// Send numbers to a pipe
void write_numbers(int fd, int *nums, int count) {
    write(fd, nums, count * sizeof(int));
}

// Read numbers from a pipe
int read_numbers(int fd, int *nums, int max_count) {
    return read(fd, nums, max_count * sizeof(int)) / sizeof(int);
}

// Filtering process
void filter(int fd_in) {
    int nums[MAX_NUM];
    int count = read_numbers(fd_in, nums, MAX_NUM);

    if (count == 0) exit(0);

    int prime = nums[0];
    printf("%d ", prime);
    fflush(stdout);

    // Build filtered list
    int filtered[MAX_NUM];
    int fcount = 0;
    for (int i = 1; i < count; i++) {
        if (nums[i] % prime != 0) {
            filtered[fcount++] = nums[i];
        }
    }

    if (fcount == 0) exit(0);

    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[1]);
        filter(pipefd[0]);
        exit(0);
    } else {
        close(pipefd[0]);
        write_numbers(pipefd[1], filtered, fcount);
        close(pipefd[1]);
        wait(NULL);
    }
}

int main() {
    printf("=== Named Pipe Sieve Implementation ===\n");
    printf("Finding primes from 2 to %d\n", MAX_NUM);

    int nums[MAX_NUM - 1];
    for (int i = 2; i <= MAX_NUM; i++) {
        nums[i - 2] = i;
    }

    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[1]);
        filter(pipefd[0]);
        exit(0);
    } else {
        close(pipefd[0]);
        write_numbers(pipefd[1], nums, MAX_NUM - 1);
        close(pipefd[1]);
        wait(NULL);
        printf("\n");
    }

    return 0;
}
