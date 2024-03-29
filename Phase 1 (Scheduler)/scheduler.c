#include "linked_list.h"
#include "vector.h"
#include <math.h>

void init();

// --------ALgorithms------
void highestPriorityFirst();
void shortestRemainingTimeNext();
void roundRobin(int quantum);
// ------------------------

int initSem(int id);

/*shared memory handlers*/
int initShm(int id);
int getShmValue(int shmid);

Node *readProcessesData();
Node *storeProcessData();
int forkNewProcess(int id, int remainingTime);

void resumeProcess(Node *processNode);
void stopProcess(Node *processNode);
void removeProcess(Node *processNode);
void updateProcessTime(Node *processNode);

void sortNewProcessesWithPriority(Node *processNode);
void sortNewProcessesWithRemainingTime(Node *processNode);

void outputPref();

int shmid, msqid;
int sem_id_scheduler, sem_id_generator;
process *shm_proc_addr;
Node *head;

//output files
FILE *logFile, *prefFIle;

int remainingProcesses = 0;
bool processIsComming = true;

Node *runningProcessNode;

int now; // current clock
//varabiles for pref file
vec *WTAs; //store all WTA of all processes
int TotalRunningTimes, TotalWaitings;
float TotalWTA;
int main(int argc, char *argv[])
{
    // initialize all
    init();

    printf("scheduler starting...\n");

    head = createLinkedList();

    //readProcessesData();

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
    fclose(logFile);
    outputPref();
    printf("scheduler terminates...\n");
    destroyClk(true);
}

void init()
{
    //initialize varabiles for pref file
    WTAs = initVec();
    TotalRunningTimes = 0;
    TotalWaitings = 0;
    TotalWTA = 0;
    //open log file to write
    logFile = fopen("scheduler.log", "w");
    fprintf(logFile, "#At Time\tx\tprocess\tY\tstate arr\tw\ttotal\tz\tremain\ty\twait\tk\n");
    /* Creating Semaphores */
    key_t key_id_sem_scheduler = ftok("keyFile", 's');
    key_t key_id_sem_generator = ftok("keyFile", 'g');

    sem_id_scheduler = semget(key_id_sem_scheduler, 1, 0660 | IPC_CREAT);
    sem_id_generator = semget(key_id_sem_generator, 1, 0660 | IPC_CREAT);
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
    msqid = msgget(PROC_MSQ_KEY, IPC_CREAT | 0644);
    if (msqid == -1)
    {
        perror("Error in create msqid");
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

Node *storeProcessData()
{
    process p = *shm_proc_addr;

    if (p.runningtime <= 0)
        return NULL;

    printf("process ID=%d Arrived Time = %d\n", p.id, getClk());
    Node *newNode = (Node *)malloc(sizeof(Node));
    // initialize newNode
    newNode->process = p;
    newNode->PCB.executionTime = 0;
    newNode->PCB.processState = WAITING;
    newNode->PCB.remainingTime = p.runningtime;
    newNode->PCB.waitingTime = 0;
    newNode->PCB.PID = -1; // -1 means the process not created
    newNode->PCB.shmid = -1;
    newNode->PCB.semid = -1;
    // insert new process to the linked list
    insert(&head, &newNode);

    remainingProcesses++;

    return newNode;
}

/* read new processes data and return poiter to the first one (if no new process returns NULL) */
Node *readProcessesData()
{
    Node *newNode = NULL;
    struct msgbuf message;
    while (processIsComming)
    {
        down(sem_id_scheduler);
        int recValue = msgrcv(msqid, &message, sizeof(message.mtext), 0, !IPC_NOWAIT);
        if (recValue == -1)
            perror("Error in recieve");

        if (message.mtext == FINISHED)
        {
            processIsComming = false;
            break;
        }

        if (message.mtext == NO_PROCESSES)
            break;

        Node *tmpNode = storeProcessData();

        if (newNode == NULL)
            newNode = tmpNode;

        if (message.mtext == COMPLETE)
            break;

        up(sem_id_generator);
    }
    return newNode;
}

void resumeProcess(Node *processNode)
{
    if (!processNode || processNode->PCB.processState == TERMINATED)
        return;

    if (processNode->PCB.processState == RUNNING)
    {
        up(processNode->PCB.semid);
        down(sem_id_scheduler);
        return;
    }

    // set process state to running
    processNode->PCB.processState = RUNNING;
    int arrivalTime = processNode->process.arrivaltime;
    int executionTime = processNode->PCB.executionTime;

    processNode->PCB.waitingTime = getClk() - arrivalTime - executionTime;
    // if this is the first time i run the process
    if (processNode->PCB.PID == -1)
    {
        //print to log file
        processNode->PCB.waitingTime = getClk() - processNode->process.arrivaltime - processNode->PCB.executionTime;
        fprintf(logFile, "At Time\t%d\tprocess\t%d\tstarted arr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), processNode->process.id,
                processNode->process.arrivaltime, processNode->process.runningtime,
                processNode->PCB.remainingTime, processNode->PCB.waitingTime);

        processNode->PCB.PID = forkNewProcess(
            processNode->process.id,
            processNode->process.runningtime);
        processNode->PCB.shmid = initShm(processNode->process.id);
        processNode->PCB.semid = initSem(processNode->process.id);
    }
    else
    {
        processNode->PCB.waitingTime = getClk() - processNode->process.arrivaltime - processNode->PCB.executionTime;
        fprintf(logFile, "At Time\t%d\tprocess\t%d\tresumed arr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), processNode->process.id,
                processNode->process.arrivaltime, processNode->process.runningtime,
                processNode->PCB.remainingTime, processNode->PCB.waitingTime);
    }
    up(processNode->PCB.semid);
    printf("running process id = %d Time = %d\n", runningProcessNode->process.id, getClk());
    down(sem_id_scheduler);
}

void stopProcess(Node *processNode)
{
    if (!processNode || processNode->PCB.processState == TERMINATED)
        return;
    processNode->PCB.waitingTime = getClk() - processNode->process.arrivaltime - processNode->PCB.executionTime;
    fprintf(logFile, "At Time\t%d\tprocess\t%d\tstopped arr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), processNode->process.id,
            processNode->process.arrivaltime, processNode->process.runningtime,
            processNode->PCB.remainingTime, processNode->PCB.waitingTime);
    processNode->PCB.processState = WAITING;
}
void removeProcess(Node *processNode)
{
    Node *deletedProcess = removeNodeWithID(&head, processNode->process.id);
    if (deletedProcess)
    {
        int TA = getClk() - processNode->process.arrivaltime;
        float WTA = TA / (1.0 * processNode->process.runningtime);
        vec_push_back(WTAs, WTA);
        TotalWTA += WTA;
        TotalRunningTimes += processNode->process.runningtime;
        processNode->PCB.waitingTime = getClk() - processNode->process.arrivaltime - processNode->PCB.executionTime;
        TotalWaitings += processNode->PCB.waitingTime;
        printf("process ID=%d terminated  Time = %d\n", processNode->process.id, getClk());
        fprintf(logFile, "At Time\t%d\tprocess\t%d\tfinished arr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\tTA\t%d\tWTA\t%.2f\n", getClk(), processNode->process.id,
                processNode->process.arrivaltime, processNode->process.runningtime,
                processNode->PCB.remainingTime, processNode->PCB.waitingTime, TA, WTA);
        free(deletedProcess);

        remainingProcesses--;
    }
}

/* Update process remaining/execution/waiting time */
void updateProcessTime(Node *processNode)
{
    if (processNode->PCB.processState == TERMINATED)
        return;

    int remainingTime = getShmValue(processNode->PCB.shmid);
    int dif = processNode->PCB.remainingTime - remainingTime;

    processNode->PCB.remainingTime = remainingTime;
    processNode->PCB.executionTime += dif;

    int arrivalTime = processNode->process.arrivaltime;
    int executionTime = processNode->PCB.executionTime;

    processNode->PCB.waitingTime = getClk() - arrivalTime - executionTime;

    if (remainingTime <= 0)
        processNode->PCB.processState = TERMINATED;
}

void highestPriorityFirst()
{
    runningProcessNode = NULL;
    bool processIsRunning = 0;
    while (remainingProcesses || processIsComming)
    {
        now = getClk();
        Node *newNode = readProcessesData();

        sortNewProcessesWithPriority(newNode);

        if (remainingProcesses && processIsRunning == 0)
        {
            if (runningProcessNode == NULL)
                runningProcessNode = head;

            processIsRunning = 1;
        }

        if (remainingProcesses)
            resumeProcess(runningProcessNode);

        //sleep(1);
        while (now == getClk())
            ;

        if (remainingProcesses)
        {
            updateProcessTime(runningProcessNode);
            /*if the process terminated give the turn to the next node*/
            if (runningProcessNode->PCB.processState == TERMINATED)
            {
                removeProcess(runningProcessNode);
                runningProcessNode = NULL;
                processIsRunning = 0;
            }
        }
        up(sem_id_generator);
    }
}
void shortestRemainingTimeNext()
{
    Node *lastRunningProcessNode = NULL;
    runningProcessNode = NULL;
    while (remainingProcesses || processIsComming)
    {
        now = getClk();
        Node *newNode = readProcessesData();
        sortNewProcessesWithRemainingTime(newNode);

        if (remainingProcesses && newNode)
        {
            lastRunningProcessNode = runningProcessNode;
            if (runningProcessNode == NULL)
                runningProcessNode = head;
            else if (head && head->PCB.remainingTime < runningProcessNode->PCB.remainingTime)
            {
                stopProcess(lastRunningProcessNode);
                runningProcessNode = head;
            }
        }

        if (remainingProcesses)
            resumeProcess(runningProcessNode);

        //sleep(1);
        while (now == getClk())
            ;

        if (remainingProcesses)
        {
            updateProcessTime(runningProcessNode);

            if (runningProcessNode && runningProcessNode->PCB.processState == TERMINATED)
            {
                removeProcess(runningProcessNode);
                runningProcessNode = head;
            }
        }
        up(sem_id_generator);
    }
}
void roundRobin(int quantum)
{
    runningProcessNode = NULL;
    Node *lastRunningProcessNode = NULL;
    int currentQuantum = 0;
    while (remainingProcesses || processIsComming)
    {
        now = getClk();
        readProcessesData();

        if (remainingProcesses && currentQuantum <= 0)
        {
            lastRunningProcessNode = runningProcessNode;
            // if runningProcess is at the end of the linked list run the first process
            if (!runningProcessNode || !runningProcessNode->next)
                runningProcessNode = head;
            else
                runningProcessNode = runningProcessNode->next;

            if (runningProcessNode->PCB.processState == WAITING)
            {
                stopProcess(lastRunningProcessNode);
            }

            currentQuantum = quantum;
        }

        if (remainingProcesses)
            resumeProcess(runningProcessNode);

        //sleep(1);
        while (now == getClk())
            ;

        if (remainingProcesses)
        {
            currentQuantum -= (getClk() - now);

            updateProcessTime(runningProcessNode);

            if (runningProcessNode && runningProcessNode->PCB.processState == TERMINATED)
            {
                removeProcess(runningProcessNode);
                currentQuantum = 0;
            }
        }
        up(sem_id_generator);
    }
}

int initSem(int id)
{
    int semKey = ftok("keyFile", id);
    int semid = semget(semKey, 1, 0660 | IPC_CREAT);
    union Semun semun;
    semun.val = 0;
    if (semctl(semid, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl");
        exit(-1);
    }
    return semid;
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
        // the shared memory removed then the process is deleted
        return 0;
    }
    // deatach shared memory
    int value = *shmaddr;
    shmdt(shmaddr);
    return value;
}

/*insert new nodes in a sorted way according to there pirorities*/
void sortNewProcessesWithPriority(Node *processNode)
{
    if (processNode == NULL)
        return;
    //disjointing the new nodes form the linked list
    if (head == processNode)
    {
        head = NULL;
    }
    else
    {
        processNode->prev->next = NULL;
        processNode->prev = NULL;
    }
    //joining them in a sorted way
    while (processNode)
    {
        //printf("id = %d, p = %d\n", processNode->process.id, processNode->process.priority);
        Node *nextNode = processNode->next;
        processNode->next = NULL;
        insertionSortWithPriority(&head, &processNode);
        processNode = nextNode;
    }
}

/*insert new nodes in a sorted way according to there remainging time*/
void sortNewProcessesWithRemainingTime(Node *processNode)
{
    if (processNode == NULL)
        return;
    //disjointing the new nodes form the linked list
    if (head == processNode)
    {
        head = NULL;
    }
    else
    {
        processNode->prev->next = NULL;
        processNode->prev = NULL;
    }
    //joining them in a sorted way
    while (processNode)
    {
        Node *nextNode = processNode->next;
        processNode->next = NULL;
        insertionSortWithRemainingTime(&head, &processNode);
        processNode = nextNode;
    }
}
void outputPref()
{
    prefFIle = fopen("scheduler.perf", "w");
    // printf("totalrunningtimes = %d now =    %d", TotalRunningTimes, now);
    fprintf(prefFIle, "CPU utilization = %d%%\n", TotalRunningTimes * 100 / now);
    int numberOfProcesses = vec_length(WTAs);
    float AvgWTA = 1.0 * TotalWTA / numberOfProcesses;
    fprintf(prefFIle, "Avg WTA = %.2f\n", AvgWTA);

    fprintf(prefFIle, "Avg Waiting = %.2f\n", 1.0 * TotalWaitings / numberOfProcesses);
    float STDWTA = 0;
    for (int i = 0; i < numberOfProcesses; i++)
        STDWTA += pow(vec_get(WTAs, i) - AvgWTA, 2);
    STDWTA = sqrt(STDWTA / numberOfProcesses);
    fprintf(prefFIle, "Std WTA = %.2f\n", STDWTA);
    fclose(prefFIle);
}