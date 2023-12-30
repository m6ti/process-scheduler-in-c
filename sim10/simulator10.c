#include "../linkedlist.c"
#include "../coursework.c"
#include "../util.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <float.h>

void * processGenerator(void* p);
void * processRunner(void* p);
void * processTerminator(void* p);
void * boosterDaemon( void * p);
void * ioDaemon(void *p);
void * loadBalancingDaemon(void *p);
void removeElement(LinkedList *pList, Element *pElement);

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

int processesTerminated = 0, readyProcesses = 0, blockedProcesses = 0;
int readyProcessesEachQueue[NUMBER_OF_CPUS] = {0};
int processesLeftToGenerate = NUMBER_OF_PROCESSES;
int boosterActive = 1;



ProcessTableEntry processTable[SIZE_OF_PROCESS_TABLE];

int main() {
    pthread_t pGenerator, pTerminator, pBooster ,pioDaemon, loadBalancer;
    pthread_t pRunners[NUMBER_OF_CPUS];

    sem_init(&sync1, 0, 1);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, 1);
    sem_init(&disposalSync, 0, 0);
    sem_init(&disposalDone, 0, 0);
    for (int i = 0; i < NUMBER_OF_CPUS; i++) {
        sem_init(&syncSemaphores[i], 0, 1);
    }

    pthread_create((&pGenerator), NULL, processGenerator, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++){
        pthread_create((&pRunners[i]), NULL, processRunner, (void *)(intptr_t) i);
    }
    pthread_create((&pTerminator), NULL, processTerminator, NULL);
    pthread_create((&pBooster), NULL, boosterDaemon, NULL);
    pthread_create((&pioDaemon), NULL, ioDaemon, NULL);
    pthread_create((&loadBalancer), NULL, loadBalancingDaemon, NULL);


    pthread_join(pGenerator, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++){
        pthread_join(pRunners[i], NULL);
    }
    pthread_join(pTerminator, NULL);
    pthread_join(pBooster, NULL);
    pthread_join(pioDaemon, NULL);
    pthread_join(loadBalancer, NULL);

    return 0;
}

void *loadBalancingDaemon(void * p) {
    while (processesTerminated != NUMBER_OF_PROCESSES) {
        usleep(LOAD_BALANCING_INTERVAL * 1000);

        sem_wait(&sync1);

        int busiestCPU = -1, leastBusyCPU = -1;
        double maxLoad = -1, minLoad = DBL_MAX;
        // Calculate response time for each CPU and find busiest and least busy CPUs
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
        // Wait for sync
        sem_wait(&sync1);
        for(int i = NUMBER_OF_PRIORITY_LEVELS/2 + 1; i < NUMBER_OF_PRIORITY_LEVELS; i++){
            for(int j = 0; j < NUMBER_OF_CPUS; j++) {
                Element *current = getHead(readyQueues[j][i]);
                while (current != NULL) {
                    Element *next = current->pNext;
                    Process *process = current->pData;
                    removeFirst(&readyQueues[j][i]);
                    readyProcesses--;
                    readyProcessesEachQueue[j]--;
                    queueInfo("QUEUE - REMOVED", "READY", readyProcessesEachQueue[j], process, i);
                    boosterInfo(process);
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
    while (processesTerminated!=NUMBER_OF_PROCESSES) {
        usleep(IO_DAEMON_INTERVAL * 1000);
        // Ensure mutual exclusion
        sem_wait(&sync1);
//        printf("entering io daemon \n");
        // Process each I/O queue
        for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++) {
            Element *current = getHead(ioQueues[i]);
            while (current != NULL) {
                Element *next = current->pNext;
                Process *process = current->pData;
                ioDaemonInfo(process);
                // Remove from corresponding I/O queue
                removeFirst(&ioQueues[i]);
                blockedProcesses--;
                int leastIndex = findSmallestQueue(readyProcessesEachQueue,NUMBER_OF_CPUS);
                // Add to smallest queue.
//                printf("smallest queue: %d\n",leastIndex);
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
        sem_wait(&empty);
        sem_wait(&sync1);

        int generateGoalProvisional = calculate_processes_to_generate(MAX_CONCURRENT_PROCESSES,NUMBER_OF_PROCESSES,
                                                                      readyProcesses,readyProcesses+processesTerminated);
//        printf("\nGenerator Checkpoint: Ready = %d, Terminated = %d, Max Concurrent = %d, GenerateGoal = %d\n\n",
//               readyProcesses, processesTerminated, MAX_CONCURRENT_PROCESSES, generateGoalProvisional);

        for(int i = 0; i < generateGoalProvisional; i++) {
            int pid = getPidFromPool(processTable);
            if (pid == -1) {
                usleep(1000);
                break;
            }

            tempProcess = generateProcess(pid);
            processTable[pid].process = tempProcess;
            processInfo("GENERATOR - CREATED", tempProcess);
            processInfo("GENERATOR - ADDED TO TABLE", tempProcess);
            readyProcessesEachQueue[cpuIndex]++;
//            printf("num of ready processes for %d is %d\n",cpuIndex,readyProcessesEachQueue[cpuIndex]);
            readyProcesses++;
            processesLeftToGenerate--;

            addLast(tempProcess, &(readyQueues[cpuIndex][tempProcess->iPriority]));
            queueSetInfo("QUEUE - ADDED", readyProcessesEachQueue[cpuIndex],tempProcess, cpuIndex);

            //Switch to other processor.
            cpuIndex = switchProcessor(cpuIndex);

            processInfo("GENERATOR - ADMITTED", tempProcess);
        }

        sem_post(&sync1);
        sem_post(&full);
    }
//    printf("generated processes: %d",NUMBER_OF_PROCESSES-processesLeftToGenerate);
    printf("GENERATOR: Finished\n");
    return NULL;
}



void * processRunner( void* p){
    Process *tempProcess;
    long responseTime, turnAroundTime;
    int exitFlag;
    int skipFlag;
    int i = 0;
    int cpuId = (int)(intptr_t)p;


    while(processesTerminated!=NUMBER_OF_PROCESSES){
        // Wait for generator to finish adding at most MAX_CONCURRENT_PROCESSES processes to the queue
//        printf("sim %d waiting for full semaphore\n",cpuId);
        sem_wait(&full);
//        printf("sim %d waiting for sync semaphore\n",cpuId);
        sem_wait(&syncSemaphores[cpuId]);
//        printf("sim %d got past both semaphores\n",cpuId);
        // Set flags to false
        exitFlag = 0;
        skipFlag = 0;
        while(processesTerminated != NUMBER_OF_PROCESSES && exitFlag == 0){
            // Find the highest priority ready queue to simulate from
            while(getHead(readyQueues[cpuId][i]) == NULL){
                i++;
                if(i >= NUMBER_OF_PRIORITY_LEVELS){
                    // Set flag to exit loop
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
            tempProcess = ((Process *)(getHead(readyQueues[cpuId][i])->pData));
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
                while (tempProcess->iRemainingBurstTime != 0) {
                    runNonPreemptiveProcess(tempProcess, true);
                }
                multiSimulatorInfo(tempProcess, "FCFS",cpuId);
            }

            // Check state of process after simulation
            if(tempProcess->iState == TERMINATED) {
                // Calculate metrics
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
                printf("SIMULATOR - CPU %d: rolling average response time = %.6f, rolling average turnaround "
                       "time = %.6f\n", cpuId, rollingAvgResponseTime[cpuId], rollingAvgTurnAroundTime[cpuId]);
                // Add to the terminated queue.
                addLast(tempProcess, &terminatedQueue);
                queueInfo("QUEUE - ADDED", "TERMINATED", 1, tempProcess, 0);
                simulatorReadyInfo(tempProcess);
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
//                exitFlag = 1;
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
        i = 0;
        sem_post(&syncSemaphores[cpuId]);
        sem_post(&empty);
    }
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