#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>


void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);
void processInfo(char* event, Process* process);
void queueInfo(char* event, char* status, int size, Process* process);
void simulatorInfo(Process* process);
void simulatorReadyInfo(Process* process);
void terminationInfo(Process* process, int counter);
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime);
void finalTerminationInfo();

sem_t empty,full,sync1,disposalSync,disposalDone;

LinkedList readyQueues[NUMBER_OF_PRIORITY_LEVELS] = {LINKED_LIST_INITIALIZER};
LinkedList terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0;
int totalTurnAroundTime = 0;

int processesTerminated = 0, readyProcesses = 0;
int processesLeftToGenerate = NUMBER_OF_PROCESSES;

#define SIZE_OF_PROCESS_TABLE MAX_CONCURRENT_PROCESSES

typedef struct {
    int active;
    Process *process;
} ProcessTableEntry;

ProcessTableEntry processTable[SIZE_OF_PROCESS_TABLE];

int getPidFromPool(){
    for(int i=0; i<MAX_CONCURRENT_PROCESSES;i++){
        if( processTable[i].active == 0 ){
            processTable[i].active = 1;
            return i;
        }
    }
    return -1;
}

void returnToPool(int pid){
    processTable[pid].active = 0;
    processTable[pid].process = NULL;
}
int main(){
    pthread_t pGenerator, pRunner, pTerminator;

    sem_init(&sync1, 0, 1);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, 1);
    sem_init(&disposalSync, 0, 0);
    sem_init(&disposalDone, 0, 0);

    pthread_create((&pGenerator), NULL, processGenerator, NULL);
    pthread_create((&pRunner), NULL, processRunner, NULL);
    pthread_create((&pTerminator), NULL, processTerminator, NULL);

    pthread_join(pGenerator, NULL);
    pthread_join(pRunner, NULL);
    pthread_join(pTerminator, NULL);
}

void * processGenerator( void * p){
    //loop initialises linked list that represents the ready queue with processes.
    Process *tempProcess;
    int idTracker = 0, smaller;

    while(processesLeftToGenerate!=0){
        sem_wait(&empty);
        sem_wait(&sync1);

        //This should run until there is either max num of concurrent processes or if smaller, the number of processes.
        if(processesLeftToGenerate < MAX_CONCURRENT_PROCESSES)
            smaller = processesLeftToGenerate;
        else
            smaller = MAX_CONCURRENT_PROCESSES;
        while(readyProcesses != smaller) {
            int pid = getPidFromPool();
            if(pid == -1)
                exit(1);

            tempProcess = generateProcess(pid);
            processTable[pid].process = tempProcess;

//            tempProcess = generateProcess(idTracker);
//            int pid = getPidFromPool();


            processInfo("GENERATOR - CREATED",tempProcess);

            idTracker++;
            readyProcesses++;
            processesLeftToGenerate--;
            addLast(tempProcess, &readyQueue);
            queueInfo("QUEUE - ADDED", "READY", readyProcesses, tempProcess);

            processInfo("GENERATOR - ADMITTED",tempProcess);
        }
        sem_post(&sync1);
        sem_post(&full);
    }
    printf("GENERATOR: Finished\n");
}


void * processRunner( void* p){
    Process *tempProcess;
    long responseTime, turnAroundTime;
    int terminatedFlag = 0;

    while(1){
        sem_wait(&full);
        sem_wait(&sync1);
        while(1){
            tempProcess = ((Process *)(getHead(readyQueue)->pData));

            removeFirst(&readyQueue);
            queueInfo("QUEUE - REMOVED", "READY", readyProcesses, tempProcess);
            readyProcesses--;

            runPreemptiveProcess(tempProcess,true);
            simulatorInfo(tempProcess);

            if(tempProcess->iState == TERMINATED){
                // If the process terminates, add to the terminated queue.
                addLast(tempProcess,&terminatedQueue);

                responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oFirstTimeRunning);
                turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oLastTimeRunning);
                totalResponseTime += responseTime;
                totalTurnAroundTime += turnAroundTime;

                simulatorTerminated(tempProcess,responseTime,turnAroundTime);
                queueInfo("QUEUE - ADDED", "TERMINATED", 1, tempProcess);
                simulatorReadyInfo(tempProcess);
                terminatedFlag = 1;

                sem_post(&disposalSync);

                sem_wait(&disposalDone);
                /* Removing this lets me maximise parallelism, by letting the simulator and generator run
                instead of making them wait for the terminator */
            }
            else{
                // If the process HASN'T terminated, add to the end of the ready queue.
                addLast(tempProcess,&readyQueue);
                readyProcesses++;

                queueInfo("QUEUE - ADDED", "READY",readyProcesses,tempProcess);
                simulatorReadyInfo(tempProcess);
            }

            if(terminatedFlag == 1){
                terminatedFlag = 0;
                sem_post(&sync1);
                sem_post(&empty);
                break;
            }

        }
        if(processesTerminated==NUMBER_OF_PROCESSES){
            break;
        }
    }
    printf("SIMULATOR: Finished\n");
}


void * processTerminator(void* p){
    Process *tempProcess;

    while(processesTerminated!= NUMBER_OF_PROCESSES){

        sem_wait(&disposalSync);
        while(getHead(terminatedQueue) != NULL){
            tempProcess = removeFirst(&terminatedQueue);
            processesTerminated++;

            queueInfo("QUEUE - REMOVED","TERMINATED",1,tempProcess);
            terminationInfo(tempProcess,processesTerminated);

            returnToPool(tempProcess->iPID);

            destroyProcess(tempProcess);
        }

        /* Removing this lets me maximise parallelism, by letting the simulator and generator run
         instead of making them wait for the terminator */
        sem_post(&disposalDone);
    }
    finalTerminationInfo();
    //Gets out of while loop in process simulator.

}

void processInfo(char* event, Process* process) {
    printf("%s - [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           event, process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void queueInfo(char* event, char* status, int size, Process* process){
    printf("%s - [Queue = %s, Size = %d, PID = %d, Priority = %d]\n",
           event, status, size, process->iPID, process->iPriority);
}

void simulatorInfo(Process* process){
    printf("SIMULATOR - CPU 0: RR [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
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

void finalTerminationInfo() {
    printf("TERMINATION DAEMON: Finished\n");
    printf("TERMINATION DAEMON: [Average Response Time = %ld, Average Turn Around Time = %ld]\n",
           totalResponseTime / NUMBER_OF_PROCESSES, totalTurnAroundTime / NUMBER_OF_PROCESSES);
}