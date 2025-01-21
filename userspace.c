/* System76 bonw15 (Clevo X370)
 * Individually addressable keyboard LED daemon
 * Michael Curtis 2025-01-17
 * shared memory (userspace side) for dynamic state change.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>  //needed for the semaphore timeout code
#include <errno.h> //needed for the semaphore timeout code
#include "sharedmem.h"

void print_usage(char *program_name) {
    fprintf(stderr, "Usage: %s [-v] [parameters...]\n", program_name);
    fprintf(stderr, " Parameter:                   Description:\n");
    fprintf(stderr, " -v                           Verbose output\n");
    fprintf(stderr, " -b+                          Increase brightness\n");
    fprintf(stderr, " -b-                          Decrease brightness\n");
    fprintf(stderr, " -b <0-10>                    Set brightness\n");
    fprintf(stderr, " -s+                          Increase pattern speed\n");
    fprintf(stderr, " -s-                          Decrease pattern speed\n");
    fprintf(stderr, " -s <0-2>                     Set pattern speed\n");
    fprintf(stderr, " -p+                          Increment pattern\n");
    fprintf(stderr, " -p-                          Decrement pattern\n");
    fprintf(stderr, " -p <-1 to 6>                 Set pattern, (-1=no pattern)\n");
    fprintf(stderr, " -bl <Red> <Grn> <Blu>        Set backlight color\n");
    fprintf(stderr, " -fo <Red> <Grn> <Blu>        Set focus color (caps/num/scroll locks)\n");
    fprintf(stderr, " -c                           Cycle through preset backlight/focus colors\n");
    fprintf(stderr, " -k <LED#> <Red> <Grn> <Blu>  Set individual LED (0-%i) color\n", NKEYS-1);
    fprintf(stderr, " -kb <LED#>                   Set individual LED (0-%i) to backlight color\n", NKEYS-1);
    fprintf(stderr, " -kf <LED#>                   Set individual LED (0-%i) to focus color\n", NKEYS-1);
    fprintf(stderr, " --dump                       Show contents of shared memory\n");
    fprintf(stderr, " --dump+                      Show contents of shared memory with each key's state\n");
    fprintf(stderr, " Where <Red> <Grn> <Blu> are 0-255\n");
}

int validrgb(const char *value) {
    int num = atoi(value);
    return (num >= 0 && num <= 255);
}

void printstructure(struct shared_data *data, char type) {
    // Print each member of the structure
    printf("Status: 0x%04x SM_B:%i SM_BI:%i SM_S:%i SM_SI:%i SM_E:%i SM_EI:%i SM_BL:%i SM_FO:%i SM_KEY:%i\n", data->status,
        data->status & 1,(data->status>>1) & 1,(data->status>>2) & 1,(data->status>>3) & 1,(data->status>>4) & 1,(data->status>>5) & 1,(data->status>>6) & 1,(data->status>>7) & 1,(data->status>>8) & 1);
    printf("Brightness: %u\n", data->brightness);
    printf("Brightness Increment: %d\n", data->brightnessinc);
    printf("Speed: %u\n", data->speed);
    printf("Speed Increment: %d\n", data->speedinc);
    printf("Effect: %d\n", data->effect);
    printf("Effect Increment: %d\n", data->effectinc);
    
    // Print the backlight (R, G, B values)
    printf("Backlight (R,G,B): (%u, %u, %u)\n", data->backlight[0], data->backlight[1], data->backlight[2]);
    
    // Print the focus (R, G, B values)
    printf("Focus (R,G,B): (%u, %u, %u)\n", data->focus[0], data->focus[1], data->focus[2]);
    
    // Print the key array if memdump=2
    if(type==2){
        printf("Keys: (R,G,B) Updt\n");
        for (int j = 0; j < NKEYS; j++) {
            printf("Key[%d]:(%u,%u,%u)%u\n", j,data->key[j][0],data->key[j][1],data->key[j][2],data->key[j][3]);
        }
    }
}

int sem_wait_timed(sem_t *sem, unsigned int tmsec) {
    if (sem == NULL) {
        fprintf(stderr, "NULL sem_t sent to sem_wait_timed.\n");
        return 1;
    }
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        return 1;
    }
    // Convert milliseconds to seconds and nanoseconds
    ts.tv_sec += tmsec / 1000;            // Add full seconds
    ts.tv_nsec += (tmsec % 1000) * 1000000; // Add the remaining milliseconds as nanoseconds
    // Normalize time if nanoseconds exceed 1 second
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }
    // Attempt to wait on the semaphore with the timeout
    if (sem_timedwait(sem, &ts) == -1) {
        if (errno == ETIMEDOUT) {
            // Semaphore not acquired due to timeout
            perror("sem_timedwait");
            return 1;
        } else {
            // Other error
            perror("sem_timedwait");
            return 1;
        }
    }
    // Semaphore successfully acquired
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    struct shared_data new_ptr;
    memset(&new_ptr, 0, sizeof(struct shared_data));
    char verbose = 0; // Flag for verbose output
    char memdump = 0; // Flag for dumping shared memory
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            // Verbose output flag
            verbose = 1;
            i++;
        }
        else if (strcmp(argv[i], "-b+") == 0) {
            // Increase brightness
            if(verbose)printf("Increase brightness\n");
            new_ptr.brightnessinc=1; //increment value
            new_ptr.status |= SM_BI; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-b-") == 0) {
            // Decrease brightness
            if(verbose)printf("Decrease brightness\n");
            new_ptr.brightnessinc=-1; //decrement value
            new_ptr.status |= SM_BI; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-b") == 0) {
            // Set brightness (0-10)
            if (i + 1 < argc && atoi(argv[i + 1]) >= 0 && atoi(argv[i + 1]) <= 10) {
                if(verbose)printf("Set brightness to %s\n", argv[i + 1]);
                new_ptr.brightness=atoi(argv[i+1]); //set value
                new_ptr.status |= SM_B; //set update flag
                i += 2;
            } else {
                fprintf(stderr, "Error: -b requires an argument between 0 and 10\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-s+") == 0) {
            // Increase pattern speed
            if(verbose)printf("Increase pattern speed\n");
            new_ptr.speedinc=1; //increment value
            new_ptr.status |= SM_SI; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-s-") == 0) {
            // Decrease pattern speed
            if(verbose)printf("Decrease pattern speed\n");
            new_ptr.speedinc=-1; //decrement value
            new_ptr.status |= SM_SI; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-s") == 0) {
            // Set pattern speed (0-2)
            if (i + 1 < argc && atoi(argv[i + 1]) >= 0 && atoi(argv[i + 1]) <= 2) {
                if(verbose)printf("Set pattern speed to %s\n", argv[i + 1]);
                new_ptr.speed=atoi(argv[i+1]); //set value
                new_ptr.status |= SM_S; //set update flag
                i += 2;
            } else {
                fprintf(stderr, "Error: -s requires an argument between 0 and 2\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-p+") == 0) {
            // Increment pattern
            if(verbose)printf("Increment pattern\n");
            new_ptr.effectinc=1; //increment value
            new_ptr.status |= SM_EI; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-p-") == 0) {
            // Decrement pattern
            if(verbose)printf("Decrement pattern\n");
            new_ptr.effectinc=-1; //decrement value
            new_ptr.status |= SM_EI; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-p") == 0) {
            // Set pattern (-1 to 6)
            if (i + 1 < argc && atoi(argv[i + 1]) >= -1 && atoi(argv[i + 1]) <= 6) {
                if(verbose)printf("Set pattern to %s\n", argv[i + 1]);
                new_ptr.effect=atoi(argv[i+1]); //set value
                new_ptr.status |= SM_E; //set update flag
                i += 2;
            } else {
                fprintf(stderr, "Error: -p requires an argument between -1 and 6\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-bl") == 0) {
            // Set backlight color (3 values: Red, Green, Blue)
            if (i + 3 < argc && validrgb(argv[i + 1]) && validrgb(argv[i + 2]) && validrgb(argv[i + 3])) {
                if(verbose)printf("Set backlight color to Red=%s, Green=%s, Blue=%s\n", argv[i + 1], argv[i + 2], argv[i + 3]);
                new_ptr.backlight[0]=atoi(argv[i+1]); //set value
                new_ptr.backlight[1]=atoi(argv[i+2]); //set value
                new_ptr.backlight[2]=atoi(argv[i+3]); //set value
                new_ptr.status |= SM_BL; //set update flag
                i += 4;
            } else {
                fprintf(stderr, "Error: -bl requires three numeric arguments (Red, Green, Blue) in the range 0-255\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-fo") == 0) {
            // Set focus color (3 values: Red, Green, Blue)
            if (i + 3 < argc && validrgb(argv[i + 1]) && validrgb(argv[i + 2]) && validrgb(argv[i + 3])) {
                if(verbose)printf("Set focus color to Red=%s, Green=%s, Blue=%s\n", argv[i + 1], argv[i + 2], argv[i + 3]);
                new_ptr.focus[0]=atoi(argv[i+1]); //set value
                new_ptr.focus[1]=atoi(argv[i+2]); //set value
                new_ptr.focus[2]=atoi(argv[i+3]); //set value
                new_ptr.status |= SM_FO; //set update flag
                i += 4;
            } else {
                fprintf(stderr, "Error: -fo requires three numeric arguments (Red, Green, Blue) in the range 0-255\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-c") == 0) {
            // Cycle through preset backlight/focus colors
            if(verbose)printf("Cycle through preset backlight/focus colors\n");
            i++;
        }
        else if (strcmp(argv[i], "-k") == 0) {
            // Set individual LED color (0-NKEYS)
            if (i + 4 < argc) {
                unsigned char led = atoi(argv[i + 1]);
                if (atoi(argv[i + 1]) >= 0 && led < NKEYS-1 && validrgb(argv[i + 2]) && validrgb(argv[i + 3]) && validrgb(argv[i + 4])) {
                    if(verbose)printf("Set LED %d color to Red=%s, Green=%s, Blue=%s\n", led, argv[i + 2], argv[i + 3], argv[i + 4]);
                    new_ptr.key[led][0]=atoi(argv[i+2]); //set value
                    new_ptr.key[led][1]=atoi(argv[i+3]); //set value
                    new_ptr.key[led][2]=atoi(argv[i+4]); //set value
                    new_ptr.key[led][3]=SM_UPD; //set update flag
                    new_ptr.status |= SM_KEY; //set update flag
                    i += 5;
                } else {
                    fprintf(stderr, "Error: -k requires a valid LED (0-%i) and three numeric color arguments (Red, Green, Blue) in the range 0-255\n", NKEYS-1);
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: -k requires LED number and three color arguments\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-kb") == 0) {
            // Set key to background color
            if (i + 1 < argc && atoi(argv[i + 1]) >= 0 && atoi(argv[i + 1]) <= NKEYS-1) {
                unsigned char led = atoi(argv[i + 1]);
                if(verbose)printf("Set LED %i to background color\n", led);
                new_ptr.key[led][3]=SM_BKGND; //set update value to background color
                new_ptr.status |= SM_KEY; //set update flag
                i += 2;
            } else {
                fprintf(stderr, "Error: -kb requires a LED number between 0 and %i\n",NKEYS-1);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-kf") == 0) {
            // Set key to background color
            if (i + 1 < argc && atoi(argv[i + 1]) >= 0 && atoi(argv[i + 1]) <= NKEYS-1) {
                unsigned char led = atoi(argv[i + 1]);
                if(verbose)printf("Set LED %i to backlight color\n", led);
                new_ptr.key[led][3]=SM_FOCUS; //set update value to focus color
                new_ptr.status |= SM_KEY; //set update flag
                i += 2;
            } else {
                fprintf(stderr, "Error: -kf requires a LED number between 0 and %i\n",NKEYS-1);
                return 1;
            }
        }
        else if (strcmp(argv[i], "--dump") == 0) {
            // Increase brightness
            if(verbose)printf("Dump contents of shared memory:\n");
            memdump=1;
            i++;
        }
        else if (strcmp(argv[i], "--dump+") == 0) {
            // Increase brightness
            if(verbose)printf("Dump contents of shared memory with individual keys:\n");
            memdump=2;
            i++;
        }
        else {
            // Handle unknown switch
            fprintf(stderr, "Error: Unknown switch: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    key_t shm_key;
    int shm_id;
    struct shared_data *shm_ptr;
    FILE *file;
    sem_t *sem;

    // Read the token from the file
    file = fopen(TOKEN_FILE, "r");
    if (!file) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    if (fscanf(file, "%d", &shm_key) != 1) {
        perror("fscanf failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    
    if(shm_key==0){
        printf("The kbled daemon is not running.  Start it and try again.\n");
        return 0;
    }

    // Access the shared memory segment using the key
    shm_id = shmget(shm_key, sizeof(struct shared_data), 0666);
    if (shm_id == -1) {
        perror("shmget failed");
        printf("The kbled shared memory is not accessible, check permissions?\n");
        exit(EXIT_FAILURE);
    }

    // Attach to the shared memory segment
    shm_ptr = shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }

    // Open the semaphore for mutual exclusion (it should already exist)
    printf("Opening semaphore...\n");
    sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        printf("Could not open semaphore: %s\n", SEM_NAME);
        exit(EXIT_FAILURE);
    }
    printf("Semaphore opened\n");
    // Wait (lock) the semaphore before accessing shared memory and making updates
    printf("Locking...\n");
    sem_wait_timed(sem,SEM_TIMEOUT_MS); //lock semaphore
    printf("Locked\n");
    if(new_ptr.status!=0) shm_ptr->status=new_ptr.status;
    if(new_ptr.status & SM_B)   shm_ptr->brightness=new_ptr.brightness;
    if(new_ptr.status & SM_BI)  shm_ptr->brightnessinc=new_ptr.brightnessinc;
    if(new_ptr.status & SM_S)   shm_ptr->speed=new_ptr.speed;
    if(new_ptr.status & SM_SI)  shm_ptr->speedinc=new_ptr.speedinc;
    if(new_ptr.status & SM_E)   shm_ptr->effect=new_ptr.effect;
    if(new_ptr.status & SM_EI)  shm_ptr->effectinc=new_ptr.effectinc;
    if(new_ptr.status & SM_BL)  for(int i=0; i<3; i++) shm_ptr->backlight[i]=new_ptr.backlight[i];
    if(new_ptr.status & SM_FO)  for(int i=0; i<3; i++) shm_ptr->focus[i]=new_ptr.focus[i];
    if(new_ptr.status & SM_KEY) for(int i=0; i<4; i++) for(int j=0; j<NKEYS; j++) if(new_ptr.key[3]!=0)shm_ptr->key[j][i]=new_ptr.key[j][i];
    if(memdump) printstructure(shm_ptr,memdump);
    printf("Unlocking...\n");
    sem_post(sem); //unlock semaphore
    printf("Unlocked\n");

    // Detach from the shared memory segment
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}
