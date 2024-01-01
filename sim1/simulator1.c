#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>


int main() {
    Process *process;
    struct timeval start, end, rStart, rEnd;

    process = generateProcess(0);

    //Start timers - for response time and for turnaround time.
    gettimeofday(&start, NULL);
    gettimeofday(&rStart, NULL);

    printf("GENERATOR - CREATED: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);

    while (process->iState != TERMINATED) {
        //End timer for response time (Only one process, and it will access CPU now).
        if (process->iBurstTime==process->iRemainingBurstTime){
            gettimeofday(&rEnd, NULL);
        }

        runPreemptiveProcess(process, true);
        printf("SIMULATOR - [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
               process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);

    }
    //end timer for turnaround time.
    gettimeofday(&end, NULL);

    printf("TERMINATOR - TERMINATED: [PID = %ld, ResponseTime = %ld, TurnAroundTime = %ld]\n",
           process->iPID, getDifferenceInMilliSeconds(rStart, rEnd), getDifferenceInMilliSeconds(start, end));

    destroyProcess(process);
}