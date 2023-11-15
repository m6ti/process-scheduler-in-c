#include "linkedlist.c"
#include "coursework.c"
#include <stdio.h>
#include <sys/types.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>


void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);

//        if(processesLeftToGenerate<MAX_CONCURRENT_PROCESSES){
//            smaller = processesLeftToGenerate;
//        }else{
//            smaller = MAX_CONCURRENT_PROCESSES;
//        }
//
//        for(int i = 0; i < smaller; i++){

sem_t empty,full,sync1,disposalSync,disposalDone;

LinkedList readyQueue = LINKED_LIST_INITIALIZER, terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0, totalTurnAroundTime = 0;

int processesTerminated = 0, processesLeftToGenerate = NUMBER_OF_PROCESSES, readyProcesses = 0, finished = 0;

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

    // *
    // We want to run the process generator as a thread, then once MAX processes generated, SLEEP.

    // Then process runner executes the processes. If process finishes, call process terminator.
    // *
}

void * processGenerator( void * p){
    //loop initialises linked list that represents the ready queue with processes.
    Process *tempProcess;
    int idTracker = 0, smaller;


    while(processesLeftToGenerate!=0){
//        printf("Waiting in : generator");
        sem_wait(&empty);
        sem_wait(&sync1);

        //This should run until there is either max num of concurrent processes or if smaller, the number of processes.
        if(processesLeftToGenerate<MAX_CONCURRENT_PROCESSES)
            smaller = processesLeftToGenerate;
        else
            smaller = MAX_CONCURRENT_PROCESSES;

        while(readyProcesses!=smaller) {

            tempProcess = generateProcess(idTracker);
            idTracker++;

            readyProcesses++;
            processesLeftToGenerate--;

            printf("GENERATOR - CREATED: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
                   tempProcess->iPID, tempProcess->iPriority, tempProcess->iBurstTime,
                   tempProcess->iRemainingBurstTime);

            addLast(tempProcess, &readyQueue);
            printf("QUEUE - ADDED: [Queue = READY, Size = %d, PID = %d, Priority = %d]\n",
                   idTracker, tempProcess->iPID, tempProcess->iPriority);

            printf("GENERATOR - ADMITTED: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
                   tempProcess->iPID, tempProcess->iPriority, tempProcess->iBurstTime,
                   tempProcess->iRemainingBurstTime);
        }
        sem_post(&sync1);
        sem_post(&full);

    }

    printf("GENERATOR: Finished\n");
}


void * processRunner( void* p){
    Process *tempProcess;
    long responseTime, turnAroundTime;
    int terminated;

    //While all processes have not been terminated...
    while(processesTerminated != NUMBER_OF_PROCESSES){
        terminated = 0;
//        printf("P terminated: %d",processesTerminated);
        sem_wait(&full);
        sem_wait(&sync1);
        while(!terminated && getHead(readyQueue) != NULL){

            tempProcess = ((Process *)(getHead(readyQueue)->pData));
            removeFirst(&readyQueue);

            readyProcesses--;

            printf("QUEUE - REMOVED: [Queue = READY, Size = %d, PID = %d, Priority = %d]\n",
                   readyProcesses, tempProcess->iPID, tempProcess->iPriority);

            // Run the process.
            runPreemptiveProcess(tempProcess,true);
            printf("SIMULATOR - CPU 0: RR [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
                   tempProcess->iPID, tempProcess->iPriority, tempProcess->iBurstTime, tempProcess->iRemainingBurstTime);

            if(tempProcess->iState == TERMINATED){
                // If the process terminates, add to the terminated queue.
                addLast(tempProcess,&terminatedQueue);

                responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oFirstTimeRunning);
                turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oLastTimeRunning);
                totalResponseTime+=responseTime;
                totalTurnAroundTime+=turnAroundTime;

                printf("SIMULATOR - CPU 0 - TERMINATED: [PID = %d, ResponseTime = %ld, TurnAroundTime = %ld]\n",
                       tempProcess->iPID, responseTime, turnAroundTime);
                printf("QUEUE - ADDED: [Queue = TERMINATED, Size = %d, PID = %d, Priority = %d]\n",
                       1, tempProcess->iPID, tempProcess->iPriority);

                // CPU is done with the process.
                printf("SIMULATOR - CPU 0 - READY: [PID = %d, Priority = %d]\n",
                       tempProcess->iPID, tempProcess->iPriority);

                // *
                // *IMPLEMENT*  -- RUN TERMINATOR !!!

                sem_post(&disposalSync);
                sem_wait(&disposalDone);
                // *

                terminated = 1;
            }
            else{
                // If the process still requires the CPU and hasn't terminated, add to the end of the ready queue.
                addLast(tempProcess,&readyQueue);
                readyProcesses++;

                printf("QUEUE - ADDED: [Queue = READY, Size = %d, PID = %d, Priority = %d]\n",
                       readyProcesses,tempProcess->iPID, tempProcess->iPriority);

                // CPU is done with the process.
                printf("SIMULATOR - CPU 0 - READY: [PID = %d, Priority = %d]\n",
                       tempProcess->iPID, tempProcess->iPriority);
            }
        }
        sem_post(&sync1);
        sem_post(&empty);
    }
    printf("SIMULATOR: Finished\n");
}


void * processTerminator(void* p){
    int tempPID,tempPriority;

    Process *tempProcess;


    while(processesTerminated!= NUMBER_OF_PROCESSES){

        sem_wait(&disposalSync);
        //length of terminatedQueue should be 1.
        while(getHead(terminatedQueue) != NULL){
            tempProcess = removeFirst(&terminatedQueue);
            processesTerminated++;

            printf("QUEUE - REMOVED: [Queue = TERMINATED, Size = %d, PID = %d, Priority = %d]\n",
                   1, tempProcess->iPID, tempProcess->iPriority);
            tempPID = tempProcess->iPID;
            tempPriority = tempProcess->iPriority;
            destroyProcess(tempProcess);
            printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d, Priority = %d]\n",
                   processesTerminated, tempPID, tempPriority);
        }

        sem_post(&disposalDone);
    }
    printf("TERMINATION DAEMON: Finished\n");
    printf("TERMINATION DAEMON: [Average Response Time = %d, Average Turn Around Time = %d]\n",
            totalResponseTime/NUMBER_OF_PROCESSES, totalTurnAroundTime/NUMBER_OF_PROCESSES);
}