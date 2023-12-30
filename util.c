#include <limits.h>
void processInfo(char* event, Process* process);
void queueInfo(char* event, char* status, int size, Process* process, int specifyPriority);
void simulatorInfo(Process* process,char * scheduling);
void simulatorReadyInfo(Process* process);
void terminationInfo(Process* process, int counter);
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime);
void finalTerminationInfo(int totalResponseTime, int totalTurnAroundTime);
void boosterCreated();
void boosterInfo(Process* process);
void ioDaemonInfo(Process* process);
void ioInfo(Process* process);
void queueSetInfo(char* event, int size, Process* process, int cpuIndex);
int switchProcessor(int cpuIndex);
int findSmallestQueue(int * queueSizes, int cpus);
int calculate_processes_to_generate(int max_concurrent, int total_processes, int current_active, int generated_so_far);
void ioDaemonFinished();
void boosterFinished();
void simulatorFinished();


typedef struct {
    int active;
    Process *process;
} ProcessTableEntry;

#define SIZE_OF_PROCESS_TABLE MAX_CONCURRENT_PROCESSES

int getPidFromPool(ProcessTableEntry* processTable){
    for(int i=0; i<MAX_CONCURRENT_PROCESSES;i++){
        if( processTable[i].active == 0 ){
            processTable[i].active = 1;
            return i;
        }
    } return -1;
}

void returnToPool(int pid, ProcessTableEntry* processTable){
    processTable[pid].active = 0;
    processTable[pid].process = NULL;
}

void processInfo(char* event, Process* process) {
    printf("%s - [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           event, process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void queueInfo(char* event, char* status, int size, Process* process, int specifyPriority){
    if (specifyPriority == 0 ) {
        printf("%s - [Queue = %s, Size = %d, PID = %d, Priority = %d]\n",
               event, status, size, process->iPID, process->iPriority);
    }
    else{
        printf("%s - [Queue = %s %d, Size = %d, PID = %d, Priority = %d]\n",
               event, status, specifyPriority ,size, process->iPID, process->iPriority);
    }
}

void queueSetInfo(char* event, int size, Process* process, int cpuIndex){
    printf("%s - [Queue = SET %d, Size = %d, PID = %d, Priority = %d]\n",
        event, cpuIndex, size, process->iPID, process->iPriority);
}

void simulatorInfo(Process* process,char * scheduling){
    printf("SIMULATOR - CPU : %s [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           scheduling,process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void multiSimulatorInfo(Process* process,char * scheduling, int id){
    printf("SIMULATOR - CPU %d: %s [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           id, scheduling,process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void simulatorReadyInfo(Process* process) {
    printf("SIMULATOR - CPU 0: READY [PID = %d, Priority = %d]\n",
           process->iPID, process->iPriority);
}
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime){
    printf("SIMULATOR - CPU 0 - TERMINATED: [PID = %d, ResponseTime = %d, TurnAroundTime = %d]\n",
           process->iPID,responseTime,turnaroundTime);
}

void terminationInfo(Process* process, int counter){
    printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d, Priority = %d]\n",
           counter, process->iPID, process->iPriority);
}

void finalTerminationInfo(int totalResponseTime, int totalTurnAroundTime){
    printf("TERMINATION DAEMON: Finished\n");
    printf("TERMINATION DAEMON: [Average Response Time = %d, Average Turn Around Time = %d]\n",
           totalResponseTime/NUMBER_OF_PROCESSES, totalTurnAroundTime/NUMBER_OF_PROCESSES);
}

void boosterCreated(){
    printf("BOOSTER DAEMON: Created\n");
}

void boosterInfo(Process* process){
    printf("BOOSTER DAEMON: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]"
           " => Boosted to Level %d\n", process->iPID, process->iPriority, process->iBurstTime,
           process->iRemainingBurstTime, NUMBER_OF_PRIORITY_LEVELS/2);
}

void ioDaemonInfo(Process* process){
    printf("I/O DAEMON - UNBLOCKED: [PID = %d, Priority = %d]\n",process->iPID,process->iPriority);
}

void ioInfo(Process* process) {
    printf("SIMULATOR - CPU 0 - I/O BLOCKED: [PID = %d, Priority = %d, Device = %d]\n",
           process->iPID, process->iPriority, process->iDeviceID);
}

int switchProcessor(int cpuIndex) {
    return (cpuIndex + 1) % NUMBER_OF_CPUS;
}

int findSmallestQueue(int * queueSizes, int cpus) {
    int smallest = INT_MAX;
    int index = 0;
    int i;
    for(i = 0; i < cpus; i++){
        if(smallest >= queueSizes[i]) {
            smallest = queueSizes[i];
            index = i;
//            printf("smallest: %d\n",smallest);
        }
    }
    return index;
}

int calculate_processes_to_generate(int max_concurrent, int total_processes, int current_active, int generated_so_far) {
    // Calculate the remaining processes to be generated
    int remaining_processes = total_processes - generated_so_far;

    // Calculate how many processes can be added without exceeding max_concurrent
    int available_slots = max_concurrent - current_active;

    // The number of processes to generate is the minimum of available slots and remaining processes
    int processes_to_generate = (available_slots < remaining_processes) ? available_slots : remaining_processes;

    return processes_to_generate;
}

void ioDaemonFinished() {
    printf("I/O DAEMON: Finished\n");
}
void boosterFinished() {
    printf("BOOSTER DAEMON: Finished\n");
}
void simulatorFinished() {
    printf("SIMULATOR: Finished\n");
}