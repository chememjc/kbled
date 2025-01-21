/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * kbled - daemon for keyboard LED backlight control
 */
 
#include <stdio.h>
#include "keymap.h"
#include "it829x.h"
#include "kbstatus.h"
#include "sharedmem.h"
#include <stdlib.h>  //needed for atoi()
#include <stdint.h>  //uint8_t etc. definitions
#include <unistd.h>  //for sleep function, open(), close() etc...
#include <signal.h>  //for handling signals sent
#include <ucontext.h> //for handling sigsev, really not that useful.  If it causes trouble, delete the code in sighandle() -> case: SIGSEV and you can remove this dependency

#define UPDATE 100*1000 //sleep time in usec between polling

void sighandle(int sig, siginfo_t *info, void *context) {
    // Print the signal name based on the signal number
    printf("Received signal: %i @ %p\n",sig,info->si_addr);
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
    //release the shared memory, close the semaphore and remove the shared memory ftok token
    sharedmem_close();
    exit(0);  // Exit the program after handling the signal
}

int main(int argc, char **argv){
    if(!(argc==7 || argc==1)) {
        printf("Syntax: %s baselineR baselineG baselineB focusR focusG focusB\notherwise defaults are used without arguments\n",argv[0]);
        return 0;
    }
    // Setup signal handler:
    struct sigaction sa;
    // Set up the signal handler
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
            printf("signals[%li]=%i\n",i,signals[i]);
            return 1;  // Exit with error if sigaction fails
        }
    }
    
    //Start main code:
    uint8_t state=0xFF; //set to a value that wouldn't ever appear so it forces an uppdate when the kbstat() function is first run
    uint8_t newstate=0;
    uint8_t backlight[3]={0,0,0}; //default to all off unless otherwise specified
    uint8_t focus[3]={64,0,0};  //default to 1/4 intensity red
    if(argc==7)for(int i=0;i<3;i++) {
        backlight[i]=atoi(argv[1+i]);  //set default backlight color from command line
        focus[i]=atoi(argv[4+i]);  //set default focus color from command line
    }
    printf("Backlight set: R %u, G %u, B %u  Focus set: R %u, G %u, B %u\n",backlight[0],backlight[1],backlight[2],focus[0],focus[1],focus[2]);
    
    //initialize the keyboard:
    printf("Setup keyboard USB interface...\n");
    if(it829x_init()==-1 || it829x_brightspeed(MAXBRIGHT, MAXSPEED)==-1 || it829x_setleds(allkeys, NKEYS, backlight)==-1) return -1; //open connection to USB, set brightness/speed, initialize all keys to backlight; quit if there is a problem
    it829x_close();
    
    //now bring up the shared memory interface to get signals from the client
    printf("Setup shared memory...\n");
    sharedmem_init();
    sharedmem_lock(); //lock the structure from other processes
    shm_ptr->status=0;
    shm_ptr->brightness=MAXBRIGHT;
    shm_ptr->brightnessinc=0;
    shm_ptr->speed=0;
    shm_ptr->speedinc=0;
    shm_ptr->effect=-1;
    shm_ptr->effectinc=0;
    shm_ptr->backlight[0]=backlight[0];
    shm_ptr->backlight[1]=backlight[1];
    shm_ptr->backlight[2]=backlight[2];
    shm_ptr->focus[0]=focus[0];
    shm_ptr->focus[1]=focus[1];
    shm_ptr->focus[2]=focus[2];
    for(int j=0;j<NKEYS;j++) for(int i=0;i<4;i++){
        if(i!=3)shm_ptr->key[j][i]=backlight[i];
        else shm_ptr->key[j][i]=0;
    }
    sharedmem_unlock();
    
    //keyboard at initial state, now wait for an event and update
    while(state!=FAULT){
    usleep(UPDATE); //polling time
    newstate=kbstat();
        if(state!=newstate){
	        state=newstate;
	        it829x_init();
	        it829x_setled(K_CAPSL,    (state & CAPLOC)? shm_ptr->focus:shm_ptr->backlight);
	        it829x_setled(K_CAPSR,    (state & CAPLOC)? shm_ptr->focus:shm_ptr->backlight);
	        it829x_setled(K_NUM_LOCK, (state & NUMLOC)? shm_ptr->focus:shm_ptr->backlight);
	        it829x_setled(K_INSERT,   (state & SCRLOC)? shm_ptr->focus:shm_ptr->backlight);
	        it829x_close();
	        //update the key state array:
	        sharedmem_lock();
            for(int i=0;i<3;i++){
                shm_ptr->key[findkey(K_CAPSL   )][i]=(state & CAPLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
                shm_ptr->key[findkey(K_CAPSR   )][i]=(state & CAPLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
                shm_ptr->key[findkey(K_NUM_LOCK)][i]=(state & NUMLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
                shm_ptr->key[findkey(K_INSERT  )][i]=(state & SCRLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
            }
            sharedmem_unlock(); 
	    }
	    if(shm_ptr->status!=0){
	        printf("Status: 0x%04x SM_B:%i SM_BI:%i SM_S:%i SM_SI:%i SM_E:%i SM_EI:%i SM_BL:%i SM_FO:%i SM_KEY:%i\n", shm_ptr->status,
                shm_ptr->status & 1,(shm_ptr->status>>1) & 1,(shm_ptr->status>>2) & 1,(shm_ptr->status>>3) & 1,(shm_ptr->status>>4) & 1,(shm_ptr->status>>5) & 1,(shm_ptr->status>>6) & 1,(shm_ptr->status>>7) & 1,(shm_ptr->status>>8) & 1);
	        sharedmem_lock();
	        if(shm_ptr->status & (SM_B | SM_BI | SM_S | SM_SI)){
	            if(shm_ptr->status & SM_BI){
	                printf("Inc/dec brightness: %i -> ",shm_ptr->brightness);
	                if(shm_ptr->brightness==MINBRIGHT && shm_ptr->brightnessinc==-1) shm_ptr->brightness=MINBRIGHT;
	                else shm_ptr->brightness += shm_ptr->brightnessinc;
	                printf(" %i\n",shm_ptr->brightness);
	            }
	            if(shm_ptr->status & SM_SI){
	                printf("Inc/dec speed: %i -> ",shm_ptr->speed);
	                if(shm_ptr->speed==MINSPEED && shm_ptr->speedinc==-1) shm_ptr->speed=MINSPEED;
	                else shm_ptr->speed += shm_ptr->speedinc;
	                printf("%i\n",shm_ptr->speed);
	            }
	            if(shm_ptr->brightness>MAXBRIGHT) shm_ptr->brightness=MAXBRIGHT;
	            if(shm_ptr->speed>MAXSPEED) shm_ptr->speed=MAXSPEED;
	            if(it829x_init()==-1 || it829x_brightspeed(shm_ptr->brightness, shm_ptr->speed)==-1) printf("Error setting brightness from shared memory\n"); //send updates to keyboard
                it829x_close();
                printf("Updated brightness/speed:: %i , %i\n",shm_ptr->brightness,shm_ptr->speed);
	        }
	        if(shm_ptr->status & SM_BL){
	            for(int i=0;i<3;i++) {
	                for(int j=0;j<NKEYS;j++) shm_ptr->key[j][i]=shm_ptr->backlight[i]; //update key state array
	            }
	            if(it829x_init()==-1 || it829x_setleds(allkeys, NKEYS, shm_ptr->backlight)==-1) printf("Error setting backlight from shared memory\n");
	            it829x_close();
	            printf("backlight: R:%i G:%i B:%i\n",shm_ptr->backlight[0],shm_ptr->backlight[1],shm_ptr->backlight[2]);
	            state=0xFF;
	        }
	        if(shm_ptr->status & SM_FO){
	            for(int i=0;i<3;i++) focus[i]=shm_ptr->focus[i];
	            printf("focus: R:%i G:%i B:%i\n",shm_ptr->focus[0],shm_ptr->focus[1],shm_ptr->focus[2]);
	            state=0xFF;
	        }
	        
	        shm_ptr->status=0;
	        sharedmem_unlock();
	    }
    }
    printf("Exiting... something yet to be discovered did not go as planned!\n");
    return -1;
}
