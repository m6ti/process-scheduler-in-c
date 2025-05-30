#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include "../outputs.c"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);

sem_t empty, full, sync1, disposalSync, disposalDone;

LinkedList readyQueue = LINKED_LIST_INITIALIZER, terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0, totalTurnAroundTime = 0;
int processesTerminated = 0, processesLeftToGenerate = NUMBER_OF_PROCESSES, readyProcesses = 0;
int readyToTerminateProcesses = 0, createdProcesses = 0;

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
//    sem_init(&disposalDone, 0, 1);

    // Create thread0
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
    int smaller;

    while(processesLeftToGenerate != 0) {
        sem_wait(&empty);
        sem_wait(&sync1);

        //This should run until there is either max num of concurrent processes or if smaller, the number of processes.
        smaller = processesLeftToGenerate < MAX_CONCURRENT_PROCESSES ? processesLeftToGenerate : MAX_CONCURRENT_PROCESSES;

        while(readyProcesses != smaller) {

            // Generate a process with the PID and add to table.
            tempProcess = generateProcess(createdProcesses);

            output("GENERATOR - CREATED:", tempProcess);

            // Set relevant variables
            readyProcesses++;
            createdProcesses ++;
            processesLeftToGenerate--;

            // Add to relevant ready queue
            addLast(tempProcess, &readyQueue);
            queueOutput("QUEUE - ADDED:", "READY", createdProcesses, tempProcess);

            output("GENERATOR - ADMITTED:", tempProcess);
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


    while(processesTerminated != NUMBER_OF_PROCESSES) {
        sem_wait(&full);
        sem_wait(&sync1);

        while(getHead(readyQueue) != NULL) {
            tempProcess = (Process *) getHead(readyQueue)->pData;
            removeFirst(&readyQueue);
            readyProcesses--;
            queueOutput("QUEUE - REMOVED:", "READY", readyProcesses, tempProcess);

            // Run the process.
            runPreemptiveProcess(tempProcess,true);
            cpuOutput("SIMULATOR - CPU 0: RR", tempProcess);

            if(tempProcess->iState == TERMINATED){
                responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oFirstTimeRunning);
                turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oLastTimeRunning);
                totalResponseTime+=responseTime;
                totalTurnAroundTime+=turnAroundTime;
                responseTimeOutput("SIMULATOR - CPU 0 - TERMINATED:", tempProcess, responseTime, turnAroundTime);

                // If the process terminates, add to the terminated queue.
                addLast(tempProcess,&terminatedQueue);
                readyToTerminateProcesses ++;
                queueOutput("QUEUE - ADDED:", "TERMINATED", 1, tempProcess);

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
                queueOutput("QUEUE - ADDED:", "READY", readyProcesses,tempProcess);

                cpuOutput("SIMULATOR - CPU 0 - READY:", tempProcess);
            }
        }
        sem_post(&sync1);
        sem_post(&empty);
    }
    printf("SIMULATOR: Finished\n");
}


void * processTerminator(void* p){
    int tempPID, tempPriority;
    Process *tempProcess;

    while(readyToTerminateProcesses != 0) {
        sem_wait(&disposalSync);

        while(getHead(terminatedQueue) != NULL) {
            tempProcess = removeFirst(&terminatedQueue);
            processesTerminated ++;
            readyToTerminateProcesses --;

            queueOutput("QUEUE - REMOVED:", "TERMINATED", 1, tempProcess);

            tempPID = tempProcess->iPID;
            tempPriority = tempProcess->iPriority;
            destroyProcess(tempProcess);

            terminationOutput(processesTerminated, tempPID, tempPriority);
        }

       /* Removing this lets me maximise parallelism, by letting the simulator and generator run
        instead of making them wait for the terminator */
//       sem_post(&disposalDone);
    }
    printf("TERMINATION DAEMON: Finished\n");
    averageResponseTimeOutput("TERMINATION DAEMON:", totalResponseTime/NUMBER_OF_PROCESSES, totalTurnAroundTime/NUMBER_OF_PROCESSES);

    //Gets out of while loop in process simulator.
    sem_post(&full);
}

#pragma clang diagnostic pop