#include <stdio.h>
#include <stdlib.h>
#include "sch-helpers.h"

process processes[MAX_PROCESSES + 1]; 
process *processors[NUMBER_OF_PROCESSORS]; 

int numberOfProcesses; 
int nextProcess; 

process_queue readyQueue; 
process_queue ioQueue; 

int totalTurnaroundTime = 0; 
int totalWaitingTime = 0; 
int totalContextSwitches = 0; 
int currentTime = 0; 
int totalCPUTime = 0; 
int lastProcessPID = 0; 

int timeQuantum; 

void sortReadyQueueByPID() {
        
    if (readyQueue.size <= 1) return; 

    process_node *sorted = NULL;
    process_node *current = readyQueue.front;

    while (current != NULL) {        
        process_node *next = current->next;
        if (sorted == NULL || current->data->pid < sorted->data->pid) {
            current->next = sorted;
            sorted = current;
        } else {
            process_node *search = sorted;
            while (search->next != NULL && search->next->data->pid < current->data->pid) {
                search = search->next;
            }
            current->next = search->next;
            search->next = current;
        }
        current = next;
    }
    readyQueue.front = sorted;
    process_node *last = sorted;

    while (last->next != NULL) {
        last = last->next;
    }    
    readyQueue.back = last;
}

int activeCPUCount() {

    int activeCPUs = 0;
    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {
        if (processors[i]) {
            activeCPUs++;
        } 
    }
    return activeCPUs;
}

process *getNextProcess() {

    if (readyQueue.size == 0) {
        return NULL;
    }
    
    process *process = readyQueue.front->data;
    dequeueProcess(&readyQueue);

    return process;
}

void checkProcessArrivals() {

    while (nextProcess < numberOfProcesses && processes[nextProcess].arrivalTime <= currentTime) {        
        if (nextProcess >= MAX_PROCESSES) {
            fprintf(stderr, "Error: Exceeded maximum number of processes.\n");
            exit(-1);
        }
        enqueueProcess(&readyQueue, &processes[nextProcess++]);
    }
}

void handleIOBursts() {

    int ioQueueSize = ioQueue.size;

    for (int i = 0; i < ioQueueSize; i++) {
        process *current = ioQueue.front->data;
        dequeueProcess(&ioQueue);
        if (current->bursts[current->currentBurst].step == current->bursts[current->currentBurst].length) {
            current->currentBurst++;
            enqueueProcess(&readyQueue, current);
        } else {
            enqueueProcess(&ioQueue, current);
        }
    }
}

void updateReadyQueue() {

    sortReadyQueueByPID();

    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {
        if (!processors[i]) {
            processors[i] = getNextProcess();
            if (processors[i]) {
                processors[i]->quantumRemaining = timeQuantum;
            }
        }
    }
}

void checkBurstCompletion() {

    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {
        if (processors[i]) {
            processors[i]->quantumRemaining--;
            if (processors[i]->bursts[processors[i]->currentBurst].step == 
                processors[i]->bursts[processors[i]->currentBurst].length) {
                processors[i]->currentBurst++;
                if (processors[i]->currentBurst < processors[i]->numberOfBursts) {
                    enqueueProcess(&ioQueue, processors[i]);
                } else {
                    processors[i]->endTime = currentTime;
                }
                processors[i] = NULL;
            } else if (processors[i]->quantumRemaining == 0) {
                enqueueProcess(&readyQueue, processors[i]);
                processors[i] = NULL;
                totalContextSwitches++;
            }
        }
    }
}

void incrementIOBursts() {

    int ioQueueSize = ioQueue.size;

    for (int i = 0; i < ioQueueSize; i++) {        
        process *current = ioQueue.front->data;
        dequeueProcess(&ioQueue);
        current->bursts[current->currentBurst].step++;
        enqueueProcess(&ioQueue, current);
    }
}

void updateWaitingTimes() {

    for (int i = 0; i < readyQueue.size; i++) {
        process *current = readyQueue.front->data;
        dequeueProcess(&readyQueue);
        current->waitingTime++;
        enqueueProcess(&readyQueue, current);
    }
}

void incrementCPUBursts() {

    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {    
        if (processors[i]) {
            processors[i]->bursts[processors[i]->currentBurst].step++;
        }
    }
}

void readInput() {
    
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

    if (processes[numberOfProcesses].numberOfBursts > MAX_BURSTS) {
        error_too_many_bursts(processes[numberOfProcesses].pid);
        exit(-1);
    }
    else if (processes[numberOfProcesses].numberOfBursts < 0) {
        fprintf(stderr, "Error: Process %d has invalid number of bursts.\n", processes[numberOfProcesses].pid);
        exit(-1);
    }

    for (int i = 0; i < processes[numberOfProcesses].numberOfBursts; i++) {
        if (processes[numberOfProcesses].bursts[i].length < 0) {
            fprintf(stderr, "Error: Process %d has negative burst length.\n", processes[numberOfProcesses].pid);
            exit(-1);
        }
    }

    qsort(processes, numberOfProcesses, sizeof(process), compareByArrival);

    initializeProcessQueue(&readyQueue);
    initializeProcessQueue(&ioQueue);
}

void roundRobinScheduler() {
    
    if (nextProcess >= MAX_PROCESSES) {
        fprintf(stderr, "Error: Exceeded maximum number of processes.\n");
        exit(-1);
    }
    
    while (1) {
        checkProcessArrivals();
        checkBurstCompletion();
        handleIOBursts();
        updateReadyQueue();

        incrementIOBursts();
        updateWaitingTimes();
        incrementCPUBursts();

        totalCPUTime += activeCPUCount();

        if (activeCPUCount() == 0 && ioQueue.size == 0) break;

        currentTime++;
    }
}

void printResults() {

    for (int i = 0; i < numberOfProcesses; i++) {
        totalTurnaroundTime += processes[i].endTime - processes[i].arrivalTime;
        totalWaitingTime += processes[i].waitingTime;
        if (processes[i].endTime == currentTime) {
            lastProcessPID += processes[i].pid;
        }
    }

    if (numberOfProcesses == 0) {
        fprintf(stderr, "Error: No processes to calculate averages.\n");
        return;
    }
    
    if (currentTime == 0) {
        fprintf(stderr, "Error: Simulation time is zero.\n");
        return;
    }
    
    float averageWaitingTime = (float)totalWaitingTime / numberOfProcesses;
    float averageTurnaroundTime = (float)totalTurnaroundTime / numberOfProcesses;
    float averageCPUUtilization = (float)totalCPUTime / currentTime * 100;

    printf("Average waiting time\t\t\t: %.2f units\n", averageWaitingTime);
    printf("Average turnaround time\t\t\t: %.2f units\n", averageTurnaroundTime);
    printf("Time all processes finished\t\t: %d\n", currentTime);
    printf("Average CPU utilization\t\t\t: %.1f%%\n", averageCPUUtilization);
    printf("Number of context switches\t\t: %d\n", totalContextSwitches);
    printf("PID(s) of last process(es) to finish\t: %d\n", lastProcessPID);
}

int main(int argc, char *argv[]) {
    
    if (argc != 2) {
        fprintf(stderr, "Error: Please specify a time quantum. \n");
        return 1;
    }

    timeQuantum = atoi(argv[1]);

    if (timeQuantum <= 0) {
        error_bad_quantum();
        return 1;
    }

    readInput();
    roundRobinScheduler();

    printResults();

    return 0;
}