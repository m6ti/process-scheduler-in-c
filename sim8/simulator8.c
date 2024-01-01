#include "../linkedlist.c"
#include "../coursework.c"
#include "../util.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);
void * boosterDaemon( void * p);
void * ioDaemon(void *p);

sem_t empty, full, sync1, disposalSync, disposalDone;

LinkedList readyQueues[NUMBER_OF_PRIORITY_LEVELS] = {LINKED_LIST_INITIALIZER};
LinkedList ioQueues[NUMBER_OF_IO_DEVICES] = {LINKED_LIST_INITIALIZER};
LinkedList terminatedQueue = LINKED_LIST_INITIALIZER;

int totalResponseTime = 0, totalTurnAroundTime = 0;
int processesTerminated = 0, readyProcesses = 0, blockedProcesses = 0;
int processesLeftToGenerate = NUMBER_OF_PROCESSES;
int boosterActive = 1;

ProcessTableEntry processTable[SIZE_OF_PROCESS_TABLE];

int main() {
    // Initialise threads
    pthread_t pGenerator, pTerminator, pBooster ,pioDaemon;
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

    // Create threads
    pthread_create((&pGenerator), NULL, processGenerator, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++){
        pthread_create((&pRunners[i]), NULL, processRunner, (void *)(intptr_t)i);
    }
    pthread_create((&pTerminator), NULL, processTerminator, NULL);
    pthread_create((&pBooster), NULL, boosterDaemon, NULL);
    pthread_create((&pioDaemon), NULL, ioDaemon, NULL);

    // Join threads
    pthread_join(pGenerator, NULL);
    pthread_join(pTerminator, NULL);
    pthread_join(pBooster, NULL);
    pthread_join(pioDaemon, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++){
        pthread_join(pRunners[i], NULL);
    }

    return 0;
}

void * boosterDaemon( void * p) {
    boosterCreated();
    while(boosterActive) {
        // Wait for interval
        usleep(BOOST_INTERVAL *1000);
        // Enter critical section
        sem_wait(&sync1);
        for(int i = NUMBER_OF_PRIORITY_LEVELS/2 + 1; i < NUMBER_OF_PRIORITY_LEVELS; i++){
            Element* current = getHead(readyQueues[i]);
            while(current != NULL){
                Element* next = current->pNext;
                Process* process = current->pData;
                // Remove from relevant ready queue
                removeFirst(&readyQueues[i]);
                readyProcesses--;
                queueInfo("QUEUE - REMOVED", "READY", readyProcesses, process, i);
                boosterInfo(process);
                // Add back to ready queue with boosted priority
                addLast(process, &readyQueues[NUMBER_OF_PRIORITY_LEVELS / 2]);
                readyProcesses++;
                queueInfo("QUEUE - ADDED", "READY", readyProcesses, process,
                          NUMBER_OF_PRIORITY_LEVELS/2);
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
        // Process each I/O queue
        for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++) {
            Element* current = getHead(ioQueues[i]);
            while(current != NULL){
                Element* next = current->pNext;
                // Get the process
                Process* process = current->pData;
                // Free the process and display info
                unblockProcess(process);
                ioDaemonInfo(process);
                // Remove from corresponding I/O queue and update metrics
                removeFirst(&ioQueues[i]);
                blockedProcesses--;
                // Move completed I/O processes back to the ready queue with priority
                addFirst(process, &readyQueues[process->iPriority]);
                readyProcesses++;
                current = next;
            }
        }
        sem_post(&sync1);

        if(processesLeftToGenerate == 0 ){
            // When all processes have been generated we post the full semaphore.
            sem_post(&full);
        }
    }
    return NULL;
}

void *processGenerator(void *p) {
    Process *tempProcess;


    while (processesLeftToGenerate > 0) {
        // Enter critical section
        sem_wait(&empty);
        sem_wait(&sync1);
        // Calculate number of processes to generate
        int generateGoal = calculateProcessesToGenerate(MAX_CONCURRENT_PROCESSES,
                                                        NUMBER_OF_PROCESSES, readyProcesses,
                                                        readyProcesses + processesTerminated);
        for(int i = 0; i < generateGoal; i++) {
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
            readyProcesses++;
            processesLeftToGenerate--;
            // Add to relevant queue
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
    int exitFlag;
    int skipFlag;
    int i = 0;
    int cpuId = (int)(intptr_t) p;

    while(processesTerminated!=NUMBER_OF_PROCESSES){
        // Wait for generator to finish adding at most MAX_CONCURRENT_PROCESSES processes to the queue
        sem_wait(&full);
        sem_wait(&sync1);
        exitFlag = 0;
        skipFlag = 0;
        i = 0;
        while(processesTerminated != NUMBER_OF_PROCESSES && exitFlag == 0){
            while(getHead(readyQueues[i]) == NULL){
                // This finds the highest priority ready queue to simulate from
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
            tempProcess = ((Process *)(getHead(readyQueues[i])->pData));
            removeFirst(&readyQueues[i]);
            queueInfo("QUEUE - REMOVED", "READY", readyProcesses, tempProcess, i);
            readyProcesses--;

            // Run the process depending on priority.
            if (tempProcess->iPriority > NUMBER_OF_PRIORITY_LEVELS / 2) {
                runPreemptiveProcess(tempProcess, true);
                multiSimulatorInfo(tempProcess, "RR",cpuId);
            } else {
                runNonPreemptiveProcess(tempProcess, true);
                multiSimulatorInfo(tempProcess, "FCFS",cpuId);
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
            else{
                // If the process hasn't terminated, add to the end of the ready queue.
                addLast(tempProcess,&readyQueues[i]);
                readyProcesses++;
                // Display info
                queueInfo("QUEUE - ADDED", "READY",readyProcesses,tempProcess, 0);
                simulatorReadyInfo(tempProcess);
            }
        }
        sem_post(&sync1);
        sem_post(&empty);
    }
    printf("SIMULATOR %d: Finished\n",cpuId);
    sem_post(&full);

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
            returnToPool(tempProcess->iPID,processTable);
            destroyProcess(tempProcess);
        }
        sem_post(&disposalDone);
    }
    // When all processes have terminated
    boosterActive = 0;
    finalTerminationInfo(totalResponseTime,totalTurnAroundTime);
    return NULL;
}