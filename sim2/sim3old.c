#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>


void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);

sem_t empty,full,sync1,disposalSync,disposalDone;

LinkedList readyQueue = LINKED_LIST_INITIALIZER, terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0, totalTurnAroundTime = 0;

int processesTerminated = 0, processesLeftToGenerate = NUMBER_OF_PROCESSES, readyProcesses = 0;

int main() {
    // Initialise threads
    pthread_t pGenerator, pRunner, pTerminator;
    if(NUMBER_OF_CPUS <=0 || NUMBER_OF_PROCESSES <= 0 || MAX_CONCURRENT_PROCESSES <= 0) {
        fprintf(stderr, "Initialization Error: NUMBER_OF_CPUS, NUMBER_OF_PROCESSES, "
                        "and MAX_CONCURRENT_PROCESSES must all be greater than 0.\n");
        fprintf(stderr, "Current values - NUMBER_OF_CPUS: %d, NUMBER_OF_PROCESSES: %d, MAX_CONCURRENT_"
                        "PROCESSES: %d\n", NUMBER_OF_CPUS, NUMBER_OF_PROCESSES, MAX_CONCURRENT_PROCESSES);
        exit(EXIT_FAILURE);
    }

    // Initialise semaphores
    sem_init(&sync1, 0, 1);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, 1);
    sem_init(&disposalSync, 0, 0);

    // Create threads
    pthread_create((&pGenerator), NULL, processGenerator, NULL);
    pthread_create((&pRunner), NULL, processRunner, NULL);
    pthread_create((&pTerminator), NULL, processTerminator, NULL);


    // Join threads
    pthread_join(pGenerator, NULL);
    pthread_join(pRunner, NULL);
    pthread_join(pTerminator, NULL);

    return 0;
}
void * processGenerator( void * p) {
    //loop initialises linked list that represents the ready queue with processes.
    Process *tempProcess;
    int idTracker = 0, smaller;

    while(processesLeftToGenerate!=0) {
        sem_wait(&empty);
        sem_wait(&sync1);

        //This should run until there is either max num of concurrent processes or if smaller, the number of processes.
        if(processesLeftToGenerate < MAX_CONCURRENT_PROCESSES)
            smaller = processesLeftToGenerate;
        else
            smaller = MAX_CONCURRENT_PROCESSES;
        while(readyProcesses != smaller) {

            // Generate a process with the PID and add to table.
            tempProcess = generateProcess(idTracker);
            printf("GENERATOR - CREATED: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
                   tempProcess->iPID, tempProcess->iPriority, tempProcess->iBurstTime,
                   tempProcess->iRemainingBurstTime);

            // Set relevant variables
            idTracker++;
            readyProcesses++;
            processesLeftToGenerate--;
            // Add to relevant ready queue
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
    return NULL;
}


void * processRunner( void* p){
    Process *tempProcess;
    long responseTime, turnAroundTime;


    while(processesTerminated != NUMBER_OF_PROCESSES){
        sem_wait(&full);
        sem_wait(&sync1);

        while(getHead(readyQueue) != NULL){
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


                // This tells the terminator to clear the terminated queue.
                sem_post(&disposalSync);

//                sem_wait(&disposalDone);
                /* Removing this lets me maximise parallelism, by letting the simulator and generator run
                instead of making them wait for the terminator */
            }
            else{
                // If the process still requires the CPU and HASN'T terminated, add to the end of the ready queue.
                addLast(tempProcess,&readyQueue);
                readyProcesses++;

                printf("QUEUE - ADDED: [Queue = READY, Size = %d, PID = %d, Priority = %d]\n",
                       readyProcesses,tempProcess->iPID, tempProcess->iPriority);
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

        /* Removing this lets me maximise parallelism, by letting the simulator and generator run
         instead of making them wait for the terminator */
//       sem_post(&disposalDone);
    }
    printf("TERMINATION DAEMON: Finished\n");
    printf("TERMINATION DAEMON: [Average Response Time = %d, Average Turn Around Time = %d]\n",
           totalResponseTime/NUMBER_OF_PROCESSES, totalTurnAroundTime/NUMBER_OF_PROCESSES);

    //Gets out of while loop in process simulator.
    sem_post(&full);
}