#include "headers.h"

int perviousClock, currentClock, id, shmIdOfRemainingTime;
int sem_id_scheduler, sem_id_generator, sem_id_process;

/*shared memory handlers*/
int initShm(int id, int value);
void setShmValue(int shmid, int value);
int getShmValue(int shmid);
void deleteShm(int shmid);

void initSem(int id);

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

    initSem(id);

    //pervious and current clock to calculate the remaining time
    perviousClock = getClk();
    currentClock = getClk();

    int remainingTime = getShmValue(shmIdOfRemainingTime);
    while (remainingTime > 0)
    {
        down(sem_id_process);
        perviousClock = getClk();

        while (perviousClock == getClk())
            ;

        currentClock = getClk();
        // if one or more clocks passed decrease the remaining time
        remainingTime = remainingTime - (currentClock - perviousClock);
        setShmValue(remainingTime, shmIdOfRemainingTime);
        //printf("rem : %d\n", remainingTime);
        up(sem_id_scheduler);
    }
    //printf("process id = %d finished\n", id);
    //send signal to parent on termination after finishing the remaining time
    kill(getppid(), SIGUSR1);

    deleteShm(shmIdOfRemainingTime);
    semctl(sem_id_process, 1, IPC_RMID, (union Semun *)0);
    destroyClk(false);

    exit(id);
}

/* Creating Semaphores */
void initSem(int id)
{
    int semKey = ftok("keyFile", id);
    key_t key_id_sem_scheduler = ftok("keyFile", 's');
    sem_id_process = semget(semKey, 1, 0660 | IPC_CREAT);
    sem_id_scheduler = semget(key_id_sem_scheduler, 1, 0660 | IPC_CREAT);
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