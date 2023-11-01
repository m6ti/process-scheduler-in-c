#include "linkedlist.c"
#include "coursework.c"
#include <stdio.h>


int main() {
    Process *process;
    LinkedList *ready;
    struct timeval start, end;

    //start timer.
    gettimeofday(&start, NULL);
    process = generateProcess(0);

    printf("GENERATOR - CREATED: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);

    while (process->iState != TERMINATED) {

        runPreemptiveProcess(process, true);
        printf("SIMULATOR - [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
               process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);

    }
    //end timer for turnaround time.
    gettimeofday(&end, NULL);

    printf("TERMINATOR - TERMINATED: [PID = %d, ResponseTime = %d, TurnAroundTime = %ld]\n",
           process->iPID, 0, getDifferenceInMilliSeconds(start, end));

    destroyProcess(process);
}