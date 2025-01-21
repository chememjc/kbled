/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * shared memory (daemon side) for dynamic state change.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include "sharedmem.h"

int shm_id;
struct shared_data *shm_ptr;
FILE *file;
sem_t *sem;

void get_executable_path(char *buffer, size_t size) {
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len == -1) {
        perror("readlink failed");
        exit(EXIT_FAILURE);
    }
    buffer[len] = '\0';  // Null-terminate the string
}

int sharedmem_init() {
    char exe_path[256];
    key_t shm_key;

    // Create the directory for the token file if it doesn't exist
    printf("Setup token file...\n");
    if (mkdir("/var/run", 0666) == -1 && errno != EEXIST) {
        perror("mkdir failed");
        exit(EXIT_FAILURE);
    }

    // Get the full path of the current executable
    get_executable_path(exe_path, sizeof(exe_path));

    // Generate a unique key using ftok
    shm_key = ftok(exe_path, PROJECT_ID);
    if (shm_key == -1) {
        perror("ftok failed");
        exit(EXIT_FAILURE);
    }

    // Write the key to the token file
    file = fopen(TOKEN_FILE, "w");
    if (!file) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "%d\n", shm_key);  // Save the key as an integer
    fclose(file);

    // Create the shared memory segment with 0666 permissions (readable and writable by all)
    printf("Create shared memeory segment (%li bytes)...\n",sizeof(struct shared_data));
    shm_id = shmget(shm_key, sizeof(struct shared_data), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    // Attach to the shared memory segment
    printf("Attach to shared memory segment...\n");
    shm_ptr = shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }

    // Create the semaphore for mutual exclusion
    printf("Create the semaphore...\n");
    mode_t old_umask = umask(0000); // change umask so the semaphore file is created with correct permissions
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);  // Initial value = 1 (mutex)
    umask(old_umask); // Restore original umask
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void sharedmem_lock(){
    // Wait (lock) the semaphore before accessing shared memory
    printf("locking...\n");
    sem_wait(sem);
    printf("semaphore locked\n");
}

void sharedmem_unlock(){
    // Signal (unlock) the semaphore after accessing shared memory
    printf("unlocking...\n");
    sem_post(sem);
    printf("semaphore unlocked\n");
}

int sharedmem_close() {
    //shutdown sequence
    // Detach from the shared memory segment
    printf("Detaching shared memory...\n");
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
        exit(EXIT_FAILURE);
    }
    printf("Shared memory detached\n");
    //remove shared memory segment
    printf("Removing shared memory segment...\n");
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl: error removing shared memory segment");
        exit(1);
    }
    printf("Shared memory segment removed\n");

    // Close the semaphore
    printf("closing...\n");
    sem_close(sem);
    printf("semaphore closed\n");
    sem_unlink(SEM_NAME);
    printf("semaphore unlinked\n");
    
    // remove the key from the token file
    file = fopen(TOKEN_FILE, "w");
    if (!file) {
        perror("second fopen failed");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "0\n");  // Save the key as an integer
    fclose(file);
    return 0;
}
