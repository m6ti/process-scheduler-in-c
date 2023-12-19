#include "../linkedlist.c"
#include "../coursework.c"
#include "../printutil.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>


void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);

sem_t empty, full, sync1, disposalSync, disposalDone;

LinkedList readyQueue = LINKED_LIST_INITIALIZER, terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0, totalTurnAroundTime = 0;

int processesTerminated = 0, processesLeftToGenerate = NUMBER_OF_PROCESSES, readyProcesses = 0;

int main(){
    pthread_t pGenerator, pTerminator;
    pthread_t pRunners[NUMBER_OF_CPUS];

    sem_init(&sync1, 0, 1);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, 1);
    sem_init(&disposalSync, 0, 0);
    sem_init(&disposalDone, 0, 0);


    pthread_create((&pGenerator), NULL, processGenerator, NULL);
    pthread_create((&pTerminator), NULL, processTerminator, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++){
        pthread_create((&pRunners[i]), NULL, processRunner, (void *)(intptr_t)i);
    }

    pthread_join(pGenerator, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++){
        pthread_join(pRunners[i], NULL);
    }
    pthread_join(pTerminator, NULL);
}

void * processGenerator( void * p){
    //loop initialises linked list that represents the ready queue with processes.
    Process *tempProcess;
    int idTracker = 0;

    while (processesLeftToGenerate > 0) {
        sem_wait(&empty);
        sem_wait(&sync1);
        int numToGenerate = (processesLeftToGenerate < MAX_CONCURRENT_PROCESSES) ? processesLeftToGenerate : MAX_CONCURRENT_PROCESSES;

        while (readyProcesses < numToGenerate) {








            tempProcess = generateProcess(idTracker);
            idTracker++;
            processInfo("GENERATOR - CREATED",tempProcess);

            readyProcesses++;
            processesLeftToGenerate--;

            addLast(tempProcess, &readyQueue);
            queueInfo("QUEUE - ADDED", "READY", readyProcesses, tempProcess,
                      0);
            processInfo("GENERATOR - ADMITTED",tempProcess);
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
    int cpuId = (int)(intptr_t)p;

    while(processesTerminated!=NUMBER_OF_PROCESSES){
        // Wait for generator to finish adding at most MAX_CONCURRENT_PROCESSES processes to the queue
        printf("sim %d waiting for full semaphore\n",cpuId);
        sem_wait(&full);
        printf("sim %d waiting for sync semaphore\n",cpuId);
        sem_wait(&sync1);
        printf("sim %d got past both semaphores\n",cpuId);
        exitFlag = 0;
        while(processesTerminated!=NUMBER_OF_PROCESSES && exitFlag == 0){
            tempProcess = ((Process *)(getHead(readyQueue)->pData));
            removeFirst(&readyQueue);
            queueInfo("QUEUE - REMOVED", "READY", readyProcesses, tempProcess, 0);
            readyProcesses--;


            // Run the process.
            runPreemptiveProcess(tempProcess,false);

            multiSimulatorInfo(tempProcess, "RR", cpuId);

            if(tempProcess->iState == TERMINATED){
                // If the process terminates, add to the terminated queue.
                addLast(tempProcess,&terminatedQueue);
                // Calculate metrics
                responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oFirstTimeRunning);
                turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oLastTimeRunning);

                totalResponseTime+=responseTime;
                totalTurnAroundTime+=turnAroundTime;
                // Display info
                simulatorTerminated(tempProcess, responseTime, turnAroundTime);
                queueInfo("QUEUE - ADDED", "TERMINATED", 1, tempProcess, 0);
                simulatorReadyInfo(tempProcess);
                // Set the terminated flag
                exitFlag = 1;
                sem_post(&disposalSync);
                sem_wait(&disposalDone);
            }
            else{
                // If the process still requires the CPU and HASN'T terminated, add to the end of the ready queue.
                addLast(tempProcess,&readyQueue);
                readyProcesses++;
                // Display info
                queueInfo("QUEUE - ADDED", "READY",readyProcesses,tempProcess, 0);
                simulatorReadyInfo(tempProcess);

            }
        }
        //If a process has terminated, we let the process generator generate another.
        sem_post(&sync1);
        sem_post(&empty);
    }
    printf("SIMULATOR %d: Finished\n",cpuId);
    return NULL;
}


void * processTerminator(void* p){
    Process *tempProcess;
    // Run until all processes have terminated
    while(processesTerminated!= NUMBER_OF_PROCESSES){
        // Wait until process terminates
        sem_wait(&disposalSync);
        while(getHead(terminatedQueue) != NULL){
            // Remove from queue and terminate
            tempProcess = removeFirst(&terminatedQueue);
            processesTerminated++;
            // Display info
            queueInfo("QUEUE - REMOVED","TERMINATED",1,tempProcess,0);
            terminationInfo(tempProcess,processesTerminated);
            destroyProcess(tempProcess);
        }
       sem_post(&disposalDone);
    }
    finalTerminationInfo(totalResponseTime,totalTurnAroundTime);
    //Gets out of while loop in process simulator.
    sem_post(&sync1);
    sem_post(&full);
    return NULL;
}