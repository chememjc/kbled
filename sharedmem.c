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
#include <time.h>
#include <errno.h>
#include "sharedmem.h"

int shm_id;
struct shared_data *shm_ptr;
sem_t *sem;

int get_executable_path(char *buffer, size_t size) {
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len == -1) {
        perror("readlink failed");
        return 1;
    }
    buffer[len] = '\0';  // Null-terminate the string
    return 0;
}

int sharedmem_masterinit(char verbose) {
    char exe_path[256];
    key_t shm_key={0};
    FILE *file;

    // Create the directory for the token file if it doesn't exist
    if(verbose)printf("Setup token file...\n");
    if(mkdir("/var/run", 0666) == -1 && errno != EEXIST) {
        perror("mkdir failed");
        return 1;
    }

    // Get the full path of the current executable, if it fails, return a failure event
    if(get_executable_path(exe_path, sizeof(exe_path))) return 1;

    // Generate a unique key using ftok
    shm_key = ftok(exe_path, PROJECT_ID);
    if(shm_key == -1) {
        perror("ftok failed");
        return 1;
    }

    // Write the key to the token file
    file = fopen(TOKEN_FILE, "w");
    if(!file) {
        perror("fopen failed");
        return 1;
    }

    fprintf(file, "%d\n", shm_key);  // Save the key as an integer
    fclose(file);

    // Create the shared memory segment with 0666 permissions (readable and writable by all)
    if(verbose)printf("Create shared memeory segment (%li bytes)...\n",sizeof(struct shared_data));
    shm_id = shmget(shm_key, sizeof(struct shared_data), IPC_CREAT | 0666);
    if(shm_id == -1) {
        perror("shmget failed");
        return 1;
    }

    // Attach to the shared memory segment
    if(verbose)printf("Attach to shared memory segment...\n");
    shm_ptr = shmat(shm_id, NULL, 0);
    if(shm_ptr == (void *)-1) {
        perror("shmat failed");
        return 1;
    }

    // Create the semaphore for mutual exclusion
    if(verbose)printf("Create the semaphore...\n");
    mode_t old_umask = umask(0000); // change umask so the semaphore file is created with correct permissions
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);  // Initial value = 1 (mutex)
    umask(old_umask); // Restore original umask
    if(sem == SEM_FAILED) {
        perror("sem_open failed");
        return 1;
    }
    return 0;
}

int sharedmem_slaveinit(char verbose) {
    key_t shm_key;
    FILE *file;

    // Read the token from the file
    file = fopen(TOKEN_FILE, "r");
    if (!file) {
        perror("slaveinit fopen failed");
        return 1;
    }

    if (fscanf(file, "%d", &shm_key) != 1) {
        perror("slaveinit fscanf failed");
        fclose(file);
        return 1;
    }
    fclose(file);
    
    if(shm_key==0){
        if(verbose)printf("The kbled daemon is not running.  Start it and try again.\n");
        return 1;
    }

    // Access the shared memory segment using the key
    shm_id = shmget(shm_key, sizeof(struct shared_data), 0666);
    if (shm_id == -1) {
        perror("slaveinit shmget failed");
        if(verbose)printf("The kbled shared memory is not accessible, check permissions?\n");
        return 1;
    }

    // Attach to the shared memory segment
    shm_ptr = shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("slaveinit shmat failed");
        return 1;
    }

    // Open the semaphore for mutual exclusion (it should already exist)
    sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("slaveinit sem_open failed");
        if(verbose)printf("Could not open semaphore: %s\n", SEM_NAME);
        return 1;
    }
    return 0;
}

int sharedmem_lock(){
    // Wait (lock) the semaphore before accessing shared memory, timeout after SM
    if (sem == NULL) {
        fprintf(stderr, "sharedmem_lock: Semaphore not opened!\n");
        return 1;
    }
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        return 1;
    }
    // Convert milliseconds to seconds and nanoseconds
    ts.tv_sec += SEM_TIMEOUT_MS / 1000;            // Add full seconds
    ts.tv_nsec += (SEM_TIMEOUT_MS % 1000) * 1000000; // Add the remaining milliseconds as nanoseconds
    // Normalize time if nanoseconds exceed 1 second
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }
    // Attempt to wait on the semaphore with the timeout
    if (sem_timedwait(sem, &ts) == -1) {
        if (errno == ETIMEDOUT) {
            // Semaphore not acquired due to timeout
            perror("sharedmem_lock: Timeout occured, proceeding anyway.  Check for lock that is never unlocked");
            return 1;
        } else {
            // Other error
            perror("sem_timedwait: Other sem_timedwait() error");
            return 1;
        }
    }
    // Semaphore successfully acquired
    return 0;
}

void sharedmem_unlock(){
    // Signal (unlock) the semaphore after accessing shared memory
    sem_post(sem);
}

int sharedmem_masterclose(char verbose) {
    //shutdown sequence
    // Detach from the shared memory segment
    int problem=0; //count the number of problems encountered during the process
    FILE *file;
    
    sharedmem_lock(); //lock the semaphore to make sure nothing else tries to attach
    printf("Detaching shared memory...\n");
    if(shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
        problem |= 1;
    }
    printf("Shared memory detached\n");
    //remove shared memory segment
    if(shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl: error removing shared memory segment");
        problem |= 1<<1;
    }
    if(verbose)printf("Shared memory segment removed\n");

    // Close the semaphore
    if(sem_close(sem) == -1){
        perror("sem_close: error closing semaphore");
        problem |= 1<<2;
    }
    if(sem_unlink(SEM_NAME) == -1){
        perror("sem_unlink: error unlinking semaphore");
        problem |= 1<<3;
    }
    if(verbose)printf("semaphore unlinked\n");
    
    // remove the key from the token file
    file = fopen(TOKEN_FILE, "w");
    if (!file) {
        perror("second fopen failed");
        problem |= 1<<4;
    }

    fprintf(file, "0\n");  // clear the shared memory ID
    fclose(file);
    return problem;
}

int sharedmem_slaveclose(char verbose) {
    // Detach from the shared memory segment
    if (shmdt(shm_ptr) == -1) {
        if(verbose) perror("shmdt failed");
        return 1;
    }
    return 0;
}
