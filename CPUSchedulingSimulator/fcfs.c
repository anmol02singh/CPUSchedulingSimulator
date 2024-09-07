#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "sch-helpers.h"

process processes[MAX_PROCESSES + 1]; 
int numberOfProcesses = 0; 
int totalWaitingTime = 0; 
int currentTime = 0; 
int totalTurnaroundTime = 0; 
int totalContextSwitches = 0; 
int totalCPUTime = 0; 
int lastProcessTime = 0; 
int lastProcessPID = -1; 

process_queue readyQueue; 
process_queue ioQueue; 

process *processors[NUMBER_OF_PROCESSORS]; 

int completedProcesses = 0; 
int CPUBusy[NUMBER_OF_PROCESSORS] = {0}; 

process *runningProcesses[NUMBER_OF_PROCESSORS] = {NULL}; 

void checkProcessArrivals() {
    
    for (int i = 0; i < numberOfProcesses; i++) {
        if (processes[i].arrivalTime == currentTime) {
            enqueueProcess(&readyQueue, &processes[i]);
        }
    }
}

void checkBurstCompletion() {

    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {
        if (CPUBusy[i] && runningProcesses[i]->bursts[runningProcesses[i]
        ->currentBurst].step == runningProcesses[i]->bursts[runningProcesses[i]
        ->currentBurst].length) {
            runningProcesses[i]->currentBurst++;
            if (runningProcesses[i] -> 
            currentBurst == runningProcesses[i]-> numberOfBursts) {
                runningProcesses[i] -> endTime = currentTime;
                completedProcesses++;
            } else if(runningProcesses[i]->currentBurst % 2 == 1) {
                enqueueProcess(&ioQueue, runningProcesses[i]);
            } else {
                enqueueProcess(&readyQueue, runningProcesses[i]);
            }
            CPUBusy[i] = 0;
            runningProcesses[i] = NULL;
        }
    }
}

void handleIOBursts() {

    if (ioQueue.size > 0) {
        process *ioProcess = ioQueue.front -> data;
        ioProcess-> bursts[ioProcess->currentBurst].step++;
        if (ioProcess->bursts[ioProcess->currentBurst].step == ioProcess->bursts[ioProcess->currentBurst].length) {
            dequeueProcess(&ioQueue);
            ioProcess->currentBurst++;
            enqueueProcess(&readyQueue, ioProcess);
        }
    }
}

void assignProcessesToCPUs() {

    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {
        if (!CPUBusy[i] && readyQueue.size > 0) {
            runningProcesses[i] = readyQueue.front->data;
            dequeueProcess(&readyQueue);
            CPUBusy[i] = 1;
            if (runningProcesses[i]->currentBurst == 0) {
                runningProcesses[i] -> startTime = currentTime;
            }
        }
    }
}

void incrementBurstSteps() {

    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {
        if (CPUBusy[i]) {
            runningProcesses[i]->bursts[runningProcesses[i]->currentBurst].step++;
        }
    }
}

void calculateWaitingTime() {

    for (int i = 0; i < numberOfProcesses; i++) {
        int waitingTime = processes[i].endTime - processes[i].arrivalTime;
        for (int j = 0; j < processes[i].numberOfBursts; j+=2) {
            waitingTime -= processes[i].bursts[j].length;
        }
        processes[i].waitingTime = waitingTime;
        totalWaitingTime += waitingTime;
    }
}

void calculateTurnaroundTime() {

    for (int i = 0; i < numberOfProcesses; i++) {
        totalTurnaroundTime += processes[i].endTime - processes[i].arrivalTime;
        if (processes[i].endTime > lastProcessTime) {
            lastProcessTime = processes[i].endTime;
            lastProcessPID = processes[i].pid;
        }
    }
}

void calculateCPUTime() {

    for (int i = 0; i < numberOfProcesses; i++) {
        for (int j = 0; j < processes[i].numberOfBursts; j += 2) {
            totalCPUTime += processes[i].bursts[j].length;
        }
    }
}

void printResults(){

    float averageWaitingTime = (float)totalWaitingTime / numberOfProcesses;
    float averageTurnaroundTime = (float)totalTurnaroundTime / numberOfProcesses;
    float averageCPUUtilization = (float)totalCPUTime / (lastProcessTime * NUMBER_OF_PROCESSORS) * 100;

    printf("Average waiting time\t\t\t: %.2f units\n", averageWaitingTime);
    printf("Average turnaround time\t\t\t: %.2f units\n", averageTurnaroundTime);
    printf("Time all processes finished\t\t: %d\n", lastProcessTime);
    printf("Average CPU utilization\t\t\t: %.1f%%\n", averageCPUUtilization);
    printf("Number of context switches\t\t: %d\n", totalContextSwitches);
    printf("PID(s) of last process(es) to finish\t: %d\n", lastProcessPID);
}

int main(){

    int status = 0;

    while ((status=readProcess(&processes[numberOfProcesses]))) {

        if (status == 1) {
            for (int i = 0; i < numberOfProcesses; i++) {
                if (processes[i].pid == processes[numberOfProcesses].pid) {
                    error_duplicate_pid(processes[numberOfProcesses].pid);
                }
            }
            numberOfProcesses++;
        } else if (status == 0) {
            break;
        }
    }

    qsort(processes, numberOfProcesses, sizeof(process), compareByArrival);

    initializeProcessQueue(&readyQueue);
    initializeProcessQueue(&ioQueue);

    while (completedProcesses < numberOfProcesses) {
        checkProcessArrivals();
        checkBurstCompletion();
        handleIOBursts();
        assignProcessesToCPUs();
        incrementBurstSteps();
        currentTime++;
    }
    
    calculateWaitingTime();
    calculateTurnaroundTime();
    calculateCPUTime();
    printResults();
    
    return 0;
}