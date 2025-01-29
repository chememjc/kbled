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

#define MAX_CORES 128
#define MAX_LINE_LENGTH 1024
#define MAX_IFLEN 32

struct shared_data new_ptr; //internal structure to write to kbled shared memory 
char interface[MAX_IFLEN] = "*"; // update to correct interface from command line arguments, placeholder

void print_usage(char *programname) {
    fprintf(stderr, "Usage: %s [parameters...]\n", programname);
    fprintf(stderr, "Example: %s [parameters...]\n", programname);
    fprintf(stderr, " Parameter:                    Description:\n");
    fprintf(stderr, " -u or --update <msec>         Update process load frequency (100 to 65535 ms)  Default=200 ms\n");
    fprintf(stderr, " -n or --network <interface>   Display network load on /dev/<interface>\n e.g. eth0 for /dev/eth0\n");
    fprintf(stderr, " -b or --bandwidth <mbits>     Define max bandwith of device specified by -n, otherwise determined automatically\n");
    fprintf(stderr, " -r or --ram                   Show RAM saturation\n");
    fprintf(stderr, " -s or --swap                  Show swap saturation\n");
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

//CPU Use     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} CpuTime;

// Function to parse a line from /proc/stat and extract CPU times
int parsecpu(const char *line, CpuTime *cpu_time) {
    return sscanf(line, "cpu%*d %llu %llu %llu %llu %llu %llu %llu %llu",
                  &cpu_time->user, &cpu_time->nice, &cpu_time->system, &cpu_time->idle,
                  &cpu_time->iowait, &cpu_time->irq, &cpu_time->softirq, &cpu_time->steal);
}

// Function to calculate CPU load for all cores
int cpuload(float *cpu_loads, int *num_cores, int avg_time_ms) {
    FILE *file;
    CpuTime prev_times[MAX_CORES], curr_times[MAX_CORES];
    char buffer[MAX_LINE_LENGTH];
    int core_count = 0;

    // First read of /proc/stat
    if ((file = fopen("/proc/stat", "r")) == NULL) {
        perror("fopen");
        return -1;
    }

    // Parse the CPU lines
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "cpu", 3) == 0 && buffer[3] >= '0' && buffer[3] <= '9') {
            if (parsecpu(buffer, &prev_times[core_count]) != 8) {
                fclose(file);
                return -1;
            }
            core_count++;
        }
    }
    fclose(file);

    *num_cores = core_count;

    // Sleep for the specified averaging time
    usleep(avg_time_ms * 1000);

    // Second read of /proc/stat
    if ((file = fopen("/proc/stat", "r")) == NULL) {
        perror("fopen");
        return -1;
    }

    core_count = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "cpu", 3) == 0 && buffer[3] >= '0' && buffer[3] <= '9') {
            if (parsecpu(buffer, &curr_times[core_count]) != 8) {
                fclose(file);
                return -1;
            }
            core_count++;
        }
    }
    fclose(file);

    // Calculate CPU load for each core
    for (int i = 0; i < *num_cores; i++) {
        unsigned long long prev_idle = prev_times[i].idle + prev_times[i].iowait;
        unsigned long long curr_idle = curr_times[i].idle + curr_times[i].iowait;
        unsigned long long prev_total = prev_idle + prev_times[i].user + prev_times[i].nice + prev_times[i].system +
                                         prev_times[i].irq + prev_times[i].softirq + prev_times[i].steal;
        unsigned long long curr_total = curr_idle + curr_times[i].user + curr_times[i].nice + curr_times[i].system +
                                         curr_times[i].irq + curr_times[i].softirq + curr_times[i].steal;
        unsigned long long total_diff = curr_total - prev_total;
        unsigned long long idle_diff = curr_idle - prev_idle;

        if (total_diff == 0) {
            cpu_loads[i] = 0.0f;
        } else {
            cpu_loads[i] = 100.0f * (1.0f - ((float)idle_diff / total_diff));
        }
    }

    return 0;
}
//CPU Use     ------------------------------------------------------------------------------------
//Network Use ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
struct net_stats {
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    time_t last_checked;
};

float max_bandwidth_mbps = 1.0f; // 1 Gbps in Mbps, updated on 
static struct net_stats last_checked_net = {0, 0, 0};

// Function to get bandwidth for wireless interfaces
float getbandwidth_wireless(const char *interface) {
    int sockfd;
    struct iwreq wrq;
    float bandwidth = -1.0f; // Error indicator

    // Open socket to communicate with kernel
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return bandwidth;
    }

    // Prepare the ioctl structure
    memset(&wrq, 0, sizeof(wrq));
    strncpy(wrq.ifr_name, interface, IFNAMSIZ);

    // Get the bit rate
    if (ioctl(sockfd, SIOCGIWRATE, &wrq) < 0) {
        close(sockfd);
        return bandwidth; // Not wireless or error
    }

    // Convert from iw_param to float. Note: iw_param.value is in bps
    bandwidth = (float)wrq.u.bitrate.value / 1e6; // Convert bps to Mbps

    close(sockfd);
    return bandwidth;
}

// Function to get bandwidth for wired interfaces
float getbandwidth_wired(const char *interface) {
    FILE *net_dev = fopen("/proc/net/dev", "r");
    if (net_dev == NULL) {
        perror("Error opening /proc/net/dev");
        return -1.0f;
    }

    char line[MAX_LINE_LENGTH];
    unsigned long long rx_bytes, tx_bytes;
    float bandwidth = 0.0f;

    // Skip header lines
    for (int i = 0; i < 2; ++i) {
        if (fgets(line, sizeof(line), net_dev) == NULL) {
            fclose(net_dev);
            return -1.0f;
        }
    }

    // Find our interface
    while (fgets(line, sizeof(line), net_dev)) {
        char iface_name[MAX_LINE_LENGTH];
        if (sscanf(line, "%[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu", iface_name, &rx_bytes, &tx_bytes) == 3) {
            if (strcmp(iface_name, interface) == 0) {
                time_t now = time(NULL);
                float time_diff = (float)(now - last_checked_net.last_checked);
                
                if (last_checked_net.last_checked == 0) { // First run, we can't calculate bandwidth correctly
                    bandwidth = 0.0f; // Or you could return -1.0f to indicate no valid data
                } else {
                    // Calculate bandwidth in Mbps for both RX and TX
                    float rx_mbps = ((float)(rx_bytes - last_checked_net.rx_bytes) * 8) / (time_diff * 1024 * 1024);
                    float tx_mbps = ((float)(tx_bytes - last_checked_net.tx_bytes) * 8) / (time_diff * 1024 * 1024);
                    bandwidth = rx_mbps + tx_mbps;
                }

                // Update last checked values
                last_checked_net.rx_bytes = rx_bytes;
                last_checked_net.tx_bytes = tx_bytes;
                last_checked_net.last_checked = now;
                
                break;
            }
        }
    }

    fclose(net_dev);
    return bandwidth;
}

// Generalized function to get bandwidth for any interface
float getbandwidth(const char *interface) {
    float bandwidth = getbandwidth_wireless(interface);
    if (bandwidth < 0) { // If it's not wireless or there was an error
        bandwidth = getbandwidth_wired(interface);
    }
    return bandwidth;
}

// Function to get network link saturation as a percentage
int netuse(float *saturation, float *mbps) {
    FILE *net_dev = fopen("/proc/net/dev", "r");
    if (net_dev == NULL) {
        perror("Error opening /proc/net/dev");
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    unsigned long long rx_bytes, tx_bytes;
    *saturation = 0.0f;
    *mbps = 0.0f;

    // Skip header lines
    for (int i = 0; i < 2; ++i) {
        if (fgets(line, sizeof(line), net_dev) == NULL) {
            fclose(net_dev);
            return 1;
        }
    }

    // Find our interface
    while (fgets(line, sizeof(line), net_dev)) {
        char iface_name[MAX_LINE_LENGTH];
        if (sscanf(line, "%[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu", iface_name, &rx_bytes, &tx_bytes) == 3) {
            if (strcmp(iface_name, interface) == 0) {
                time_t now = time(NULL);
                float time_diff = (float)(now - last_checked_net.last_checked);
                
                if (last_checked_net.last_checked == 0 || time_diff == 0) { 
                    // First run or no time has passed, return 0.0 for saturation and bandwidth
                    *saturation = 0.0f;
                    *mbps = 0.0f;
                } else {
                    // Calculate bandwidth in Mbps for both RX and TX
                    float rx_diff = (float)(rx_bytes - last_checked_net.rx_bytes);
                    float tx_diff = (float)(tx_bytes - last_checked_net.tx_bytes);
                    
                    if (rx_diff < 0 || tx_diff < 0) {
                        // Handle potential counter wrap-around or data error
                        *saturation = 0.0f;
                        *mbps = 0.0f;
                    } else {
                        *mbps = ((rx_diff + tx_diff) * 8) / (time_diff * 1024 * 1024);
                        float max_bandwidth_mbps = 864.60f; // Use the actual interface speed from your system

                        if (max_bandwidth_mbps > 0) {
                            *saturation = (*mbps / max_bandwidth_mbps) * 100.0f;
                            if (*saturation > 100.0f) *saturation = 100.0f; // Cap at 100%
                        } else {
                            *saturation = 0.0f;
                        }
                    }
                }

                // Update last checked values
                last_checked_net.rx_bytes = rx_bytes;
                last_checked_net.tx_bytes = tx_bytes;
                last_checked_net.last_checked = now;
                
                break;
            }
        }
    }

    fclose(net_dev);
    return 0; // Success
}
//Network Use ------------------------------------------------------------------------------------
//Memory Use  ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
int memuse(float *usage) {
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo == NULL) {
        perror("Error opening /proc/meminfo");
        return 1;
    }
    long total_mem = 0, free_mem = 0, total_swap = 0, free_swap = 0;
    char line[MAX_LINE_LENGTH], key[MAX_LINE_LENGTH];
    long value;
    // Read relevant information from meminfo
    while (fgets(line, sizeof(line), meminfo)) {
        if (sscanf(line, "%s %ld", key, &value) == 2) {
            if (strcmp(key, "MemTotal:") == 0) {
                total_mem = value;
            } else if (strcmp(key, "MemFree:") == 0) {
                free_mem = value;
            } else if (strcmp(key, "SwapTotal:") == 0) {
                total_swap = value;
            } else if (strcmp(key, "SwapFree:") == 0) {
                free_swap = value;
            }
        }
    }
    fclose(meminfo);
    // Check if we got valid data
    if (total_mem == 0) {
        fprintf(stderr, "Could not retrieve memory information.\n");
        return 1;
    }
    // Calculate RAM usage percentage
    usage[0] = ((float)(total_mem - free_mem) / total_mem) * 100.0f;
    // Handle swap usage calculation
    if (total_swap > 0) { // If there's swap space
        usage[1] = ((float)(total_swap - free_swap) / total_swap) * 100.0f;
    } else {
        usage[1] = 0.0f;  // No swap space, so usage is 0%
    }
    return 0; // Success
}
//Memory Use    ------------------------------------------------------------------------------------

void gradient(float value, float max, float min,  uint8_t bright, uint8_t *target, uint8_t elements){
    //value=variable, max=max possible value, min=min possible value, bright=color intensity 0-255, *target=key index array, elements=#of elements to include from array
    //write color to array for value from min-max
    uint8_t i; //for lops
    //make sure value is within bounds
    if(value<min) value=min;
    else if(value>max) value=max;
    //generate bin size
    float color[3]; //color array
    value += -min; //rerange so 0=minimum
    max += -min; //rearrange so max is reranged as well
    float binsize = (max)/((float) elements); //since everything is reranged, max is the largest value from 0->max
    for(i=0; i<elements; i++){
        if (value<0.0){
            color[0]=0;
            color[1]=0; //green
            color[2]=bright; //blue
            //new_ptr.key[target[i]][3] = SM_BKLT ; //set to backlight color
        }
        else if(value<binsize/2){
            color[0]=0;
            color[1]=(int) (((float) bright)*value/(binsize/2)); //green
            color[2]=((float) bright)-color[1]; //blue
        }
        else if(value<binsize){
            color[0]=(int) (((float) bright)*(value-binsize/2)/(binsize/2)); //red
            color[1]=((float) bright)-color[0]; //green
            color[2]=0.0;
        }
        else { //greater than max, set to max color
            color[0]=bright;
            color[1]=0.0;
            color[2]=0.0;
        }
        if(color[0] != new_ptr.key[target[i]][0] || color[1] != new_ptr.key[target[i]][1] || color[2] != new_ptr.key[target[i]][2]){
            new_ptr.key[target[i]][0]=color[0];
            new_ptr.key[target[i]][1]=color[1];
            new_ptr.key[target[i]][2]=color[2];
            if(new_ptr.key[target[i]][3] != SM_BKLT) new_ptr.key[target[i]][3]=SM_UPD; //set update flag for key unless it is set to backlight mode
        }
        value += -binsize; //decrement the value by binsize for the next indicator
    }
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
    uint8_t ram=0; //flag for showing ram/swap saturation
    uint32_t update = 150; //update time
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
                if(verbose)printf("Change process update frequency to: %s", argv[i + 1]);
                update=atoi(argv[i+1]); //set value
                if(update<100) update=100;
                printf("Process update frequence set to %u ms\n",update);
                i += 2;
            } else {
                fprintf(stderr, "Error: %s requires an argument between 100 and 65535\n",argv[i]);
                return 1;
            }
        }
        else if ((strcmp(argv[i], "-n") == 0) || (strcmp(argv[i], "--network") == 0)) {
            // Set pattern (-1 to 6)
            if (i + 1 < argc && argv[i+1][0]!='/') {
                if(verbose)printf("Set network device to: %s\n", argv[i + 1]);
                strncpy(interface, argv[i+1], MAX_IFLEN-1); //set value, give space for null character
                printf("Network interface set to %s\n",interface);
                i += 2;
            } else {
                fprintf(stderr, "Error: %s expects just the device name, ex: not '/dev/eth0' just 'eth0'\n",argv[i]);
                return 1;
            }
        }
        else if ((strcmp(argv[i], "-b") == 0) || (strcmp(argv[i], "--bandwidth") == 0)) {
            // Set pattern (-1 to 6)
            if (i + 1 < argc && atof(argv[i+1]) >1.0 && atof(argv[i+1]) <- 50000.0) {  //make sure range is greater than 1 megabit and less than or equal to 50 gigabits
                if(verbose)printf("Set network device %s to: %f\n", interface, atof(argv[i + 1]));
                max_bandwidth_mbps = atof(argv[i+1]); //set value
                printf("Network interface speed set to %f Mbps\n",max_bandwidth_mbps);
                i += 2;
            } else {
                fprintf(stderr, "Error: %s expects just the device name, ex: not '/dev/eth0' just 'eth0'\n",argv[i]);
                return 1;
            }
        }
        else if ((strcmp(argv[i], "-r") == 0) || (strcmp(argv[i], "--ram") == 0)) {
            // display ram saturation
            if(verbose)printf("Display RAM saturation:\n");
            ram |= 1;
            i++;
        }
        else if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--swap") == 0)) {
            // display ram saturation
            if(verbose)printf("Display Swap saturation:\n");
            ram |= 2;
            i++;
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
    int cores=0;
    uint8_t cpukeymap[MAX_CORES]={0};
    int keyidx=0;
    float cpu[MAX_CORES]={0};
    float mem[2]; //memory use: 0=mem% 1=swap%
    uint8_t memkeymap[5]={113,94,75,56,37}; //listed min to max :max to min: {37,56,75,94,113}
    uint8_t swapkeymap[5]={114,95,76,57,38}; //listed min to max :max to min: {38,57,76,95,114}
    //uint8_t netkeymap[5]={112,93,74,55,36}; //listed min to max :max to min: {37,56,75,94,113}
    //uint8_t net100keymap[4]={111,92,73,54}; //100 megabit bar graph (right arrow to 7 key)
    //uint8_t netkeymap[16]={96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111}; //bottom row bar
    uint8_t netkeymap[9]={111,112,92,93,73,74,54,55,36};
    
    if(sharedmem_slaveinit(verbose)!=0){
        fprintf(stderr, "Failed to connect to kbled daemon, are you sure it is running?\n");
        return 1;
    }
    if(verbose)printf("Attached to shared memory\n");
    //find out how many cores we are dealing with
    cpuload(cpu, &cores, 100);
    printf("Found %i cores, assigning to keys 0 to %i\n",cores, cores-1);
    for(i=0;i<MAX_CORES;i++){ //load defaults into cpu activity keymap array
        if(i<NKEYS) cpukeymap[i]=i;
        else cpukeymap[MAX_CORES]=NKEYS-1; //out of keys to assign.  You either have a lot of processor cores or not many keys!
    }
    //handle network interface
    if(max_bandwidth_mbps==1.0) max_bandwidth_mbps = getbandwidth(interface); //if bandwidth wasn't set on command line, set it automagically
    if (max_bandwidth_mbps > 0) {
        printf("Bandwidth for %s: %.2f Mbps\n", interface, max_bandwidth_mbps);
    } else {
        printf("Failed to get bandwidth for %s.  Setting to gigabit (1000 megabits)\n", interface);
        max_bandwidth_mbps = 1000.0;
    }
    float saturation,mbps;
    //netuse(&saturation,&mbps);
    if (netuse(&saturation,&mbps) == 0) {
        printf("Network link saturation: %.2f%%\n", saturation);
    } else {
        printf("Failed to get network saturation.\n");
    }
    while(1){
        //usleep((uint32_t)update * 1000); //polling time
        if (cpuload(cpu, &cores, update) != 0) { // Average over <update> ms, also serves to pause between updates
            fprintf(stderr, "Failed to get CPU load\n");
        } else{
            new_ptr.status|=SM_KEY;
            for(i=0;i<32; i++){
                keyidx=cpukeymap[i];
                if(cpu[i]<50.0){
                    new_ptr.key[keyidx][3]=SM_UPD;
                    new_ptr.key[keyidx][0]=0; //red
                    new_ptr.key[keyidx][1]=(int)  (255.0*cpu[i]/50.0); //green
                    new_ptr.key[keyidx][2]=255-new_ptr.key[keyidx][0]; //blue
                } else {
                    new_ptr.key[keyidx][3]=SM_UPD;
                    new_ptr.key[keyidx][0]=(int)  (255.0*(cpu[i]-50.0)/50.0); //red
                    new_ptr.key[keyidx][1]=255-new_ptr.key[keyidx][0]; //green
                    new_ptr.key[keyidx][2]=0; //blue
                }
            }
        }
        if (ram !=0 && memuse(mem) == 0) {
            //printf("RAM used: %.2f%% Swap used: %.2f%%\n", mem[0], mem[1]);
            if(ram & 1) gradient(mem[0], 100.0, 0.0,  255, memkeymap, (uint8_t) sizeof(memkeymap));
            if(ram & 2)gradient(mem[1], 100.0, 0.0,  255, swapkeymap, (uint8_t) sizeof(swapkeymap));
            new_ptr.status |= SM_KEY;
        } else {
            if(ram !=0) printf("Failed to get memory information.\n");
        }
        //Network
        if(interface[0]!='*' && time(NULL)-last_checked_net.last_checked>1){
            if (netuse(&saturation, &mbps) == 0) {
                //printf("Network link saturation: %.2f%% @ %02f mbps\n", saturation, mbps);
                gradient(saturation, 100.0, 0.0,  255, netkeymap, (uint8_t) sizeof(netkeymap));
            } else {
                printf("Failed to get network saturation.\n");
            }
        } //else it is too soon
        
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
