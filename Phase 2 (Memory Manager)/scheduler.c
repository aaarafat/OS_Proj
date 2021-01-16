#include "linked_list.h"
#include "vector.h"
#include <math.h>
void init();
void processTerminatedHandler(int signum);
void clearResources(int signum);

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
int forkNewProcess(Node *processNode);

/* memory functions */
void allocateMemoryFor(Node *processNode);
void insertAndCreateMemory(int mem, int start, int end);
void insertMemory(int mem, memoryBlock *memBlock);
void splitMemory(int mem);
memoryBlock *allocateMemory(int mem);
void deallocateMemory(Node *processNode);
bool isMemoryAvailableFor(Node *processNode);
void printMem();

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

int remainingProcesses = 0;
bool processIsComming = true;
//output files
FILE *logFile, *prefFIle;

Node *runningProcessNode;

int now;         // current clock
FILE *memoryLog; //output file

memoryBlock *memoryBlocks[MEMORY_SIZE + 1];
//varabiles for pref file
vec *WTAs; //store all WTA of all processes
int TotalRunningTimes, TotalWaitings;
float TotalWTA;

int main(int argc, char *argv[])
{
    // initialize all
    init();

    signal(SIGINT, clearResources);

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
    fclose(memoryLog);
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
    //open scheduler log file to write
    logFile = fopen("scheduler.log", "w");
    fprintf(logFile, "#At Time\tx\tprocess\tY\tstate arr\tw\ttotal\tz\tremain\ty\twait\tk\n");
    //open memory log file to write
    memoryLog = fopen("memory.log", "w");

    fprintf(memoryLog, "#At time\tx\tallocated\ty\tbytes for process\tz\tfrom\ti\tto\tj\n");
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

    // init signal handler
    signal(SIGUSR1, processTerminatedHandler);

    // init memory
    insertAndCreateMemory(MEMORY_SIZE, 0, 1023);
}

int forkNewProcess(Node *processNode)
{
    int processPID = fork();
    if (processPID == 0)
    {
        int id = processNode->process.id;
        int remainingTime = processNode->PCB.remainingTime;
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

    allocateMemoryFor(processNode);
    return processPID;
}

Node *storeProcessData()
{
    process p = *shm_proc_addr;
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
    newNode->PCB.memBlock = NULL;
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
    if (!processNode)
        return;

    if (processNode->PCB.processState == RUNNING)
    {
        up(processNode->PCB.semid);
        down(sem_id_scheduler);
        return;
    }

    // set process state to running
    processNode->PCB.processState = RUNNING;

    // if this is the first time i run the process
    if (processNode->PCB.PID == -1)
    {
        //print to log file
        processNode->PCB.waitingTime = getClk() - processNode->process.arrivaltime - processNode->PCB.executionTime;
        fprintf(logFile, "At Time\t%d\tprocess\t%d\tstarted arr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", getClk(), processNode->process.id,
                processNode->process.arrivaltime, processNode->process.runningtime,
                processNode->PCB.remainingTime, processNode->PCB.waitingTime);

        processNode->PCB.PID = forkNewProcess(processNode);
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

        fprintf(logFile, "At Time\t%d\tprocess\t%d\tfinished arr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\tTA\t%d\tWTA\t%.2f\n", getClk(), processNode->process.id,
                processNode->process.arrivaltime, processNode->process.runningtime,
                processNode->PCB.remainingTime, processNode->PCB.waitingTime, TA, WTA);
        deallocateMemory(deletedProcess);
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

            while (!isMemoryAvailableFor(runningProcessNode))
            {
                runningProcessNode = runningProcessNode->next;
                if (!runningProcessNode)
                    runningProcessNode = head;
            }

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

void processTerminatedHandler(int signum)
{
    int stat_loc, pid;
    pid = wait(&stat_loc);
    int id = stat_loc >> 8;
    Node *terminatedProcess = findNodeWithID(head, id);

    if (terminatedProcess)
    {
        terminatedProcess->PCB.processState = TERMINATED;
        terminatedProcess->PCB.remainingTime = 0;
        terminatedProcess->PCB.executionTime = terminatedProcess->process.runningtime;
        removeProcess(terminatedProcess);
    }

    printf("process ID=%d terminated  Time = %d\n", id, getClk());
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

// Calculates log2 of number.
int Log2(int n)
{
    int res = 0;
    n--;
    while (n)
    {
        res++;
        n >>= 1;
    }
    return res;
}

memoryBlock *allocateMemory(int mem)
{
    printf("allocated memory from %d to %d Time = %d\n", memoryBlocks[mem]->start, memoryBlocks[mem]->end, getClk());
    memoryBlock *allocatedMemoryBlock = memoryBlocks[mem];
    memoryBlocks[mem] = memoryBlocks[mem]->next;

    //printMem();
    allocatedMemoryBlock->next = NULL;
    return allocatedMemoryBlock;
}

void splitMemory(int mem)
{
    memoryBlock *deletedMemoryBlock = memoryBlocks[mem];
    memoryBlocks[mem] = memoryBlocks[mem]->next;
    int mid = (deletedMemoryBlock->end - deletedMemoryBlock->start + 1) / 2;
    insertAndCreateMemory(mem - 1, deletedMemoryBlock->start, deletedMemoryBlock->start + mid - 1);
    insertAndCreateMemory(mem - 1, deletedMemoryBlock->start + mid, deletedMemoryBlock->end);
    free(deletedMemoryBlock);
}

void insertAndCreateMemory(int mem, int start, int end)
{
    memoryBlock *memBlock = (memoryBlock *)malloc(sizeof(memoryBlock));
    memBlock->start = start;
    memBlock->end = end;
    memBlock->next = NULL;

    insertMemory(mem, memBlock);
}

void insertMemory(int mem, memoryBlock *memBlock)
{
    //printf("inserted mem from %d to %d \n", memBlock->start, memBlock->end);
    if (!memoryBlocks[mem])
    {
        memoryBlocks[mem] = memBlock;
        return;
    }

    if (memoryBlocks[mem]->start > memBlock->start)
    {
        memoryBlock *nxt = memoryBlocks[mem];
        memoryBlocks[mem] = memBlock;
        memBlock->next = nxt;
    }

    memoryBlock *headMem = memoryBlocks[mem];

    while (headMem->next && headMem->next->start < memBlock->start)
        headMem = headMem->next;

    memoryBlock *nxt = headMem->next;
    headMem->next = memBlock;
    memBlock->next = nxt;
}

void printMem()
{
    for (int mem = 0; mem <= MEMORY_SIZE; mem++)
    {
        memoryBlock *headMem = memoryBlocks[mem];
        if (!headMem)
            continue;
        printf("mem : %d   ", mem);
        while (headMem)
        {
            printf("start : %d end : %d, ", headMem->start, headMem->end);
            headMem = headMem->next;
        }
        printf("\n");
    }
}

/* allocate memory for a process */
void allocateMemoryFor(Node *processNode)
{
    int mem = Log2(processNode->process.memsize);
    if (memoryBlocks[mem])
    {
        fprintf(memoryLog, "At time\t%d\tallocated\t%d\tbytes for process\t%d\tfrom\t%d\tto\t%d\n", getClk(), processNode->process.memsize,
                processNode->process.id, memoryBlocks[mem]->start, memoryBlocks[mem]->end);
        processNode->PCB.memBlock = allocateMemory(mem);
        return;
    }

    int memIdx;
    for (memIdx = mem + 1; memIdx <= MEMORY_SIZE && !memoryBlocks[memIdx]; memIdx++)
        ;

    if (memIdx > MEMORY_SIZE)
    {
        perror("You Don't have enough memory!!\n");
        exit(1);
    }

    while (memIdx != mem)
        splitMemory(memIdx--);

    fprintf(memoryLog, "At time\t%d\tallocated\t%d\tbytes for process\t%d\tfrom\t%d\tto\t%d\n", getClk(), processNode->process.memsize,
            processNode->process.id, memoryBlocks[mem]->start, memoryBlocks[mem]->end);
    processNode->PCB.memBlock = allocateMemory(mem);
}

void deallocateMemory(Node *processNode)
{
    memoryBlock *memBlock = processNode->PCB.memBlock;
    int mem = Log2(memBlock->end - memBlock->start);
    insertMemory(mem, memBlock);

    printf("freed memory from %d to %d Time = %d\n", memBlock->start, memBlock->end, getClk());
    fprintf(memoryLog, "At time\t%d\tfreed\t%d\tbytes from process\t%d\tfrom\t%d\tto\t%d\n", getClk(), processNode->process.memsize,
            processNode->process.id, memBlock->start, memBlock->end);
    bool found = true;
    while (found)
    {
        found = false;

        if (!memoryBlocks[mem] || !memoryBlocks[mem]->next)
            break;

        //printf("start %d  start %d\n", memoryBlocks[mem]->start, memoryBlocks[mem]->next->start);
        memoryBlock *headMem = memoryBlocks[mem];
        while (headMem && headMem->next)
        {
            if (headMem->end + 1 == headMem->next->start)
            {
                // Todo : change this
                int blockSize = headMem->next->end - headMem->start + 1;
                if (headMem->start % blockSize == 0)
                {
                    found = true;
                    memoryBlock *nxt = headMem->next;
                    headMem->next = nxt->next;

                    //printf("st : %d  en : %d\n", headMem->start, nxt->end);

                    insertAndCreateMemory(mem + 1, headMem->start, nxt->end);

                    headMem->start = -1; // will be deleted

                    free(nxt);
                }
            }
            headMem = headMem->next;
        }

        if (!found)
            break;

        // now delete temp memBlocks
        while (memoryBlocks[mem] && memoryBlocks[mem]->start == -1)
        {
            // remove it and skip
            memoryBlock *temp = memoryBlocks[mem];
            memoryBlocks[mem] = memoryBlocks[mem]->next;
            free(temp);
        }

        headMem = memoryBlocks[mem];
        while (headMem && headMem->next)
        {
            if (headMem->next->start == -1)
            {
                memoryBlock *temp = headMem->next;
                headMem->next = temp->next;
                free(temp);
            }
            headMem = headMem->next;
        }

        mem++;
    }

    //printMem();
}

void clearResources(int signum)
{
    // remove memory
    for (int i = 0; i <= MEMORY_SIZE; i++)
    {
        while (memoryBlocks[i])
        {
            memoryBlock *tmp = memoryBlocks[i];
            memoryBlocks[i] = memoryBlocks[i]->next;
            free(tmp);
        }
    }
}
void outputPref()
{
    prefFIle = fopen("scheduler.perf", "w");
    // printf("totalrunningtimes = %d now =    %d", TotalRunningTimes, now);
    fprintf(prefFIle, "CPU utilization= %d%%\n", TotalRunningTimes * 100 / now);
    int numberOfProcesses = vec_length(WTAs);
    float AvgWTA = 1.0 * TotalWTA / numberOfProcesses;
    fprintf(prefFIle, "Avg WTA= %.2f\n", AvgWTA);

    fprintf(prefFIle, "Avg Waiting = %.2f\n", 1.0 * TotalWaitings / numberOfProcesses);
    float STDWTA = 0;
    for (int i = 0; i < numberOfProcesses; i++)
        STDWTA += pow(vec_get(WTAs, i) - AvgWTA, 2);
    STDWTA = sqrt(STDWTA / numberOfProcesses);
    fprintf(prefFIle, "Std WTA = %.2f\n", STDWTA);
    fclose(prefFIle);
}

bool isMemoryAvailableFor(Node *processNode)
{
    if (processNode == NULL)
        return 0;

    if (processNode->PCB.PID != -1)
        return 1;

    int mem = Log2(processNode->process.memsize);

    int memIdx;
    for (memIdx = mem; memIdx <= MEMORY_SIZE && !memoryBlocks[memIdx]; memIdx++)
        ;

    if (memIdx > MEMORY_SIZE)
        return 0;

    return 1;
}