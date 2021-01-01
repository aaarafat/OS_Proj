#include "headers.h"

int perviousClock, currentClock, id, shmIdOfRemainingTime;

/*signals handlers*/
void stopHandler(int signum);
void continueHandler(int signum);

/*shared memory handlers*/
int initShm(int id, int value);
void setShmValue(int shmid, int value);
int getShmValue(int shmid);
void deleteShm(int shmid);

int main(int argc, char *argv[])
{
    initClk();
    //initliaize the remaining time with the running time passed in commandline arguments
    if (argc <= 2)
    {
        printf("too few arguments\n");
        return 0;
    }
    int runningTime = atoi(argv[2]);
    id = atoi(argv[1]);
    shmIdOfRemainingTime = initShm(id, runningTime);

    //pervious and current clock to calculate the remaining time
    perviousClock = getClk();
    currentClock = getClk();

    //attach handlers to used singals
    signal(SIGTSTP, stopHandler);
    signal(SIGCONT, continueHandler);

    int remainingTime = getShmValue(shmIdOfRemainingTime);
    while (remainingTime > 0)
    {
        remainingTime = getShmValue(shmIdOfRemainingTime);
        perviousClock = currentClock;
        currentClock = getClk();

        // if one or more clocks passed decrease the remaining time
        if (currentClock - perviousClock > 0)
            setShmValue(remainingTime - (currentClock - perviousClock), shmIdOfRemainingTime);
    }

    //send signal to parent on termination after finishing the remaining time
    kill(getppid(), SIGUSR1);

    deleteShm(shmIdOfRemainingTime);
    destroyClk(false);
    exit(0);
}

/*SIGTSTP handler*/
void stopHandler(int signum)
{
    /*it may happens that a new clock cycle will begin 
    right before blocking  the process */
    int remainingTime = getShmValue(shmIdOfRemainingTime);
    if (currentClock - perviousClock > 0)
        setShmValue(remainingTime - (currentClock - perviousClock), shmIdOfRemainingTime);

    //blocking the process
    kill(getpid(), SIGSTOP);
}

/*SIGCONT handler*/
void continueHandler(int signum)
{
    // reset the pervious and current clock
    perviousClock = getClk();
    currentClock = getClk();
}

/*initlizer remainging time as a shared memory varible*/
int initShm(int id, int value)
{
    /*creating shared memory variable*/
    int shmKey = ftok("keyFile", id);
    int shmid = shmget(shmKey, 4, IPC_CREAT | 0666);
    if ((long)shmid == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }

    /* attach shared memory  */
    int *shmaddr = (int *)shmat(shmid, (void *)0, 0);

    if ((long)shmaddr == -1)
    {

        printf("Error in attaching the shm in process with id: %d\n", id);
        perror("");
        exit(-1);
    }
    *shmaddr = value; /* initialize shared memory */
    // deatach shared memory
    shmdt(shmaddr);
    return shmid;
}

/*set shared memory  value*/
void setShmValue(int value, int shmid)
{
    //attach shared memory
    int *shmaddr = (int *)shmat(shmid, (void *)0, 0);
    if ((long)shmaddr == -1)
    {
        printf("Error in attaching the shm in process with id: %d\n", id);
        perror("");
        exit(-1);
    }
    *shmaddr = value; /*decrement */
    // deatach shared memory
    shmdt(shmaddr);
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

/*delete shared memory */
void deleteShm(int shmid)
{
    shmctl(shmid, IPC_RMID, NULL);
}