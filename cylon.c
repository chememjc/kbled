/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * kbledpsmon - program to update keys based on processor load
 * Useful tool for testing: stress --cpu 2 --timeout 60
 * use apt: "apt install stress"
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>   //for handling signals sent
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <time.h>
#include "sharedmem.h"

#define CYLONKEYS 20

struct shared_data new_ptr; //internal structure to write to kbled shared memory 

void print_usage(char *programname) {
    fprintf(stderr, "Usage: %s [parameters...]\n", programname);
    fprintf(stderr, "Example: %s [parameters...]\n", programname);
    fprintf(stderr, " Parameter:                    Description:\n");
    fprintf(stderr, " -u or --update <msec>         Update process load frequency (100 to 65535 ms)  Default=200 ms\n");
    fprintf(stderr, " -cpu                          Display the time it took kbled daemon to execute the last update\n");
    fprintf(stderr, " -v                            Verbose output\n");
    fprintf(stderr, " --scan                        Change keyboard update speed (1 to 65535 ms) default= 100 ms\n");
    fprintf(stderr, " --dump                        Show contents of shared memory\n");
    fprintf(stderr, " --dump+                       Show contents of shared memory with each key's state\n");
    fprintf(stderr, " -h or --help                  Display this message\n");
}

void sighandle(int sig, siginfo_t *info, void *context) {
    // Print the signal name based on the signal number
    printf("\nReceived signal: %i @ %p\n",sig,info->si_addr);
    switch (sig) {
        case SIGINT:
            printf("Received signal: SIGINT (Interrupt from keyboard)\n");
            break;
        case SIGTERM:
            printf("Received signal: SIGTERM (Termination signal)\n");
            break;
        case SIGSEGV:
            printf("Received signal: SIGSEGV (Segmentation fault)\n");
            ucontext_t *uc = (ucontext_t *)context;
            printf("RIP (Instruction Pointer): %llx\n", uc->uc_mcontext.gregs[0]);
            printf("RSP (Stack Pointer): %llx\n", uc->uc_mcontext.gregs[1]);
            break;
        case SIGABRT:
            printf("Received signal: SIGABRT (Abort signal)\n");
            break;
        case SIGFPE:
            printf("Received signal: SIGFPE (Floating point exception)\n");
            break;
        case SIGILL:
            printf("Received signal: SIGILL (Illegal instruction)\n");
            break;
        case SIGBUS:
            printf("Received signal: SIGBUS (Bus error)\n");
            break;
        case SIGQUIT:
            printf("Received signal: SIGQUIT (Quit signal)\n");
            break;
        case SIGHUP:
            printf("Received signal: SIGHUP (Hangup)\n");
            break;
        case SIGPIPE:
            printf("Received signal: SIGPIPE (Broken pipe)\n");
            break;
        case SIGALRM:
            printf("Received signal: SIGALRM (Alarm clock)\n");
            break;
        case SIGCHLD:
            printf("Received signal: SIGCHLD (Child process terminated or stopped)\n");
            break;
        default:
            printf("Received unknown signal: %d\n", sig);
            break;
    }
    //restore keyboard to original state unless something went fairly wrong:
    if(sig == SIGINT || sig == SIGTERM || sig == SIGABRT || sig == SIGQUIT || sig==SIGHUP){
      sharedmem_lock();
      shm_ptr->status |= SM_BL; //update backlight across the keyboard
      sharedmem_unlock();
    }
    //free(cpu); //free used memory for cpu load array
    //release the shared memory, close the semaphore and remove the shared memory ftok token
    sharedmem_slaveclose(SM_QUIET);
    printf("kbledpsmon closing...\n");
    exit(0);  // Exit the program since everything should be cleaned up
}

int main(int argc, char *argv[]) {
    // Setup signal handler:
    struct sigaction sa;
    sa.sa_sigaction = sighandle;  // Use the extended handler
    sa.sa_flags = SA_SIGINFO;     // Enable extended signal handling
    sigemptyset(&sa.sa_mask);     // Don't block any signals

    // Register the signal handler for these possible signals
    //{SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, SIGQUIT, SIGHUP, SIGKILL, SIGPIPE, SIGALRM, SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGUSR1, SIGUSR2, SIGPOLL, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH};
    const int signals[] = {SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, SIGQUIT, SIGHUP, SIGPIPE, SIGALRM, SIGCHLD};
    
    // Register each signal
    for (long unsigned int i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        if (sigaction(signals[i], &sa, NULL) == -1) {
            perror("sigaction");
            printf("problem setting sigaction() for signals[%li]=%i\n",i,signals[i]);
            return 1;  //return with fault since there was a problem
        }
    }
    
    memset(&new_ptr, 0, sizeof(struct shared_data));
    char verbose = 0; // Flag for verbose output
    char memdump = 0; // Flag for dumping shared memory
    char cputime = 0; // Flag for reporting time spent in last kbled keyboard update event
    uint32_t update = 150; //default update time
    int i = 1;
    
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            // Verbose output flag
            verbose = 1;
            i++;
        }
        else if ((strcmp(argv[i], "-u") == 0) || (strcmp(argv[i], "--update") == 0)) {
            // Set pattern (-1 to 6)
            if (i + 1 < argc && atoi(argv[i + 1]) >= 100) {
                if(verbose)printf("Change movement speed to: %s", argv[i + 1]);
                update=atoi(argv[i+1]); //set value
                if(update<100) update=100;
                printf("movement speed set to %u ms\n",update);
                i += 2;
            } else {
                fprintf(stderr, "Error: %s requires an argument between 100 and 65535\n",argv[i]);
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
        else if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            // Increase brightness
            print_usage(argv[0]);
            return 1;
        }
        else {
            // Handle unknown switch
            fprintf(stderr, "Error: Unknown switch: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    //Houskeeping Complete  ------------------------------------------------------------------------------------
    uint8_t cylonkeymap[CYLONKEYS]={0}; 
    int8_t cylonpos=0; //initial position of bright spot
    int8_t cylondir=1; //positive to go to right, negative to go to left
    for(i=0;i<CYLONKEYS; i++) cylonkeymap[i]=i; //populate cylonkeymap to the first row
    uint8_t val=255; //value of red color
    
    if(sharedmem_slaveinit(verbose)!=0){
        fprintf(stderr, "Failed to connect to kbled daemon, are you sure it is running?\n");
        return 1;
    }
    if(verbose)printf("Attached to shared memory\n");
    
    
    while(1){
        usleep((uint32_t)update * 1000); //delay time between updates
        //+++++++++++++++++++++++++++ Your code goes below here
        for(i=0;i<CYLONKEYS;i++){
            if(i==cylonpos) val=255;
            else if(i==cylonpos+1 || i== cylonpos-1) val=127; //if one less or one greater, then go at 1/2 brightness
            else if(i==cylonpos+2 || i== cylonpos-2) val=16; //if two less or two greater, then go at 1/16 brightness
            else val=0;
            new_ptr.key[cylonkeymap[i]][0]=val; //set value
            new_ptr.key[cylonkeymap[i]][1]=0; //set value
            new_ptr.key[cylonkeymap[i]][2]=0; //set value
            new_ptr.key[cylonkeymap[i]][3]=SM_UPD; //set update flag
        }
        
        new_ptr.status |= SM_KEY; //set update flag
        cylonpos+= cylondir;
        if(cylonpos>=CYLONKEYS){ //turn the other direction when we reach the max key
            cylonpos=CYLONKEYS-1; //subtract 2 if you want it to not pause for 1 cycle on the end
            cylondir=-1;
        } 
        else if(cylonpos<0){ //turn the other direction when we reach the min key
            cylonpos=0; //set to 1 if you want it to not pause for 1 cycle on the end
            cylondir=1;
        }
        //--------------------------- Your code goes above here
        
        sharedmem_lock(); //lock semaphore **************************************************************************************
        if(verbose)printf("Semaphore opened\n");
        if(new_ptr.status!=0) shm_ptr->status=new_ptr.status;
        if(new_ptr.status & SM_KEY)     for(i=0; i<4; i++) for(int j=0; j<NKEYS; j++) if(new_ptr.key[3]!=0)shm_ptr->key[j][i]=new_ptr.key[j][i];
        if(memdump) sharedmem_printstructure(shm_ptr,memdump);
        if(cputime) printf("Last kbled daemon LED update time: %f ms, idle loop time %f ns\n", shm_ptr->lastcputime*1000.0,shm_ptr->idlecputime*1000.0);
        sharedmem_unlock(); //unlock semaphore  *********************************************************************************************************
        if(verbose)printf("Semaphore closed\n");
    }
    
    sharedmem_slaveclose(verbose);
    if(verbose)printf("Detached from shared memory, though somehow the while(1) loop failed\n");

    return 0;
}
