#include "linked_list.h"

void init();

// --------ALgorithms------
void highestPriorityFirst();
void shortestRemainingTimeNext();
void roundRobin(int quantum);
// ------------------------

/*shared memory handlers*/
int initShm(int id);
int getShmValue(int shmid);

void readProcessesData();
void storeProcessData();
int forkNewProcess(int id, int remainingTime);

void resumeProcess(Node *processNode);
void stopProcess(Node *processNode);
void removeProcess(Node *processNode);

int shmid, msqdownid, msqupid;
process *shm_proc_addr;
Node *head;

int remainingProcesses = 0;
bool processIsComming = true;

Node *runningProcessNode;

int main(int argc, char *argv[])
{
    // initialize all
    init();

    printf("scheduler starting...\n");

    head = createLinkedList();

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
        roundRobin(quantum);
        break;
    }

    //TODO implement the scheduler :)
    //upon termination release the clock resources.

    //TODO Start a new process. (Fork it and give it its parameters.)
    //Switch between two processes according to the scheduling algorithm. (Stop the old process and save its state and start/resume another one.)
    //Keep a process control block (PCB) for each process in the system.
    //Delete the data of a process when it gets noties that it nished.
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

int forkNewProcess(int id, int remainingTime)
{
    int processPID = fork();
    if (processPID == 0)
    {
        // convert args to char*
        char execStr1[MAX_DIGITS + 1];
        char execStr2[MAX_DIGITS + 1];
        snprintf(execStr1, MAX_DIGITS + 1, "%d", id);
        snprintf(execStr2, MAX_DIGITS + 1, "%d", remainingTime);

        execl("./process.out", "process.out", execStr1, execStr2, (char *)0);
        printf("Error in executing process.out\n");
        destroyClk(true);
        exit(1);
    }
    else if (processPID == -1)
    {
        perror("error in forking process");
        destroyClk(true);
        exit(1);
    }

    return processPID;
}

void storeProcessData()
{
    process p = *shm_proc_addr;
    printf("ID=%d\n", p.id);
    Node *newNode = (Node *)malloc(sizeof(Node));
    // initialize newNode
    newNode->process = p;
    newNode->PCB.executionTime = 0;
    newNode->PCB.processState = WAITING;
    newNode->PCB.remainingTime = p.runningtime;
    newNode->PCB.waitingTime = 0;
    newNode->PCB.PID = -1; // -1 means the process not created
    newNode->PCB.shmid = initShm(p.id);
    // insert new process to the linked list
    insert(head, newNode);
    remainingProcesses++;
}

void readProcessesData()
{
    struct msgbuf message;
    while (processIsComming)
    {
        int recValue = msgrcv(msqdownid, &message, sizeof(message.mtext), 0, IPC_NOWAIT);
        if (recValue == -1)
            break;

        if (message.mtext == FINISHED)
        {
            processIsComming = false;
            break;
        }

        storeProcessData();

        int sendValue = msgsnd(msqupid, &message, sizeof(message.mtext), !IPC_NOWAIT);
        if (sendValue == -1)
            perror("Error in send");

        if (message.mtext == COMPLETE)
            break;
    }
}

void resumeProcess(Node *processNode)
{
    if (processNode == NULL)
        processNode = head;

    // set process state to running
    processNode->PCB.processState = RUNNING;

    // if this is the first time i run the process
    if (processNode->PCB.PID == -1)
    {
        processNode->PCB.PID = forkNewProcess(
            processNode->process.id,
            processNode->process.runningtime);
    }
    else
    {
        // send signal to resume
        kill(processNode->PCB.PID, SIGCONT);
    }
}

void stopProcess(Node *processNode)
{
    processNode->PCB.processState = WAITING;
    kill(processNode->PCB.PID, SIGTSTP);
}
void removeProcess(Node *processNode)
{
    processNode = removeNodeWithID(head, processNode->process.id);
    if (processNode != NULL)
    {
        // TODO : output the results
        free(processNode);
        remainingProcesses--;
    }
}

void highestPriorityFirst() {}
void shortestRemainingTimeNext() {}
void roundRobin(int quantum)
{
    runningProcessNode = NULL;
    int currentQuantum = 0;
    while (remainingProcesses || processIsComming)
    {
        int now = getClk();

        readProcessesData();

        if (remainingProcesses)
        {
            resumeProcess(runningProcessNode);
            currentQuantum = quantum;
        }

        sleep(1);
        while (now == getClk())
            ;

        if (remainingProcesses)
        {
            currentQuantum -= (getClk() - now);

            if (runningProcessNode && runningProcessNode->PCB.processState == TERMINATED)
            {
                removeProcess(runningProcessNode);
                runningProcessNode = runningProcessNode->next;
            }
            else if (currentQuantum <= 0)
            {
                stopProcess(runningProcessNode);
                runningProcessNode = runningProcessNode->next;
            }
        }
    }
}

/*initlizer remainging time as a shared memory varible*/
int initShm(int id)
{
    /*creating shared memory variable*/
    int shmKey = ftok("keyFile", id);
    int shmid = shmget(shmKey, 4, IPC_CREAT | 0666);
    if ((long)shmid == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }

    return shmid;
}

/*get shared memory current value*/
int getShmValue(int shmid)
{
    //attach shared memory
    int *shmaddr = (int *)shmat(shmid, (void *)0, 0);

    if ((long)shmaddr == -1)
    {
        printf("Error in attaching the shm in process with id: %d\n", id);
        perror("");
        exit(-1);
    }
    // deatach shared memory
    int value = *shmaddr;
    shmdt(shmaddr);
    return value;
}
