#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);
void * boosterDaemon( void * p);
void *ioDaemon(void *p);
void processInfo(char* event, Process* process);
void queueInfo(char* event, char* status, int size, Process* process, int specifyPriority);
void simulatorInfo(Process* process,char * scheduling);
void simulatorReadyInfo(Process* process);
void terminationInfo(Process* process, int counter);
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime);
void finalTerminationInfo();
void boosterCreated();
void boosterInfo(Process* process);
void ioInfo(Process* process);


sem_t empty, full, sync1, disposalSync, disposalDone;

LinkedList readyQueues[NUMBER_OF_PRIORITY_LEVELS] = {LINKED_LIST_INITIALIZER};
LinkedList ioQueues[NUMBER_OF_IO_DEVICES] = {LINKED_LIST_INITIALIZER};
LinkedList terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0, totalTurnAroundTime = 0;
int processesTerminated = 0, readyProcesses = 0, blockedProcesses = 0;
int processesLeftToGenerate = NUMBER_OF_PROCESSES;

#define SIZE_OF_PROCESS_TABLE MAX_CONCURRENT_PROCESSES

typedef struct {
    int active;
    Process *process;
} ProcessTableEntry;

ProcessTableEntry processTable[SIZE_OF_PROCESS_TABLE];


int main(){
    pthread_t pGenerator, pRunner, pTerminator, pBooster, pIoDaemon;

    sem_init(&sync1, 0, 1);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, 1);
    sem_init(&disposalSync, 0, 0);
    sem_init(&disposalDone, 0, 0);

    pthread_create((&pGenerator), NULL, processGenerator, NULL);
    pthread_create((&pRunner), NULL, processRunner, NULL);
    pthread_create((&pTerminator), NULL, processTerminator, NULL);
    pthread_create((&pBooster), NULL, boosterDaemon, NULL);
    pthread_create((&pIoDaemon), NULL, ioDaemon, NULL);

    pthread_join(pGenerator, NULL);
    pthread_join(pRunner, NULL);
    pthread_join(pTerminator, NULL);
    pthread_join(pBooster, NULL);
    pthread_join(pIoDaemon, NULL);

}

void * boosterDaemon( void * p){
    boosterCreated();
    while(processesTerminated!=NUMBER_OF_PROCESSES) {
        // Wait for interval
        usleep(BOOST_INTERVAL *1000);
        // Wait for sync
        sem_wait(&sync1);
        for(int i = NUMBER_OF_PRIORITY_LEVELS/2 + 1; i < NUMBER_OF_PRIORITY_LEVELS; i++){
            Element* current = getHead(readyQueues[i]);
            while(current != NULL){
                Element* next = current->pNext;
                Process* process = current->pData;
                // Check to see if process hasn't already been boosted.
//                if (shouldBoost(process)) { }
                removeFirst(&readyQueues[i]);
                queueInfo("QUEUE - REMOVED", "READY", readyProcesses, process, i);
                boosterInfo(process);
                queueInfo("QUEUE - ADDED", "READY", readyProcesses, process,
                          NUMBER_OF_PRIORITY_LEVELS/2);
                addLast(process, &readyQueues[NUMBER_OF_PRIORITY_LEVELS / 2]);
                current = next;
            }
        }
        sem_post(&sync1);
    }
    return NULL;
}

void *ioDaemon(void *p) {
    while (processesTerminated!=NUMBER_OF_PROCESSES) {
        usleep(IO_DAEMON_INTERVAL * 1000);
        // Ensure mutual exclusion
        sem_wait(&sync1);
        printf("ENTERING THE IO DAEMON.\n");
        // Process each I/O queue
        for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++) {
            Element* current = getHead(ioQueues[i]);
            while(current != NULL){
                Element* next = current->pNext;
                Process* process = current->pData;
                printf("CLEARING PID:%d\n",process->iPID);
                // Remove from corresponding I/O queue
                removeFirst(&ioQueues[i]);
                blockedProcesses--;
                // Move completed I/O processes back to the ready queue with priority
                addFirst(process, &readyQueues[process->iPriority]);
                readyProcesses++;
                current = next;
            }
        }
        sem_post(&sync1);
    }
    return NULL;
}

int getPidFromPool(){
    for(int i=0; i<MAX_CONCURRENT_PROCESSES;i++){
        if( processTable[i].active == 0 ){
            processTable[i].active = 1;
            return i;
        }
    } return -1;
}

void returnToPool(int pid){
    processTable[pid].active = 0;
    processTable[pid].process = NULL;
}

void *processGenerator(void *p) {
    Process *tempProcess;
    int idTracker = 0;

    while (processesLeftToGenerate > 0) {
        sem_wait(&empty);
        sem_wait(&sync1);

        int numToGenerate = (processesLeftToGenerate < MAX_CONCURRENT_PROCESSES) ? processesLeftToGenerate : MAX_CONCURRENT_PROCESSES;

        printf("\nGenerator Checkpoint: Ready = %d, Terminated = %d, Max Concurrent = %d, To Generate = %d\n", readyProcesses, processesTerminated, MAX_CONCURRENT_PROCESSES, numToGenerate);

        while (readyProcesses < numToGenerate) {
            int pid = getPidFromPool();
            if (pid == -1) {
                printf("ERROR - PID POOL EMPTY\n");
                break;
            }

            tempProcess = generateProcess(pid);
            processTable[pid].process = tempProcess;
            processInfo("GENERATOR - CREATED", tempProcess);
            processInfo("GENERATOR - ADDED TO TABLE", tempProcess);

            readyProcesses++;
            processesLeftToGenerate--;

            addLast(tempProcess, &(readyQueues[tempProcess->iPriority]));
            queueInfo("QUEUE - ADDED", "READY", readyProcesses, tempProcess, tempProcess->iPriority);
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
    int terminatedFlag = 1, blockedFlag = 1;
    int waitForIO = 0;
    int i = 0;

    while(1){
        // Wait for generator to finish adding at most MAX_CONCURRENT_PROCESSES processes to the queue
        sem_wait(&full);
        sem_wait(&sync1);
        while(1){
            /* If no process has terminated in the previous iteration
               then we know there won't be a higher priority process waiting */
            // not for sim6
            waitForIO = 0;
            i = 0;
            while(getHead(readyQueues[i]) == NULL){
                // This finds the highest priority ready queue to simulate from
                printf("Checking %d\n",i);
                i++;
                if(i >= NUMBER_OF_PRIORITY_LEVELS){
                    printf("REACHED %d : ERROR - PRIORITY LEVELS BEYOND MAX\n",i);
                    waitForIO = 1;
                    break;
                }
            }

            if(!waitForIO){
                terminatedFlag = 0;
                blockedFlag = 0;


                // Retrieve the first process in the ready queue
                tempProcess = ((Process *)(getHead(readyQueues[i])->pData));
                removeFirst(&readyQueues[i]);
                queueInfo("QUEUE - REMOVED", "READY", readyProcesses, tempProcess, i);
                readyProcesses--;


                // Run the process depending on priority.
                if (tempProcess->iPriority > NUMBER_OF_PRIORITY_LEVELS / 2) {
                    runPreemptiveProcess(tempProcess, true);
                    simulatorInfo(tempProcess, "RR");
                } else {
                    while (tempProcess->iRemainingBurstTime != 0) {
                        runNonPreemptiveProcess(tempProcess, true);
                        if(tempProcess->iState == BLOCKED) {
                            break;
                        }
                    }
                    if(tempProcess->iState != BLOCKED) {
                        simulatorInfo(tempProcess, "FCFS");
                    }
                    //                printf("%d %ds\n",1==1,tempProcess->iState==TERMINATED);
                }

                // Now, check if terminated or not.
                if(tempProcess->iState == TERMINATED) {
                    // If the process terminates, add to the terminated queue.
                    addLast(tempProcess, &terminatedQueue);
                    // Calculate metrics
                    responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,
                                                               tempProcess->oFirstTimeRunning);
                    turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,
                                                                 tempProcess->oLastTimeRunning);
                    totalResponseTime += responseTime;
                    totalTurnAroundTime += turnAroundTime;
                    // Display info
                    simulatorTerminated(tempProcess, responseTime, turnAroundTime);
                    queueInfo("QUEUE - ADDED", "TERMINATED", 1, tempProcess, 0);
                    simulatorReadyInfo(tempProcess);
                    // Set the terminated flag
                    terminatedFlag = 1;

                    sem_post(&disposalSync);
                    sem_wait(&disposalDone);
                }
                else if(tempProcess->iState == BLOCKED) {
                    addLast(tempProcess, &ioQueues[tempProcess->iDeviceID]);
                    blockedProcesses++;
                    ioInfo(tempProcess);
                    queueInfo("QUEUE - ADDED","I/O",blockedProcesses,tempProcess,
                              tempProcess->iPriority);
                    blockedFlag = 1;
                }
                else{
                    // If the process hasn't terminated, add to the end of the ready queue.
                    addLast(tempProcess,&readyQueues[i]);
                    readyProcesses++;
                    // Display info
                    queueInfo("QUEUE - ADDED", "READY",readyProcesses,tempProcess, 0);
                    simulatorReadyInfo(tempProcess);

                }
            }

            if(terminatedFlag == 1 || blockedFlag == 1){
                // Check if we need to break the loop to attend to a possibly higher priority process
                printf("A process has terminated:)\n");
                // Resetting 'i' ensures we start from ready queue 0
                sem_post(&sync1);
                sem_post(&empty);
                break;
            }
        }
        if(processesTerminated==NUMBER_OF_PROCESSES){
            // Check if completely finished
            break;
        }
    }
    printf("SIMULATOR: Finished\n");
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
            // Display info
            queueInfo("QUEUE - REMOVED","TERMINATED",1,tempProcess,0);
            terminationInfo(tempProcess,processesTerminated);
            // Return PID to enable reuse of ids
            returnToPool(tempProcess->iPID);
            destroyProcess(tempProcess);
        }
        sem_post(&disposalDone);
    }
    // When all processes have terminated
    finalTerminationInfo();
    return NULL;
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

void simulatorInfo(Process* process,char * scheduling){
    printf("SIMULATOR - CPU 0: %s [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           scheduling,process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
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

void finalTerminationInfo(){
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

void ioInfo(Process* process){
    printf("SIMULATOR - CPU 0 - I/O BLOCKED: [PID = %d, Priority = %d, Device = %d]\n",
           process->iPID, process->iPriority, process->iDeviceID);

}