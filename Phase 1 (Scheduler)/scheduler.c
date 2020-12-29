#include "headers.h"
#include "linked_list.h"

// --------ALgorithms------
void highestPriorityFirst();
void shortestRemainingTimeNext();
void roundRobin();
// ------------------------

void readProcessesData();

int shmid, msqdownid, msqupid;
process *shm_proc_addr;

void init();

int main(int argc, char *argv[])
{
    // initialize all
    init();

    printf("scheduler starting...\n");

    readProcessesData();

    int schAlgo = atoi(argv[1]), quantum = atoi(argv[2]);

    switch (schAlgo)
    {
    case HPF:
        highestPriorityFirst();
        break;
    case SRTN:
        shortestRemainingTimeNext();
        break;
    case RR:
        roundRobin();
        break;
    }

    //TODO implement the scheduler :)
    //upon termination release the clock resources.

    //TODO Start a new process. (Fork it and give it its parameters.)
    //Switch between two processes according to the scheduling algorithm. (Stop the old process and save its state and start/resume another one.)
    //Keep a process control block (PCB) for each process in the system.
    //Delete the data of a process when it gets noties that it nished.
    sleep(5);
    printf("scheduler terminates...\n");
    destroyClk(true);
}

void init()
{
    /* Creating Shared Memory Segment */
    shmid = shmget(PROC_SH_KEY, sizeof(process), IPC_CREAT | 0644);
    if ((long)shmid == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }
    shm_proc_addr = (process *)shmat(shmid, (void *)0, 0);
    if ((long)shm_proc_addr == -1)
    {
        perror("Error in attaching the shm in process generator!");
        exit(-1);
    }

    msqdownid = msgget(PROC_MSQ_DOWN_KEY, IPC_CREAT | 0644);
    if (msqdownid == -1)
    {
        perror("Error in create msqdownid");
        exit(-1);
    }

    msqupid = msgget(PROC_MSQ_UP_KEY, IPC_CREAT | 0644);
    if (msqupid == -1)
    {
        perror("Error in create msqupid");
        exit(-1);
    }

    // initialize clock
    initClk();
}

void readProcessesData()
{
    struct msgbuf message;
    while (1)
    {
        int recValue = msgrcv(msqupid, &message, sizeof(message.mtext), 0, IPC_NOWAIT);
        if (recValue == -1)
            break;

        // store the data
        printf("boooom\n");

        message.mtext = COMPLETE;
        int sendValue = msgsnd(msqdownid, &message, sizeof(message.mtext), !IPC_NOWAIT);
        if (sendValue == -1)
            perror("Error in send");

        if (recValue == COMPLETE)
            break;
    }
}

void highestPriorityFirst() {}
void shortestRemainingTimeNext() {}
void roundRobin() {}