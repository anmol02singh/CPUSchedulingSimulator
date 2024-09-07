#include <stdio.h>
#include <stdlib.h>
#include "sch-helpers.h"

process processes[MAX_PROCESSES + 1]; 
process *processors[NUMBER_OF_PROCESSORS];

int numberOfProcesses; 
int nextProcess; 

int totalTurnaroundTime = 0; 
int totalWaitingTime = 0; 
int totalContextSwitches = 0; 
int currentTime = 0; 
int totalCPUTime = 0; 
int lastProcessPID = 0; 

int timeQuantumQ0; 
int timeQuantumQ1; 

process_queue ioQueue; 
process_queue Q0; 
process_queue Q1; 
process_queue Q2; 

void sortQueueByPID(process_queue *queue) {
    
    if (queue->size <= 1) return; 
    process_node *sorted = NULL;
    process_node *current = queue->front;

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
    queue->front = sorted;
    process_node *last = sorted;

    while (last->next != NULL) {
        last = last->next;
    }
    queue->back = last;
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

    process *process = NULL;   

    if (Q0.size > 0) {
        process = Q0.front->data;
        dequeueProcess(&Q0);
    } else if (Q1.size > 0) {
        process = Q1.front->data;
        dequeueProcess(&Q1);
    } else if (Q2.size > 0) {
        process = Q2.front->data;
        dequeueProcess(&Q2);
    }
    return process;
}

void checkProcessArrivals() {

    while (nextProcess < numberOfProcesses && processes[nextProcess].arrivalTime <= currentTime) {    
        if (nextProcess >= MAX_PROCESSES) {
            fprintf(stderr, "Error: Exceeded maximum number of processes.\n");
            exit(-1);
        }
        processes[nextProcess].currentQueue = 0;
        enqueueProcess(&Q0, &processes[nextProcess++]);
    }
}

void handleIOBursts() {

    int ioQueueSize = ioQueue.size;

    for (int i = 0; i < ioQueueSize; i++) {
        process *current = ioQueue.front->data;
        dequeueProcess(&ioQueue);
        if (current->bursts[current->currentBurst].step == current->bursts[current->currentBurst].length) {
            current->currentBurst++;
            current->currentQueue = 0;
            enqueueProcess(&Q0, current);
        } else {
            enqueueProcess(&ioQueue, current);
        }
    }
}

void updateReadyQueues() {

    sortQueueByPID(&Q0);
    sortQueueByPID(&Q1);
    sortQueueByPID(&Q2);

    for (int i = 0; i < NUMBER_OF_PROCESSORS; i++) {
        if (!processors[i]) {
            processors[i] = getNextProcess();
            if (processors[i]) {
                processors[i]->quantumRemaining = (processors[i]->currentQueue == 0) ? timeQuantumQ0 : timeQuantumQ1;
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
                if (processors[i]->currentQueue == 0) {
                    processors[i]->currentQueue = 1;
                    enqueueProcess(&Q1, processors[i]);
                } else if (processors[i]->currentQueue == 1) {
                    processors[i]->currentQueue = 2;
                    enqueueProcess(&Q2, processors[i]);
                } else {
                    enqueueProcess(&Q2, processors[i]);
                }
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

    for (int i = 0; i < Q0.size; i++) {
        process *current = Q0.front->data;
        dequeueProcess(&Q0);
        current->waitingTime++;
        enqueueProcess(&Q0, current);
    }

    for (int i = 0; i < Q1.size; i++) {
        process *current = Q1.front->data;
        dequeueProcess(&Q1);
        current->waitingTime++;
        enqueueProcess(&Q1, current);
    }
    
    for (int i = 0; i < Q2.size; i++) {
        process *current = Q2.front->data;
        dequeueProcess(&Q2);
        current->waitingTime++;
        enqueueProcess(&Q2, current);
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

    initializeProcessQueue(&Q0);
    initializeProcessQueue(&Q1);
    initializeProcessQueue(&Q2);
    initializeProcessQueue(&ioQueue);
}

void fbqScheduler() {

    if (nextProcess >= MAX_PROCESSES) {
        fprintf(stderr, "Error: Exceeded maximum number of processes.\n");
        exit(-1);
    }

    while (1) {
        checkProcessArrivals();
        checkBurstCompletion();
        handleIOBursts();
        updateReadyQueues();

        incrementIOBursts();
        updateWaitingTimes();
        incrementCPUBursts();
        
        totalCPUTime += activeCPUCount();
        
        if (activeCPUCount() == 0 && ioQueue.size == 0 && Q0.size == 0 && Q1.size == 0 && Q2.size == 0) break;
        
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

    if (argc != 3) {
        fprintf(stderr, "Error: Please specify two time quantums. \n");
        return -1;
    }
    
    timeQuantumQ0 = atoi(argv[1]);
    timeQuantumQ1 = atoi(argv[2]);

    if (timeQuantumQ0 <= 0 || timeQuantumQ1 <=0) {
        error_bad_quantum();
        return -1;
    }
    
    readInput();
    fbqScheduler();

    printResults();

    return 0;
}