#include "headers.h"

void initSem();

void clearResources(int);
void getAlgo(int *, int *, int, char **);
void initProcesses(int *, int *, int, int);
int getNumOfProcesses(FILE *);
void readProcesses(FILE *, process *);

int shmid, msqid;
int sem_id_scheduler, sem_id_generator;

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.
    FILE *fptr;
    if ((fptr = fopen("processes.txt", "r")) == NULL)
    {
        printf("Error! processes.txt is not found");
        exit(1);
    }

    /* get the number of process */
    int n = getNumOfProcesses(fptr);
    /* read processes */
    process parr[n];
    readProcesses(fptr, parr);
    // for(int i = 0; i < n; i++)
    //     printf("%d\t%d\t%d\t%d\n", parr[i].id, parr[i].arrivaltime, parr[i].runningtime, parr[i].priority);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    //./process_generator <algorithm> [parameters]
    int schAlgo = NOT_SELECTED, quantum = 2; //in case of round robin: default value = 2
    getAlgo(&schAlgo, &quantum, argc, argv);
    // 3. Initiate and create the scheduler and clock processes.
    /* Creating Semaphores */
    initSem();
    int clkPID, schPID;
    initProcesses(&clkPID, &schPID, schAlgo, quantum);
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    /* Creating Shared Memory Segment */
    shmid = shmget(PROC_SH_KEY, sizeof(process), IPC_CREAT | 0644);
    if ((long)shmid == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }
    process *shmaddr = (process *)shmat(shmid, (void *)0, 0);
    if ((long)shmaddr == -1)
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
    struct msgbuf message;
    message.mtype = 1;
    int currentProcess = 0;
    // Generation Main Loop
    while (currentProcess < n)
    {
        down(sem_id_generator);
        up(sem_id_scheduler);
        // 6. Send the information to the scheduler at the appropriate time.
        int currClk = getClk();
        if (parr[currentProcess].arrivaltime <= currClk)
        {
            *shmaddr = parr[currentProcess];
            currentProcess++;
            message.mtext = COMPLETE;
            if (currentProcess < n && parr[currentProcess].arrivaltime <= currClk)
                message.mtext = WAIT_FOR_NEXT_PROCESS;

            int sendValue = msgsnd(msqid, &message, sizeof(message.mtext), !IPC_NOWAIT);
            if (sendValue == -1)
                perror("Error in send");
        }
        else
        {
            message.mtext = NO_PROCESSES;
            int sendValue = msgsnd(msqid, &message, sizeof(message.mtext), !IPC_NOWAIT);
            if (sendValue == -1)
                perror("Error in send");
        }
    }
    down(sem_id_generator);
    up(sem_id_scheduler);
    message.mtext = FINISHED;
    int sendValue = msgsnd(msqid, &message, sizeof(message.mtext), !IPC_NOWAIT);
    if (sendValue == -1)
        perror("Error in send");

    while (true)
        ;

    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msqid, IPC_RMID, NULL);
    semctl(sem_id_scheduler, 1, IPC_RMID, (union Semun *)0);
    semctl(sem_id_generator, 1, IPC_RMID, (union Semun *)0);
    printf("process generator terminating!\n");
    destroyClk(true); // ?? not sure
    exit(0);
}

int getNumOfProcesses(FILE *fptr)
{
    int n = 0;
    char ctmp;
    while ((ctmp = fgetc(fptr)) != EOF)
    {
        if (ctmp == '#')
        {
            while ((ctmp = fgetc(fptr)) != EOF && ctmp != '\n')
                ;
            continue;
        }
        fseek(fptr, -1L, SEEK_CUR);
        while ((ctmp = fgetc(fptr)) != EOF && ctmp != '\n')
            ;
        n++;
    }
    return n;
}

void readProcesses(FILE *fptr, process *parr)
{
    fseek(fptr, 0, SEEK_SET);
    int i = 0;
    char ctmp;
    while ((ctmp = fgetc(fptr)) != EOF)
    {
        if (ctmp == '#')
        {
            while ((ctmp = fgetc(fptr)) != EOF && ctmp != '\n')
                ;
            continue;
        }
        fseek(fptr, -1L, SEEK_CUR);
        fscanf(fptr, "%d\t%d\t%d\t%d\t%d\n", &parr[i].id, &parr[i].arrivaltime, &parr[i].runningtime, &parr[i].priority, &parr[i].memsize);
        i++;
    }
}

void getAlgo(int *schAlgo, int *quantum, int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Argument Not found: ./process_generator <algorithm> [<parameters>]\n");
        printf("./process_generator.out -h for more info\n");
        exit(1);
    }
    else if (argc == 2)
    {
        if (strcmp(argv[1], "-h") == 0)
        {
            printf("./process_generator.out <algorithm> [<parameters>]\n");
            printf("<algorithm> is an integer from 0 to 2\n");
            printf("0: Non-preemptive Highest Priority First\n1: Shortest Remaining time Nex\n2: Round Robin\n");
            printf("<prameters> is optional for only Round Robin to specify the quantum and should be positive, if not specified it is %d by default\n", *quantum);
            exit(0);
        }
        *schAlgo = atoi(argv[1]);
        if (*schAlgo < 0 || *schAlgo >= NO_OF_ALGORITHMS)
        {
            printf("algorithm must range from 0 to %d got %d\n", NO_OF_ALGORITHMS - 1, *schAlgo);
        }
    }
    else if (argc == 3 && (*schAlgo = atoi(argv[1])) == RR)
    {
        *quantum = atoi(argv[2]);
        if (*quantum < 1)
        {
            printf("quantum should be postive\n");
            exit(1);
        }
    }
    else
    {
        printf("Argument Error: ./process_generator.out <algorithm> [<parameters>]\nExpected 2 or 3 arguments with algorithm ranging from 0 to %d\n", NO_OF_ALGORITHMS - 1);
        printf("./process_generator.out -h for more info\n");
        exit(1);
    }
}

void initProcesses(int *clkPID, int *schPID, int schAlgo, int quantum)
{
    *clkPID = fork();
    if (*clkPID == 0)
    {
        execl("./clk.out", "clk.out", (char *)0);
        printf("Error in executing clk.out\n");
        exit(1);
    }
    else if (*clkPID == -1)
    {
        perror("error in forking clk");
        exit(1);
    }
    *schPID = fork();
    if (*schPID == 0)
    {
        // convert args to char*
        char execStr1[2];
        char execStr2[MAX_DIGITS + 1];
        snprintf(execStr1, 2, "%d", schAlgo);
        snprintf(execStr2, MAX_DIGITS + 1, "%d", quantum);

        execl("./scheduler.out", "scheduler.out", execStr1, execStr2, (char *)0);
        printf("Error in executing scheduler.out\n");
        destroyClk(true);
        exit(1);
    }
    else if (*schPID == -1)
    {
        perror("error in forking clk");
        destroyClk(true);
        exit(1);
    }
}

/* Creating Semaphores */
void initSem()
{
    /* Creating Semaphores */
    key_t key_id_sem_scheduler = ftok("keyFile", 's');
    key_t key_id_sem_generator = ftok("keyFile", 'g');

    sem_id_scheduler = semget(key_id_sem_scheduler, 1, 0660 | IPC_CREAT);
    sem_id_generator = semget(key_id_sem_generator, 1, 0660 | IPC_CREAT);

    union Semun semun;
    semun.val = 0;
    if (semctl(sem_id_scheduler, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl : process");
        exit(-1);
    }
    semun.val = 1;
    if (semctl(sem_id_generator, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl : process");
        exit(-1);
    }
}
