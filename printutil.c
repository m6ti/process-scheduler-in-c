
void processInfo(char* event, Process* process);
void queueInfo(char* event, char* status, int size, Process* process, int specifyPriority);
void simulatorInfo(Process* process,char * scheduling);
void simulatorReadyInfo(Process* process);
void terminationInfo(Process* process, int counter);
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime);
void finalTerminationInfo(int totalResponseTime, int totalTurnAroundTime);
void boosterCreated();
void boosterInfo(Process* process);
void ioDaemonInfo(Process* process);
void ioInfo(Process* process);


void processInfo(char* event, Process* process) {
    printf("%s - [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           event, process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void queueInfo(char* event, char* status, int size, Process* process, int specifyPriority){
    if (specifyPriority == 0 ) {
        printf("%s - [Queue = %s, Size = %d, PID = %d, Priority = %d]\n",
               event, status, size, process->iPID, process->iPriority);
    }
    else{
        printf("%s - [Queue = %s %d, Size = %d, PID = %d, Priority = %d]\n",
               event, status, specifyPriority ,size, process->iPID, process->iPriority);
    }
}

void simulatorInfo(Process* process,char * scheduling){
    printf("SIMULATOR - CPU 0: %s [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           scheduling,process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void multiSimulatorInfo(Process* process,char * scheduling, int id){
    printf("SIMULATOR %d- CPU 0: %s [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",
           id, scheduling,process->iPID, process->iPriority, process->iBurstTime, process->iRemainingBurstTime);
}

void simulatorReadyInfo(Process* process) {
    printf("SIMULATOR - CPU 0: READY [PID = %d, Priority = %d]\n",
           process->iPID, process->iPriority);
}
void simulatorTerminated(Process* process, int responseTime, int turnaroundTime){
    printf("SIMULATOR - CPU 0 - TERMINATED: [PID = %d, ResponseTime = %d, TurnAroundTime = %d]\n",
           process->iPID,responseTime,turnaroundTime);
}

void terminationInfo(Process* process, int counter){
    printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d, Priority = %d]\n",
           counter, process->iPID, process->iPriority);
}

void finalTerminationInfo(int totalResponseTime, int totalTurnAroundTime){
    printf("TERMINATION DAEMON: Finished\n");
    printf("TERMINATION DAEMON: [Average Response Time = %d, Average Turn Around Time = %d]\n",
           totalResponseTime/NUMBER_OF_PROCESSES, totalTurnAroundTime/NUMBER_OF_PROCESSES);
}

void boosterCreated(){
    printf("BOOSTER DAEMON: Created\n");
}

void boosterInfo(Process* process){
    printf("BOOSTER DAEMON: [PID = %d, Priority = %d, InitialBurstTime = %d, RemainingBurstTime = %d]"
           " => Boosted to Level %d\n", process->iPID, process->iPriority, process->iBurstTime,
           process->iRemainingBurstTime, NUMBER_OF_PRIORITY_LEVELS/2);
}

void ioDaemonInfo(Process* process){
    printf("I/O DAEMON - UNBLOCKED: [PID = %d, Priority = %d]\n",process->iPID,process->iPriority);
}

void ioInfo(Process* process) {
    printf("SIMULATOR - CPU 0 - I/O BLOCKED: [PID = %d, Priority = %d, Device = %d]\n",
           process->iPID, process->iPriority, process->iDeviceID);
}