//#include "linkedlist.c"
//#include "coursework.c"
#include <stdio.h>

void output(char* output, Process* process);
void responseTime(char* output, Process* process);
void queueOutput(char* output, char* queue,  int size, Process* process);
void cpuOutput(char* cpu, Process* process);
void terminationOutput(int size, int pid, int priority);
void averageResponseTimeOutput(char* output, long responseTime, long turnAroundTime);


void output(char* output, Process* process) {
    printf("%s [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           output, process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void responseTimeOutput(char* output, Process* process, long responseTime, long turnAroundTime) {
    printf("%s [PID = %d, ResponseTime = %ld, TurnAroundTime = %ld]\n", output,
           process->iPID, responseTime, turnAroundTime);
}

void queueOutput(char* output, char* queue, int size, Process* process) {
    printf("%s: [Queue = %s, Size = %d, PID = %d, Priority = %d]\n",
           output, queue, size, process->iPID, process->iPriority);
}

void cpuOutput(char* cpu, Process* process){
    printf("%s [PID = %d, Priority = %d]\n", cpu, process->iPID, process->iPriority);
}

void terminationOutput(int size, int pid, int priority){
    printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d, Priority = %d]\n",
           size, pid, priority);

}

void averageResponseTimeOutput(char* output, long responseTime, long turnAroundTime) {
    printf("%s [Average Response Time = %ld, Average Turn Around Time = %ld]\n", output, responseTime, turnAroundTime);
}
