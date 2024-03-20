#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <float.h>
#include <limits.h>
//#include "../util.c"

void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);
void * boosterDaemon( void * p);
void * ioDaemon(void *p);
void * loadBalancingDaemon(void *p);

void processInfo(char* event, Process* process);
void queueInfo(char* event, char* status, int size, Process* process, int specifyPriority);
void simulatorReadyInfo(Process* process);
void terminationInfo(Process* process, int counter);
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime);
void finalTerminationInfo(int responseTime, int turnAroundTime);
void boosterCreated();
void boosterInfo(Process* process);
void ioDaemonInfo(Process* process);
void ioInfo(Process* process);
void queueSetInfo(char* event, int size, Process* process, int cpuIndex);
int switchProcessor(int cpuIndex);
int findSmallestQueue(int * queueSizes, int numOfCPUS);
int calculateProcessesToGenerate(int maxConcurrentProcesses, int totalProcesses, int processes,
                                 int totalGeneratedProcesses);
void ioDaemonFinished();
void boosterFinished();
void simulatorFinished();
void simulatorAverageTimes(int cpuId, double responseTime, double turnAroundTime);
void multiSimulatorInfo(Process* process,char * scheduling, int id);

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

sem_t empty, full, sync1, disposalSync, disposalDone;
sem_t syncSemaphores[NUMBER_OF_CPUS];

LinkedList readyQueues[NUMBER_OF_CPUS][NUMBER_OF_PRIORITY_LEVELS] = {{LINKED_LIST_INITIALIZER}};
LinkedList ioQueues[NUMBER_OF_IO_DEVICES] = {LINKED_LIST_INITIALIZER};
LinkedList terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0, totalTurnAroundTime = 0;
int totalResponseTimeEachCPU[NUMBER_OF_CPUS] = {0};
int totalTurnAroundTimeEachCPU[NUMBER_OF_CPUS] = {0};
int processCountEachCPU[NUMBER_OF_CPUS] = {0};
double rollingAvgResponseTime[NUMBER_OF_CPUS] = {0};
double rollingAvgTurnAroundTime[NUMBER_OF_CPUS]  = {0};

int processesTerminated = 0, readyProcesses = 0, blockedProcesses = 0, processesWaitingForTermination = 0;
int readyProcessesEachQueue[NUMBER_OF_CPUS] = {0};
int processesLeftToGenerate = NUMBER_OF_PROCESSES;
int boosterActive = 1, loadBalancerActive = 1, ioDaemonActive = 1, simulatorActive = 1;

ProcessTableEntry processTable[SIZE_OF_PROCESS_TABLE];

int main() {
    // Initialise threads
    pthread_t pGenerator, pTerminator, pBooster ,pioDaemon, loadBalancer;
    if(NUMBER_OF_CPUS <=0 || NUMBER_OF_PROCESSES <= 0 || MAX_CONCURRENT_PROCESSES <= 0) {
        fprintf(stderr, "Initialization Error: NUMBER_OF_CPUS, NUMBER_OF_PROCESSES, "
                        "and MAX_CONCURRENT_PROCESSES must all be greater than 0.\n");
        fprintf(stderr, "Current values - NUMBER_OF_CPUS: %d, NUMBER_OF_PROCESSES: %d, MAX_CONCURRENT_"
                        "PROCESSES: %d\n", NUMBER_OF_CPUS, NUMBER_OF_PROCESSES, MAX_CONCURRENT_PROCESSES);
        exit(EXIT_FAILURE);
    }
    pthread_t pRunners[NUMBER_OF_CPUS];

    // Initialise semaphores
    sem_init(&sync1, 0, 1);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, 1);
    sem_init(&disposalSync, 0, 0);
    sem_init(&disposalDone, 0, 0);
    for (int i = 0; i < NUMBER_OF_CPUS; i++) {
        sem_init(&syncSemaphores[i], 0, 1);
    }

    // Create threads
    pthread_create((&pGenerator), NULL, processGenerator, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++) {
        pthread_create((&pRunners[i]), NULL, processRunner, (void *)(intptr_t) i);
    }
    pthread_create((&pTerminator), NULL, processTerminator, NULL);
    pthread_create((&pBooster), NULL, boosterDaemon, NULL);
    pthread_create((&pioDaemon), NULL, ioDaemon, NULL);
    pthread_create((&loadBalancer), NULL, loadBalancingDaemon, NULL);

    // Join threads
    pthread_join(pGenerator, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++) {
        pthread_join(pRunners[i], NULL);
    }
    pthread_join(pTerminator, NULL);
    pthread_join(pBooster, NULL);
    pthread_join(pioDaemon, NULL);
    pthread_join(loadBalancer, NULL);

    return 0;
}

void *loadBalancingDaemon(void * p) {
    while(loadBalancerActive) {
        // Wait for the interval
        usleep(LOAD_BALANCING_INTERVAL * 1000);
        // Enter critical section
        sem_wait(&sync1);
        // Calculate response time for each CPU and find busiest and least busy CPUs
        int busiestCPU = -1, leastBusyCPU = -1;
        double maxLoad = -1, minLoad = DBL_MAX;
        for (int i = 0; i < NUMBER_OF_CPUS; i++) {
            double load = rollingAvgResponseTime[i];
            if (load > maxLoad) {
                maxLoad = load;
                busiestCPU = i;
            }
            if (load < minLoad) {
                minLoad = load;
                leastBusyCPU = i;
            }
        }
        // Check if load balancing is needed
        if (busiestCPU != -1 && leastBusyCPU != -1 && maxLoad > minLoad) {
            // Randomly select a starting priority level
            int randomStart = rand() % NUMBER_OF_PRIORITY_LEVELS;
            // Iterate through priority levels in a wrap-around manner
            for (int i = 0; i < NUMBER_OF_PRIORITY_LEVELS; i++) {
                int priority = (randomStart + i) % NUMBER_OF_PRIORITY_LEVELS;
                Element* current = getHead(readyQueues[busiestCPU][priority]);
                if (current != NULL) {
                    // Get the process to move from the head of the queue
                    Process* processToMove = current->pData;
                    // Remove the process from the busiest CPU's queue and update count
                    removeFirst(&readyQueues[busiestCPU][priority]);
                    readyProcessesEachQueue[busiestCPU]--;
                    // Add the process to the least busy CPU's queue and update count
                    addLast(processToMove, &readyQueues[leastBusyCPU][priority]);
                    readyProcessesEachQueue[leastBusyCPU]++;
                    // Log the transfer
                    printf("LOAD BALANCER: Moving [PID = %d, Priority = %d] from CPU %d to CPU %d\n",
                           processToMove->iPID, processToMove->iPriority, busiestCPU, leastBusyCPU);
                    break;
                }
            }
        }
        sem_post(&sync1);
    }
    printf("LOAD BALANCER: Finished\n");
    return NULL;
}

void * boosterDaemon( void * p) {
    // Wait for interval
    usleep(BOOST_INTERVAL * 1000);
    boosterCreated();
    while(boosterActive) {
        // Enter critical section
        sem_wait(&sync1);
        for(int i = NUMBER_OF_PRIORITY_LEVELS/2 + 1; i < NUMBER_OF_PRIORITY_LEVELS; i++){
            for(int j = 0; j < NUMBER_OF_CPUS; j++) {
                Element *current = getHead(readyQueues[j][i]);
                while (current != NULL) {
                    Element *next = current->pNext;
                    Process *process = current->pData;
                    // Remove from relevant ready queue
                    removeFirst(&readyQueues[j][i]);
                    readyProcesses--;
                    readyProcessesEachQueue[j]--;
                    queueInfo("QUEUE - REMOVED", "READY", readyProcessesEachQueue[j], process,
                              i);
                    boosterInfo(process);
                    // Add back to ready queue with boosted priority
                    addLast(process, &readyQueues[j][NUMBER_OF_PRIORITY_LEVELS / 2]);
                    readyProcesses++;
                    readyProcessesEachQueue[j]++;
                    queueInfo("QUEUE - ADDED", "READY", readyProcessesEachQueue[j], process,
                              NUMBER_OF_PRIORITY_LEVELS / 2);
                    current = next;
                }
            }
        }
        sem_post(&sync1);
        // Wait for interval
        usleep(BOOST_INTERVAL * 1000);
    }

    boosterFinished();
    return NULL;
}

void *ioDaemon(void *p) {
    while(ioDaemonActive) {
        usleep(IO_DAEMON_INTERVAL * 1000);
        // Ensure mutual exclusion
        sem_wait(&sync1);
        // Process each I/O queue
        for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++) {
            Element *current = getHead(ioQueues[i]);
            while (current != NULL) {
                Element *next = current->pNext;
                // Get the process
                Process *process = current->pData;
                // Free the process and display info
                unblockProcess(process);
                ioDaemonInfo(process);
                // Remove from corresponding I/O queue and update metrics
                removeFirst(&ioQueues[i]);
                blockedProcesses--;
                // Add to the smallest queue with priority.
                int leastIndex = findSmallestQueue(readyProcessesEachQueue,NUMBER_OF_CPUS);
                addFirst(process, &readyQueues[leastIndex][process->iPriority]);
                // Move completed I/O processes back to the ready queue with priority
                readyProcesses++;
                readyProcessesEachQueue[leastIndex]++;
                current = next;
            }
        }
        sem_post(&sync1);

        if(processesLeftToGenerate == 0 ){
            // When all processes have been generated we post the full semaphore.
            sem_post(&full);
        }
    }

    ioDaemonFinished();
    return NULL;
}

void *processGenerator(void *p) {
    Process *tempProcess;
    int cpuIndex = 0;

    while (processesLeftToGenerate > 0) {
        // Enter critical section
        sem_wait(&empty);
        sem_wait(&sync1);
        // Calculate number of processes to generate
        int generateGoal = calculateProcessesToGenerate(MAX_CONCURRENT_PROCESSES,
                                                        NUMBER_OF_PROCESSES, readyProcesses,
                                                        readyProcesses + processesTerminated);
        for(int i = 0; i < generateGoal; i++) {
            // Acquire process IDs from the table
            int pid = getPidFromPool(processTable);
            if (pid == -1) {
                usleep(1000);
                break;
            }
            // Generate process with ID
            tempProcess = generateProcess(pid);
            processInfo("GENERATOR - CREATED", tempProcess);
            processTable[pid].process = tempProcess;
            processInfo("GENERATOR - ADDED TO TABLE", tempProcess);
            // Update metrics
            readyProcessesEachQueue[cpuIndex]++;
            readyProcesses++;
            processesLeftToGenerate--;
            // Add to relevant queue
            addLast(tempProcess, &(readyQueues[cpuIndex][tempProcess->iPriority]));
            queueSetInfo("QUEUE - ADDED", readyProcessesEachQueue[cpuIndex],tempProcess, cpuIndex);
            //Switch to other processor.
            cpuIndex = switchProcessor(cpuIndex);

            processInfo("GENERATOR - ADMITTED", tempProcess);
        }
        sem_post(&sync1);
        sem_post(&full);
    }

    printf("GENERATOR: Finished\n");
    return NULL;
}



void * processRunner( void* p){
    Process *tempProcess;
    long responseTime, turnAroundTime;
    int exitFlag;
    int skipFlag;
    int i;
    int cpuId = (int)(intptr_t) p;

    while(simulatorActive){
        // Wait for generator to finish adding up to max concurrent processes to the queue
        sem_wait(&full);
        sem_wait(&syncSemaphores[cpuId]);

        // Set flags to false
        exitFlag = 0;
        skipFlag = 0;
        i = 0;
        while(simulatorActive && exitFlag == 0){
            // Find the highest priority ready queue to simulate from
            while(getHead(readyQueues[cpuId][i]) == NULL){
                i++;
                if(i >= NUMBER_OF_PRIORITY_LEVELS){
                    // Set flag to exit loop if no processes on current CPU
                    skipFlag = 1;
                    sem_post(&sync1);
                    sem_post(&empty);
                    break;
                }
            }
            if(skipFlag == 1){
                // Exit loop
                break;
            }
            // Retrieve the first process in the ready queue
            tempProcess = (Process *) (getHead(readyQueues[cpuId][i])->pData);
            removeFirst(&readyQueues[cpuId][i]);
            // Set metrics
            readyProcesses--;
            readyProcessesEachQueue[cpuId]--;
            queueInfo("QUEUE - REMOVED", "READY", readyProcessesEachQueue[cpuId],
                      tempProcess, i);
            // Run the process depending on priority.
            if (tempProcess->iPriority > NUMBER_OF_PRIORITY_LEVELS / 2) {
                runPreemptiveProcess(tempProcess, true);
                multiSimulatorInfo(tempProcess, "RR",cpuId);
            } else {
                runNonPreemptiveProcess(tempProcess, true);
                multiSimulatorInfo(tempProcess, "FCFS",cpuId);
            }

            // Check state of process after simulation
            if(tempProcess->iState == TERMINATED) {
                // Calculate and update metrics
                responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,
                                                           tempProcess->oFirstTimeRunning);
                turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,
                                                             tempProcess->oLastTimeRunning);
                totalResponseTime += responseTime;
                totalTurnAroundTime += turnAroundTime;
                // Display info
                simulatorTerminated(tempProcess, responseTime, turnAroundTime);
                // Update metrics for the CPU
                processCountEachCPU[cpuId] ++;
                totalResponseTimeEachCPU[cpuId] += responseTime;
                totalTurnAroundTimeEachCPU[cpuId] += turnAroundTime;
                rollingAvgResponseTime[cpuId] = (double) totalResponseTimeEachCPU[cpuId]/processCountEachCPU[cpuId];
                rollingAvgTurnAroundTime[cpuId] = (double) totalTurnAroundTimeEachCPU[cpuId]/processCountEachCPU[cpuId];
                // Display average response and turnaround times for the CPU
                simulatorAverageTimes(cpuId, rollingAvgResponseTime[cpuId],
                                      rollingAvgTurnAroundTime[cpuId]);

                // Add to the terminated queue.
                addLast(tempProcess, &terminatedQueue);
                queueInfo("QUEUE - ADDED", "TERMINATED", 1, tempProcess, 0);
                processesWaitingForTermination++;
//                simulatorReadyInfo(tempProcess);
                // Set the terminated flag
                exitFlag = 1;
                // Terminate the process
                sem_post(&disposalSync);
                sem_wait(&disposalDone);
            }
            else if(tempProcess->iState == BLOCKED) {
                // If process is waiting on response from IO device, add to the relevant IO queue
                addLast(tempProcess, &ioQueues[tempProcess->iDeviceID]);
                // Set metrics
                blockedProcesses++;
                ioInfo(tempProcess);
                queueInfo("QUEUE - ADDED","I/O",blockedProcesses,tempProcess,
                          tempProcess->iPriority);
            }
            else {
                // If process is ready to run again, add to the end of the ready queue.
                addLast(tempProcess,&readyQueues[cpuId][i]);
                // Set metrics
                readyProcesses++;
                readyProcessesEachQueue[cpuId]++;
                // Display info
                queueInfo("QUEUE - ADDED", "READY",readyProcessesEachQueue[cpuId],tempProcess, 0);
                simulatorReadyInfo(tempProcess);
            }
        }
        sem_post(&syncSemaphores[cpuId]);
        sem_post(&empty);
    }
    // Free up other simulators
    sem_post(&full);

    simulatorFinished();
    return NULL;
}

void * processTerminator(void* p){
    Process *tempProcess;
    // Run until all processes have terminated
    while(processesTerminated!= NUMBER_OF_PROCESSES){
        sem_wait(&disposalSync);
        // Wait until process terminates
        while(getHead(terminatedQueue) != NULL){
            // Remove from queue and terminate
            tempProcess = removeFirst(&terminatedQueue);
            processesTerminated++;
            processesWaitingForTermination--;
            // Display info
            queueInfo("QUEUE - REMOVED","TERMINATED",processesWaitingForTermination,tempProcess,0);
            terminationInfo(tempProcess,processesTerminated);
            // Return PID to enable reuse of ids
            returnToPool(tempProcess->iPID,processTable);
            destroyProcess(tempProcess);
        }
        sem_post(&disposalDone);
    }
    // When all processes have terminated
    loadBalancerActive = 0;
    ioDaemonActive = 0;
    boosterActive = 0;
    simulatorActive = 0;

    finalTerminationInfo(totalResponseTime,totalTurnAroundTime);
    return NULL;
}


// Functions from util.c


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

void multiSimulatorInfo(Process* process,char * scheduling, int id){
    printf("SIMULATOR - CPU %d: %s [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           id, scheduling, process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void simulatorReadyInfo(Process* process) {
    printf("SIMULATOR - CPU 0: READY [PID = %d, Priority = %d]\n",
           process->iPID, process->iPriority);
}
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime){
    printf("SIMULATOR - CPU 0 - TERMINATED: [PID = %d, ResponseTime = %d, TurnAroundTime = %d]\n",
           process->iPID, responseTime, turnaroundTime);
}

void terminationInfo(Process* process, int counter){
    printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d, Priority = %d]\n",
           counter, process->iPID, process->iPriority);
}

void finalTerminationInfo(int responseTime, int turnAroundTime){
    printf("TERMINATION DAEMON: Finished\n");
    printf("TERMINATION DAEMON: [Average Response Time = %d, Average Turn Around Time = %d]\n",
           responseTime / NUMBER_OF_PROCESSES, turnAroundTime / NUMBER_OF_PROCESSES);
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
    printf("I/O DAEMON - UNBLOCKED: [PID = %d, Priority = %d]\n", process->iPID, process->iPriority);
}

void ioInfo(Process* process) {
    printf("SIMULATOR - CPU 0 - I/O BLOCKED: [PID = %d, Priority = %d, Device = %d]\n",
           process->iPID, process->iPriority, process->iDeviceID);
}

int switchProcessor(int cpuIndex) {
    return (cpuIndex + 1) % NUMBER_OF_CPUS;
}

int findSmallestQueue(int * queueSizes, int numOfCPUS) {
    int smallest = INT_MAX;
    int index = 0;
    int i;
    for(i = 0; i < numOfCPUS; i++){
        if(smallest >= queueSizes[i]) {
            smallest = queueSizes[i];
            index = i;
        }
    }
    return index;
}

int calculateProcessesToGenerate(int maxConcurrentProcesses, int totalProcesses, int processes,
                                 int totalGeneratedProcesses) {
    // Calculate the remaining processes to be generated
    int remainingProcesses = totalProcesses - totalGeneratedProcesses;
    // Calculate how many processes can be added without exceeding maxConcurrentProcesses
    int availableSlots = maxConcurrentProcesses - processes;
    // The number of processes to generate is the minimum of available slots and remaining processes
    int processesToGenerate = (availableSlots < remainingProcesses) ? availableSlots : remainingProcesses;
    return processesToGenerate;
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

void simulatorAverageTimes(int cpuId, double responseTime, double turnAroundTime) {
    printf("SIMULATOR - CPU %d: rolling average response time = %.6f, rolling average turnaround "
           "time = %.6f\n", cpuId, responseTime, turnAroundTime);
}