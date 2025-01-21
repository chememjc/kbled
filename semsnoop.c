/* semsnoop diagnostic tool
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-21
 * 
 * Check status of semaphores and increment/decrement the value
 */


#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *prog_name) {
    printf("Usage: %s <sem_name> <action>\n", prog_name);
    printf("Actions:\n");
    printf("  inc - Increment the semaphore\n");
    printf("  dec - Decrement the semaphore\n");
    printf("  val - Display the current value of the semaphore\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *sem_name = argv[1];
    const char *action = argv[2];
    
    sem_t *sem = sem_open(sem_name, 0); // Open an existing semaphore
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    if (strcmp(action, "inc") == 0) {
        if (sem_post(sem) == -1) {
            perror("sem_post");
            exit(EXIT_FAILURE);
        }
        printf("Semaphore incremented successfully.\n");
    } else if (strcmp(action, "dec") == 0) {
        if (sem_wait(sem) == -1) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
        printf("Semaphore decremented successfully.\n");
    } else if (strcmp(action, "val") == 0) {
        int value;
        if (sem_getvalue(sem, &value) == -1) {
            perror("sem_getvalue");
            exit(EXIT_FAILURE);
        }
        printf("Semaphore value: %d\n", value);
    } else {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    sem_close(sem); // Close the semaphore
    return 0;
}
