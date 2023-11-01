#include "linkedlist.c"
#include "coursework.c"
#include <stdio.h>
#define NUMBER_OF_PROCESSES 5

int main(){
    Process *tempProcess;
    LinkedList readyQueue = LINKED_LIST_INITIALIZER, terminatedQueue = LINKED_LIST_INITIALIZER;

    long responseTime, turnAroundTime, totalResponseTime = 0 ,totalTurnAroundTime = 0;

    //loop initialises linked list that represents the ready queue with processes.
    for(int i=0;i<NUMBER_OF_PROCESSES;i++){
        tempProcess = generateProcess(i);
        printf("GENERATOR - CREATED: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
               tempProcess->iPID, tempProcess->iPriority, tempProcess->iBurstTime, tempProcess->iRemainingBurstTime);

        addLast(tempProcess,&readyQueue);
        printf("QUEUE - ADDED: [Queue = READY, Size = %d, PID = %d, Priority = %d]\n",
               i + 1,tempProcess->iPID,tempProcess->iPriority);

        printf("GENERATOR - ADMITTED: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
               tempProcess->iPID, tempProcess->iPriority, tempProcess->iBurstTime, tempProcess->iRemainingBurstTime);

    }

    printf("GENERATOR: Finished\n");


    // These variables are initialised to the sizes of the linked lists which represent the different queues.
    int readySize = NUMBER_OF_PROCESSES;
    int terminatedSize = 0;

    while(getHead(readyQueue) != NULL){
        // Remove the head (first) process from the ready queue.
        tempProcess = ((Process *)(getHead(readyQueue)->pData));
        removeFirst(&readyQueue);
        readySize--;
        printf("QUEUE - REMOVED: [Queue = READY, Size = %d, PID = %d, Priority = %d]\n",
               readySize, tempProcess->iPID, tempProcess->iPriority);

        // Run the process.
        runPreemptiveProcess(tempProcess,true);
        printf("SIMULATOR - CPU 0: RR [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
               tempProcess->iPID, tempProcess->iPriority, tempProcess->iBurstTime, tempProcess->iRemainingBurstTime);

        if(tempProcess->iState == TERMINATED){
            // If the process terminates, add to the terminated queue.
            addLast(tempProcess,&terminatedQueue);
            terminatedSize++;

            responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oFirstTimeRunning);
            turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oLastTimeRunning);
            totalResponseTime+=responseTime;
            totalTurnAroundTime+=turnAroundTime;

            printf("SIMULATOR - CPU 0 - TERMINATED: [PID = %d, ResponseTime = %ld, TurnAroundTime = %ld]\n",
                     tempProcess->iPID, responseTime, turnAroundTime);
            printf("QUEUE - ADDED: [Queue = TERMINATED, Size = %d, PID = %d, Priority = %d]\n",
                   terminatedSize, tempProcess->iPID, tempProcess->iPriority);
        }
        else{
            // If the process still requires the CPU and hasn't terminated, add to the end of the ready queue.
            addLast(tempProcess,&readyQueue);
            readySize++;

            printf("QUEUE - ADDED: [Queue = READY, Size = %d, PID = %d, Priority = %d]\n",
                   readySize,tempProcess->iPID, tempProcess->iPriority);
        }

        // CPU is done with the process.
        printf("SIMULATOR - CPU 0 - READY: [PID = %d, Priority = %d]\n",
               tempProcess->iPID, tempProcess->iPriority);
    }
    printf("SIMULATOR: Finished\n");

    int tempPID,tempPriority;
    // Now terminate each process from the terminated queue.
    while(getHead(terminatedQueue) != NULL){
        tempProcess = removeFirst(&terminatedQueue);
        terminatedSize--;
        printf("QUEUE - REMOVED: [Queue = TERMINATED, Size = %d, PID = %d, Priority = %d]\n",
               terminatedSize, tempProcess->iPID, tempProcess->iPriority);
        tempPID = tempProcess->iPID;
        tempPriority = tempProcess->iPriority;
        destroyProcess(tempProcess);
        printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d, Priority = %d]\n",
               NUMBER_OF_PROCESSES-terminatedSize, tempPID, tempPriority);
    }
    printf("TERMINATION DAEMON: Finished\n");
    printf("TERMINATION DAEMON: [Average Response Time = %ld, Average Turn Around Time = %ld]\n",
           totalResponseTime/NUMBER_OF_PROCESSES, totalTurnAroundTime/NUMBER_OF_PROCESSES);

}