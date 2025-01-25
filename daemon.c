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
#include <stdlib.h>   //needed for atoi()
#include <stdint.h>   //uint8_t etc. definitions
#include <unistd.h>   //for sleep function, open(), close() etc...
#include <signal.h>   //for handling signals sent
#include <time.h>     //for calculatinng the time the loop takes to execute
#include <ucontext.h> //for handling sigsev, really not that useful.  If it causes trouble, delete the code in sighandle() -> case: SIGSEV and you can remove this dependency
#include <systemd/sd-daemon.h>  //for talking to systemd

#define UPDATE 100 //sleep time in usec between polling
#define DEFAULTBKLT {0,0,0} //default backlight RGB value
#define DEFAULTFOCUS {0,127,0} //default focus RGB value
#define DEFAULTBRIGHT MAXBRIGHT //default brightness value
#define DEFAULTSPEED 1 //default speed
#define DEFAULTEFFECT SM_EFFECT_NONE //default keyboard effect, -1=no effect (normal operation)

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
    sd_notify(0, "STATUS=kbled is shutting down...");
    //release the shared memory, close the semaphore and remove the shared memory ftok token
    sharedmem_masterclose(SM_VERBOSE);
    sd_notify(0, "STATUS=kbled is stopped");
    exit(0);  // Exit the program since everything should be cleaned up
}

int main(int argc, char **argv){
    if(!(argc==7 || argc==1)) {
        printf("Syntax: %s baselineR baselineG baselineB focusR focusG focusB\notherwise defaults are used without arguments\n",argv[0]);
        return 0; //let systemd know that there was a problem
    }
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
            return 1;  //let systemd know that there was a problem
        }
    }
    
    //Start main code:
    if(sharedmem_daemonstatus()!=0){ //check to see if daemon is already running; running multiple copies simultaneously would cause all sorts of trouble
        printf("Warning: kbled is already running, close running instance before starting.\n");
        return 1; //let system know there was a problem
    }
    if (sd_notify(0, "STATUS=kbled is starting...") < 0) {
        printf("Systemd notifications not supported; running standalone. (i.e not started by systemd)\n");
    }
    uint8_t state=0xFF; //set to a value that wouldn't ever appear so it forces an uppdate when the kbstat() function is first run for lock keys
    uint16_t scanspeed=UPDATE; //set default scan speed/update period
    uint8_t newstate=0; //latest keyboard lock key states
    uint8_t lockupdate=0; //flag to determine if the state of the lock keys on the keyboard were updated in the main loop
    uint8_t backlight[3]=DEFAULTBKLT; //set to default value for backlight color in case it isn't set on the command line
    uint8_t focus[3]=DEFAULTFOCUS;  //set to default value for focus color in case it isn't set on the command line
    clock_t begintime, endtime; //variables for holding start/end times for cpu time calculation in loop
    double cputime=-1.0; //time it took to run through the loop the last time something was updated
    int i,j; //general purpose incrementing variables
    
    if(argc==7)for(i=0;i<3;i++) {
        backlight[i]=atoi(argv[1+i]);  //set default backlight color from command line
        focus[i]=atoi(argv[4+i]);  //set default focus color from command line
    }
    printf("Backlight set: R %u, G %u, B %u  Focus set: R %u, G %u, B %u\n",backlight[0],backlight[1],backlight[2],focus[0],focus[1],focus[2]);
    
    //initialize the keyboard:
    printf("Setup keyboard USB interface...\n");
    if(it829x_init()==-1 || it829x_reset()==-1 || it829x_brightspeed(MAXBRIGHT, MAXSPEED)==-1 || it829x_setleds(allkeys, NKEYS, backlight)==-1){ //open connection to USB, set brightness/speed, initialize all keys to backlight; quit if there is a problem
        it829x_close(); //try to close in case it was opened successfully, no need for semaphore and shared memory 
        sd_notify(0, "STATUS=kbled could not connect to IT829x device over usb... Exiting.  Check permissions and presence of IT829x with lsusb");
        printf("could not connect to IT829x device over usb... Exiting.\nCheck permissions and presence of IT829x (ID=048d:8910) with lsusb\nMake sure you are running this process as root or with sudo\n");
        return 1; //let systemd know that there was a problem
    }
    it829x_close();
    
    //now bring up the shared memory interface to get signals from the client
    printf("Setup shared memory...\n");
    if(sharedmem_masterinit(SM_VERBOSE)!=0){
        sd_notify(0, "STATUS=kbled could not allocate shared memory, check permissions.  Exiting...");
        printf("Could not allocate shared memory, check permissions.  Exiting...\n");
        sharedmem_masterclose(SM_VERBOSE); //try to clean up shared memory and semaphore in case some of it succeeded
        return 1; //let systemd know that there was a problem
    }
    sharedmem_lock(); //lock the structure from other processes
    shm_ptr->status=0;
    shm_ptr->onoff=SM_ON;
    shm_ptr->scanspeed=UPDATE;
    shm_ptr->lastcputime=cputime;
    shm_ptr->brightness=DEFAULTBRIGHT;
    shm_ptr->brightnessinc=0;
    shm_ptr->speed=DEFAULTSPEED;
    shm_ptr->speedinc=0;
    shm_ptr->effect=DEFAULTEFFECT;
    shm_ptr->effectinc=0;
    shm_ptr->colorindex=0;
    shm_ptr->backlight[0]=backlight[0];
    shm_ptr->backlight[1]=backlight[1];
    shm_ptr->backlight[2]=backlight[2];
    shm_ptr->focus[0]=focus[0];
    shm_ptr->focus[1]=focus[1];
    shm_ptr->focus[2]=focus[2];
    for(j=0;j<NKEYS;j++) for(i=0;i<4;i++){
        if(i!=3)shm_ptr->key[j][i]=backlight[i];
        else shm_ptr->key[j][i]=0;
    }
    sharedmem_unlock();
    
    //keyboard at initial state, now wait for an event and update
    sd_notify(0, "READY=1"); //tell systemd that we're running
    sd_notify(0, "STATUS=kbled is running");
    while(1){
        usleep((uint32_t)scanspeed * 1000); //polling time
        begintime = clock(); //set the start time for measuring time spent for keyboard LED update
        newstate=kbstat();
        if(state!=newstate && state != FAULT && shm_ptr->effect==SM_EFFECT_NONE){ //if the keyboard state changed, read successfully and the keyboard isn't in an effect mode then update the caps/scroll/num lock LEDs
            state=newstate;
            it829x_init();
            it829x_setled(K_CAPSL,    (state & CAPLOC)? shm_ptr->focus:shm_ptr->backlight);
            it829x_setled(K_CAPSR,    (state & CAPLOC)? shm_ptr->focus:shm_ptr->backlight);
            it829x_setled(K_NUM_LOCK, (state & NUMLOC)? shm_ptr->focus:shm_ptr->backlight);
            it829x_setled(K_INSERT,   (state & SCRLOC)? shm_ptr->focus:shm_ptr->backlight);
            it829x_close();
            //update the key state array:
            sharedmem_lock();
            for(i=0;i<3;i++){ //update the state in shared memory
                shm_ptr->key[findkey(K_CAPSL   )][i]=(state & CAPLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
                shm_ptr->key[findkey(K_CAPSR   )][i]=(state & CAPLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
                shm_ptr->key[findkey(K_NUM_LOCK)][i]=(state & NUMLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
                shm_ptr->key[findkey(K_INSERT  )][i]=(state & SCRLOC)? shm_ptr->focus[i]:shm_ptr->backlight[i];
            }
            shm_ptr->lastcputime=cputime; //update cpu end time, this will be overwritten if something else happened in the same cycle
            sharedmem_unlock(); 
            lockupdate=1;
        }
        if(shm_ptr->status!=0){
            //printf("Status: 0x%04x SM_B:%i SM_BI:%i SM_S:%i SM_SI:%i SM_E:%i SM_EI:%i SM_BL:%i SM_FO:%i SM_KEY:%i\n", shm_ptr->status, //debug to verify flags are set properly
            //    shm_ptr->status & 1,(shm_ptr->status>>1) & 1,(shm_ptr->status>>2) & 1,(shm_ptr->status>>3) & 1,(shm_ptr->status>>4) & 1,(shm_ptr->status>>5) & 1,(shm_ptr->status>>6) & 1,(shm_ptr->status>>7) & 1,(shm_ptr->status>>8) & 1);
            sharedmem_lock();
            if(shm_ptr->status & SM_SSPD){ //handle kbled loop scan speed update
                if(shm_ptr->scanspeed>1){
                    printf("Update scan speed changed from %u ms -> %u ms\n",scanspeed,shm_ptr->scanspeed);
                    scanspeed=shm_ptr->scanspeed;
                }
                else printf("Scan speed out of range (1-65535 ms): %u ms\n",shm_ptr->scanspeed);
            }
            if(shm_ptr->status & (SM_B | SM_BI | SM_S | SM_SI)){ //handle brightness and speed changes
                if(shm_ptr->status & SM_BI){
                    printf("Inc/dec brightness: %i -> ",shm_ptr->brightness);
                    if(shm_ptr->brightness==MINBRIGHT && shm_ptr->brightnessinc==-1) shm_ptr->brightness=MINBRIGHT;
                    else shm_ptr->brightness += shm_ptr->brightnessinc;
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
                printf("Updated brightness/speed: %i , %i\n",shm_ptr->brightness,shm_ptr->speed);
            }
            if(shm_ptr->status & (SM_E | SM_EI)){ //handle effect change and increment
                if(shm_ptr->status & SM_EI){
                    printf("Inc/dec effect: %i -> ",shm_ptr->effect);
                    if(shm_ptr->effect==SM_EFFECT_NONE && shm_ptr->effectinc==-1) shm_ptr->effect=SM_EFFECT_SNAKE;  //decrement only until the effect is equal to the lowest value, -1 (no effect) and then wrap around
                    else shm_ptr->effect += shm_ptr->effectinc;
                }
                if(shm_ptr->effect > SM_EFFECT_SNAKE) shm_ptr->effect=SM_EFFECT_NONE;  //if you're at the end of the list then roll back around to the beginning, change this to SM_EFFECT_SNAKE to stay at the last event rather than rolling over
                if(shm_ptr->effect>=0){
                    if(it829x_init()==-1 || it829x_reset()==-1 || it829x_brightspeed(shm_ptr->brightness, shm_ptr->speed)==-1 || it829x_effect(shm_ptr->effect)==-1){
                        printf("Error setting effect from shared memory\n"); //send updates to keyboard
                    }
                 }
                 if(shm_ptr->effect==SM_EFFECT_NONE){
                    if(it829x_init()==0) {
                        it829x_reset();
                        it829x_brightspeed(shm_ptr->brightness, shm_ptr->speed);
                        shm_ptr->status |= (SM_BL | SM_FO); //make sure to update the backlight and focus colors if we're out of effect mode
                        state=0xFF;  //force an update of the lock key states when we go back to normal mode
                    }
                    else printf("Error opening connection to USB\n");
                }
                it829x_close();
                printf("Updated effect: %i\n",shm_ptr->effect);
            }
            if(shm_ptr->status & SM_PALT){
                shm_ptr->colorindex++;
                if(shm_ptr->colorindex>=SM_NUMCOLORS) shm_ptr->colorindex=0;
                if(shm_ptr->effect!=SM_EFFECT_NONE){
                    if(it829x_init()==0) {
                        it829x_reset();
                        it829x_brightspeed(shm_ptr->brightness, shm_ptr->speed);
                    }
                    else printf("Error opening connection to USB\n");
                }
                for(i=0;i<3;i++){
                    shm_ptr->backlight[i]=pallete[shm_ptr->colorindex].backlight[i];
                    shm_ptr->focus[i]=pallete[shm_ptr->colorindex].focus[i];
                }
                shm_ptr->status |= (SM_BL | SM_FO); //make sure to update the backlight and focus colors if we're out of effect mode
                state=0xFF;  //force an update of the lock key states when we go back to normal mode
            }
            if((shm_ptr->status & SM_BL) && (shm_ptr->effect==SM_EFFECT_NONE)){ //handle backlight color change but only if we're not displaying an effect
                for(i=0;i<3;i++) {
                    for(j=0;j<NKEYS;j++) shm_ptr->key[j][i]=shm_ptr->backlight[i]; //update key state array
                }
                if(it829x_init()==-1 || it829x_setleds(allkeys, NKEYS, shm_ptr->backlight)==-1) printf("Error setting backlight from shared memory\n");
                it829x_close();
                printf("backlight: R:%i G:%i B:%i\n",shm_ptr->backlight[0],shm_ptr->backlight[1],shm_ptr->backlight[2]);
                state=0xFF;
            }
            if((shm_ptr->status & SM_FO) && (shm_ptr->effect==SM_EFFECT_NONE)){ //handle focus color change but only if we're not displaying an effect
                for(i=0;i<3;i++) focus[i]=shm_ptr->focus[i];
                printf("focus: R:%i G:%i B:%i\n",shm_ptr->focus[0],shm_ptr->focus[1],shm_ptr->focus[2]);
                state=0xFF;  //force an update of the lock key states since the focus color changed
            }
            if((shm_ptr->status & SM_KEY) && (shm_ptr->effect==SM_EFFECT_NONE)){ //handle focus color change but only if we're not displaying an effect
                if(it829x_init()==0) {
                    for(uint8_t k=0;k<NKEYS;k++){
                        if(shm_ptr->key[k][3]==SM_UPD) it829x_setled(allkeys[k], shm_ptr->key[k]);
                        if(shm_ptr->key[k][3]==SM_BKGND) it829x_setled(allkeys[k], shm_ptr->backlight);
                        if(shm_ptr->key[k][3]==SM_FOCUS) it829x_setled(allkeys[k], shm_ptr->focus);
                        shm_ptr->key[k][3]=SM_NOUPD;
                    }
                it829x_close();
                printf("Updated individual keyboard keys: %i\n",shm_ptr->effect);
                }
                else printf("Error opening connection to USB\n");
            }
            if(shm_ptr->status & SM_ONOFF){ //turn the keyboard backlight on or off
                if(shm_ptr->brightness==0) shm_ptr->brightness=1; //turn on to minimum brightness if it was set at 0 to avoid confusion of whether it changed state
                if(shm_ptr->onoff & SM_TOG) shm_ptr->onoff= (shm_ptr->onoff & SM_ON) ^ SM_ON; //xor for toggle
                if(shm_ptr->onoff==SM_ON){
                    if(it829x_init()==-1 || it829x_brightspeed(shm_ptr->brightness, shm_ptr->speed)==-1) printf("Error setting brightness/speed for on/off state\n"); //keep the same state
                }
                else if(shm_ptr->onoff==SM_OFF){
                    if(it829x_init()==-1 || it829x_brightspeed(0, shm_ptr->speed)==-1) printf("Error setting brightness/speed for on/off state\n"); //keep the same state
                }
                it829x_close();
                printf("Keyboard backlight on/off: %i\n",shm_ptr->onoff);
            }
            
            shm_ptr->status=0; //reset status flag since we just handled them all
            shm_ptr->lastcputime=cputime; //update cpu end time
            sharedmem_unlock();
            }
        }
        endtime = clock(); //set the end time for measureing time spend for keyboard LED update
        if(state==0xFF || lockupdate!=0) {
            cputime = ((double) (endtime - begintime)) / CLOCKS_PER_SEC; //if the keyboard LEDs were updated, update the last loop time
            lockupdate=0; //reset lockupdate flag back to zero
        }
    printf("Exiting... something yet to be discovered did not go as planned and broke out of the while(1) loop!\n");
    sd_notify(0, "STATUS=kbled encountered an unknown fault and is shutting down");
    sharedmem_masterclose(SM_VERBOSE);
    return 1; //tells systemd that there was a fault
}
