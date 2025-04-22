#include "../linkedlist.c"
#include "../coursework.c"
#include "../outputs.c"
#include <stdio.h>


int main() {
    Process *process;
    struct timeval start, end, rStart, rEnd;

    process = generateProcess(0);

    //Start timers - for response time and for turnaround time.
    gettimeofday(&start, NULL);
    gettimeofday(&rStart, NULL);

    output("GENERATOR - CREATED", process);

    while (process->iState != TERMINATED) {
        //End timer for response time (Only one process, and it will access CPU now).
        if (process->iBurstTime <= process->iRemainingBurstTime){
            gettimeofday(&rEnd, NULL);
        }

        runPreemptiveProcess(process, true);
        output("SIMULATOR -", process);
    }

    //end timer for turnaround time.
    gettimeofday(&end, NULL);

    responseTimeOutput("TERMINATOR - TERMINATED:", process,
                       getDifferenceInMilliSeconds(rStart, rEnd),
                       getDifferenceInMilliSeconds(start, end));

    destroyProcess(process);

    return 0;
}