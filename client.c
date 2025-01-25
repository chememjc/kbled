/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * kbledclient - program for sending updates to kbled daemon
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
#include "sharedmem.h"

void print_usage(char *program_name) {
    fprintf(stderr, "Usage: %s [-v] [parameters...]\n", program_name);
    fprintf(stderr, " Parameter:                   Description:\n");
    fprintf(stderr, " -on                          Turn backlight on\n");
    fprintf(stderr, " -off                         Turn backlight off\n");
    fprintf(stderr, " -tog                         Toggle backlight off->on or on->off\n");
    fprintf(stderr, " -v                           Verbose output\n");
    fprintf(stderr, " -b+                          Increase brightness\n");
    fprintf(stderr, " -b-                          Decrease brightness\n");
    fprintf(stderr, " -b <0-10>                    Set brightness (default=10)\n");
    fprintf(stderr, " -s+                          Increase pattern speed\n");
    fprintf(stderr, " -s-                          Decrease pattern speed\n");
    fprintf(stderr, " -s <0-2>                     Set pattern speed (default=0)\n");
    fprintf(stderr, " -p+                          Increment pattern\n");
    fprintf(stderr, " -p-                          Decrement pattern\n");
    fprintf(stderr, " -p <-1 to 6>                 Set pattern, (default=-1 [no pattern])\n");
    fprintf(stderr, " -bl <Red> <Grn> <Blu>        Set global backlight color\n");
    fprintf(stderr, " -fo <Red> <Grn> <Blu>        Set global focus color (caps/num/scroll locks)\n");
    fprintf(stderr, " -c                           Cycle through preset backlight/focus colors\n");
    fprintf(stderr, " -k <LED#> <Red> <Grn> <Blu>  Set individual LED (0-%i) color\n", NKEYS-1);
    fprintf(stderr, " -kb <LED#>                   Set individual LED (0-%i) to backlight color\n", NKEYS-1);
    fprintf(stderr, " -kf <LED#>                   Set individual LED (0-%i) to focus color\n", NKEYS-1);
    fprintf(stderr, " -cpu                         Display the time it took kbled daemon to execute the last update\n");
    fprintf(stderr, " --scan                       Change update speed (1 to 65535 ms) default= 100 ms\n");
    fprintf(stderr, " --dump                       Show contents of shared memory\n");
    fprintf(stderr, " --dump+                      Show contents of shared memory with each key's state\n");
    fprintf(stderr, " -h or --help                Display this message\n");
    fprintf(stderr, " Where <Red> <Grn> <Blu> are 0-255\n");
}

int validrgb(const char *value) {
    int num = atoi(value);
    return (num >= 0 && num <= 255);
}

void printstructure(struct shared_data *data, char type) {
    // Print each member of the structure
    printf("Status: 0x%04x SM_B:%i SM_BI:%i SM_S:%i SM_SI:%i SM_E:%i SM_EI:%i SM_BL:%i SM_FO:%i SM_KEY:%i \nSM_SSPD: %i SM_PALT: %i SM_ONOFF: %i SM_BIT13: %i SM_BIT14: %i SM_BIT15: %i SM_BIT16: %i\n", data->status,
        data->status & 1,(data->status>>1) & 1,(data->status>>2) & 1,(data->status>>3) & 1,(data->status>>4) & 1,(data->status>>5) & 1,(data->status>>6) & 1,(data->status>>7) & 1,(data->status>>8) & 1,
        (data->status>>9) & 1,(data->status>>10) & 1, (data->status>>11) & 1, (data->status>>12) & 1, (data->status>>13) & 1, (data->status>>14) & 1, (data->status>>15) & 1);
    printf("On/Off state: %u\n", data->onoff);
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    struct shared_data new_ptr;
    memset(&new_ptr, 0, sizeof(struct shared_data));
    char verbose = 0; // Flag for verbose output
    char memdump = 0; // Flag for dumping shared memory
    char cputime = 0; // Flag for reporting time spent in last kbled keyboard update event
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            // Verbose output flag
            verbose = 1;
            i++;
        }
        else if (strcmp(argv[i], "-on") == 0) {
            // Increase brightness
            if(verbose)printf("Turn backlight on\n");
            new_ptr.onoff=SM_ON; //increment value
            new_ptr.status |= SM_ONOFF; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-off") == 0) {
            // Increase brightness
            if(verbose)printf("Turn backlight off\n");
            new_ptr.onoff=SM_OFF; //increment value
            new_ptr.status |= SM_ONOFF; //set update flag
            i++;
        }
        else if (strcmp(argv[i], "-tog") == 0) {
            // Increase brightness
            if(verbose)printf("Toggle backlight on/off\n");
            new_ptr.onoff |= SM_TOG; //increment value
            new_ptr.status |= SM_ONOFF; //set update flag
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
            new_ptr.status |= SM_PALT; //set update flag
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
            // Set key to focus color
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
        else if (strcmp(argv[i], "-cpu") == 0) {
            // Cycle through preset backlight/focus colors
            if(verbose)printf("Report time spent by kbled daemon durinng the last keyboard update event\n");
            cputime=1; //report time spent the last time kbled daemon performed keyboard update event
            i++;
        }
        else if (strcmp(argv[i], "--scan") == 0) {
            // set new scan speed for kbled daemon
            if (i + 1 < argc && atoi(argv[i + 1]) >= 1 && atoi(argv[i + 1]) <= 65535) {
                uint16_t scan = atoi(argv[i + 1]);
                if(verbose)printf("Set scan speed to %i ms\n", scan);
                new_ptr.scanspeed=scan; //set update value to focus color
                new_ptr.status |= SM_SSPD; //set update flag
                i += 2;
            } else {
                fprintf(stderr, "Error: --scan must be between 1 and 65535 ms, you specified: %s\n",argv[i + 1]);
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
    
    if(sharedmem_slaveinit(verbose)!=0){
        fprintf(stderr, "Failed to connect to kbled daemon, are you sure it is running?\n");
        return 1;
    }
    if(verbose)printf("Semaphore opened\n");
    // Wait (lock) the semaphore before accessing shared memory and making updates;
    sharedmem_lock(); //lock semaphore **************************************************************************************
    if(new_ptr.status!=0) shm_ptr->status=new_ptr.status;
    if(new_ptr.status & SM_ONOFF)   shm_ptr->onoff=((shm_ptr->onoff & 1) | (new_ptr.onoff & 1)) | (new_ptr.onoff & 2); //preserve state unless changed (first part) and set toggle flag
    if(new_ptr.status & SM_B)       shm_ptr->brightness=new_ptr.brightness;
    if(new_ptr.status & SM_BI)      shm_ptr->brightnessinc=new_ptr.brightnessinc;
    if(new_ptr.status & SM_S)       shm_ptr->speed=new_ptr.speed;
    if(new_ptr.status & SM_SI)      shm_ptr->speedinc=new_ptr.speedinc;
    if(new_ptr.status & SM_E)       shm_ptr->effect=new_ptr.effect;
    if(new_ptr.status & SM_EI)      shm_ptr->effectinc=new_ptr.effectinc;
    if(new_ptr.status & SM_BL)      for(i=0; i<3; i++) shm_ptr->backlight[i]=new_ptr.backlight[i];
    if(new_ptr.status & SM_FO)      for(i=0; i<3; i++) shm_ptr->focus[i]=new_ptr.focus[i];
    if(new_ptr.status & SM_KEY)     for(i=0; i<4; i++) for(int j=0; j<NKEYS; j++) if(new_ptr.key[3]!=0)shm_ptr->key[j][i]=new_ptr.key[j][i];
    if(new_ptr.status & SM_SSPD)    shm_ptr->scanspeed=new_ptr.scanspeed;
    if(memdump) printstructure(shm_ptr,memdump);
    if(cputime) printf("Last kbled daemon LED update time: %f ms, idle loop time %f ns\n", shm_ptr->lastcputime*1000.0,shm_ptr->idlecputime*1000.0);
    sharedmem_unlock(); //unlock semaphore  *********************************************************************************************************
    if(verbose)printf("Semaphore closed\n");
    
    sharedmem_slaveclose(verbose);
    if(verbose)printf("Detached from shared memory\n");

    return 0;
}
