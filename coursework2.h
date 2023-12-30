/**************************************************************************************************************************
			- DO NOT CHANGE THE CONTENTS OF THIS FILE FOR YOUR COURSEWORK (APART FROM THE VALUES FOR THE CONSTANTS)
			- ONLY WORK WITH THE OFFICIAL VERSION
**************************************************************************************************************************/

#include <sys/time.h>

typedef enum {false, true} bool;

#define NUMBER_OF_PROCESSES   100 //number of processes to simulate
#define MAX_CONCURRENT_PROCESSES    32 //max number of processes in the system at any one point in time
#define SIZE_OF_PROCESS_TABLE MAX_CONCURRENT_PROCESSES // number of processes in the process table
#define NUMBER_OF_PRIORITY_LEVELS 16
#define NUMBER_OF_CPUS 2  // number of CPU emulators
#define NUMBER_OF_QUEUE_SETS 1  // number of CPU emulators
#define NUMBER_OF_IO_DEVICES 2

#define TIME_SLICE 5 // Duration of the time slice for the round robin algorithm
#define MAX_BURST_TIME 50 // Max CPU time that any one process requires
// process states
#define READY 1
#define RUNNING 2
#define BLOCKED 3
#define TERMINATED 4

#define BOOST_INTERVAL 50
#define IO_DAEMON_INTERVAL 50
#define LOAD_BALANCING_INTERVAL 50

// struct representing a (simplified) process control block
typedef struct {
    struct timeval oTimeCreated;
    struct timeval oFirstTimeRunning;
    struct timeval oLastTimeRunning;

    int iPID; // process identifier, assumed to be positive int up to MAX_VALUE
    int iPriority;
    int iBurstTime; // Initial CPU time required by the process
    int iRemainingBurstTime; // CPU time left for the process
    int iDeviceID;

    int iState; // process state
} Process;

// creates a process control block in dynamic memory. initialises it, and returns it
Process * generateProcess(int iPID);

Process * destroyProcess(Process * pProcess);

// simulates a process running in round robin
void runPreemptiveProcess(Process * pTemp, bool bSimulateIO);

// simulates a process running in round robin
void runNonPreemptiveProcess(Process * pTemp, bool bSimulateIO);

// simulates IO operations
void unblockProcess(Process * pProcess);
// function to calculate the difference between two time stamps
long int getDifferenceInMilliSeconds(struct timeval start, struct timeval end);
