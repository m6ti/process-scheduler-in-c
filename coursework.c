/**************************************************************************************************************************
      DO NOT CHANGE THE CONTENTS OF THIS FILE FOR YOUR COURSEWORK. ONLY WORK WITH THE OFFICIAL VERSION
**************************************************************************************************************************/
#include <stdlib.h>
#include <sys/time.h>
#include "coursework.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// Macro to initialise a process struct (Process control block)
#define PROCESS_INITIALIZER {.iPID = -1, .iBurstTime = 0, .iRemainingBurstTime = 0, .iState = READY}
//
Process * generateProcess(int iPID)
{  
  Process * pProcess = (Process *) malloc (sizeof(Process));
  if(pProcess == NULL)
  {
    printf("Malloc Failed\n");
    exit(-1);
  }
  *pProcess = (Process) PROCESS_INITIALIZER;
  gettimeofday(&pProcess->oTimeCreated, NULL);
  pProcess->iPID = iPID;
  pProcess->iRemainingBurstTime = pProcess->iBurstTime = (rand() % MAX_BURST_TIME) + 1;
  pProcess->iPriority = rand() % NUMBER_OF_PRIORITY_LEVELS;
  return pProcess;
}

Process * destroyProcess(Process * pProcess) 
{
  free(pProcess);
}

void runProcess(Process * pProcess, int iBurstTime, bool bSimulateIO) {
  pProcess->iState = RUNNING;
  if(pProcess->iBurstTime == pProcess->iRemainingBurstTime) {
    gettimeofday(&pProcess->oFirstTimeRunning, NULL);
  }
  if(bSimulateIO && rand() % 10 == 1) {
    pProcess->iState = BLOCKED;noten
    pProcess->iDeviceID = rand() % NUMBER_OF_IO_DEVICES;
    return;
  }
  nanosleep((const struct timespec[]){{0, iBurstTime * 1000000L}}, NULL);
  pProcess->iRemainingBurstTime-=iBurstTime;
  if(pProcess->iRemainingBurstTime == 0) {
    pProcess->iState = TERMINATED;
    gettimeofday(&pProcess->oLastTimeRunning, NULL);
    return;
  }
  pProcess->iState = READY;
  return;
}

void unblockProcess(Process * pProcess) 
{
    pProcess->iState = READY;
}

void runPreemptiveProcess(Process * pProcess, bool bSimulateIO) {
  int iBurstTime = pProcess->iRemainingBurstTime > TIME_SLICE ? TIME_SLICE : pProcess->iRemainingBurstTime;
  runProcess(pProcess, iBurstTime, bSimulateIO);
}

void runNonPreemptiveProcess(Process * pProcess, bool bSimulateIO) {
  int iBurstTime = pProcess->iRemainingBurstTime;
  runProcess(pProcess, iBurstTime, bSimulateIO);
}
/*
 * Function returning the time difference in milliseconds between the two time stamps, with start being the earlier time, and end being the later time.
 */
long int getDifferenceInMilliSeconds(struct timeval start, struct timeval end)
{
  long int iSeconds = end.tv_sec - start.tv_sec;
  long int iUSeconds = end.tv_usec - start.tv_usec;
  long int mtime = (iSeconds * 1000 + iUSeconds / 1000.0);
  return mtime;
}
