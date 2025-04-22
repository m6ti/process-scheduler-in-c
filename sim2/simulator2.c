#include "../linkedlist.c"
#include "../coursework.c"
#include "../outputs.c"

#include <stdio.h>

int main(){
    Process *tempProcess;
    LinkedList readyQueue = LINKED_LIST_INITIALIZER, terminatedQueue = LINKED_LIST_INITIALIZER;

    long responseTime, turnAroundTime, totalResponseTime = 0 ,totalTurnAroundTime = 0;

    //loop initialises linked list that represents the ready queue with processes.
    for(int i=0;i<NUMBER_OF_PROCESSES;i++){
        tempProcess = generateProcess(i);
        output("GENERATOR - CREATED", tempProcess);

        addLast(tempProcess,&readyQueue);
        queueOutput("QUEUE - ADDED:", "READY", i + 1, tempProcess);

        output("GENERATOR - ADMITTED:", tempProcess);
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
        queueOutput("QUEUE - REMOVED:", "READY", readySize, tempProcess);

        // Run the process.
        runPreemptiveProcess(tempProcess,true);
        output("SIMULATOR - CPU 0:", tempProcess);

        if(tempProcess->iState == TERMINATED){
            // If the process terminates, add to the terminated queue.
            addLast(tempProcess,&terminatedQueue);
            terminatedSize++;

            responseTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oFirstTimeRunning);
            turnAroundTime = getDifferenceInMilliSeconds(tempProcess->oTimeCreated,tempProcess->oLastTimeRunning);
            totalResponseTime+=responseTime;
            totalTurnAroundTime+=turnAroundTime;

            output("SIMULATOR - CPU 0 - TERMINATED:", tempProcess);

            queueOutput("QUEUE - ADDED:", "TERMINATED", terminatedSize, tempProcess);
        }
        else{
            // If the process still requires the CPU and hasn't terminated, add to the end of the ready queue.
            addLast(tempProcess,&readyQueue);
            readySize++;

            queueOutput("QUEUE - ADDED:", "READY", readySize, tempProcess);
        }

        // CPU is done with the process.
        cpuOutput("SIMULATOR - CPU 0 - READY:", tempProcess);
    }

    printf("SIMULATOR: Finished\n");

    int tempPID,tempPriority;
    // Now terminate each process from the terminated queue.
    while(getHead(terminatedQueue) != NULL){
        tempProcess = removeFirst(&terminatedQueue);
        terminatedSize--;
        queueOutput("QUEUE - REMOVED:", "TERMINATED", terminatedSize, tempProcess);

        tempPID = tempProcess->iPID;
        tempPriority = tempProcess->iPriority;
        destroyProcess(tempProcess);
        terminationOutput(NUMBER_OF_PROCESSES - terminatedSize, tempPID, tempPriority);
    }
    printf("TERMINATION DAEMON: Finished\n");

    averageResponseTimeOutput("TERMINATION DAEMON:", totalResponseTime/NUMBER_OF_PROCESSES, totalTurnAroundTime/NUMBER_OF_PROCESSES);
}