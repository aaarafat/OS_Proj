#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#define BSZ 0
#define NUM 1
#define ADD 2
#define REM 3
struct msgbuff
{
    long mtype;
    char mtext[70];
};

union Semun
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};
void cleanSegements(int bdataid, int bufferid, int msgq_id, int mutex)
{
    if (bdataid != -1)
    {
        shmctl(bdataid, IPC_RMID, (struct shmid_ds *)0);
    }
    if (bufferid != -1)
    {
        shmctl(bufferid, IPC_RMID, (struct shmid_ds *)0);
    }
    if (msgq_id != -1)
    {
        msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    }
    if (mutex != -1)
    {
        semctl(mutex, 0, IPC_RMID);
    }
}
void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}
int bdataid = -1, bufferid = -1, msgq_id = -1, mutex = -1;
void handler(int signum)
{
    cleanSegements(bdataid, bufferid, msgq_id, mutex);
    killpg(getpgrp(), SIGKILL);
}
int main()
{
    signal(SIGINT, handler);
    union Semun semun;
    int rec_val, send_val;
    int bsize = 0;
    int bdata[4];
    bdata[0] = bsize;
    bdata[1] = 0;    // current full entries
    bdata[2] = 0;    // add here
    bdata[3] = 0;    // remove from here
    int buff[bsize]; // buffer array itself
    int consumer = semget(ftok("key", 102), 1, 0666 | IPC_CREAT);
    semun.val = 1; /* initial value of the semaphore, Binary semaphore */
    if (semctl(consumer, 0, SETVAL, semun) == -1)
    {

        perror("Error in semctl");
        exit(-1);
    }
    int producer = semget(ftok("key", 101), 1, 0666);
    down(consumer);
    if (producer != -1)
    {
        down(producer);
    }
    bdataid = shmget(ftok("key", 300), sizeof(int) * 4, 0644);
    msgq_id = msgget(ftok("key", 302), 0666 | IPC_CREAT);
    mutex = semget(ftok("key", 303), 1, 0666 | IPC_CREAT);
    int *pdata;
    if (mutex == -1 || msgq_id == -1)
    {
        perror("Error in create");
        exit(-1);
    }
    if (bdataid == -1)
    {
        printf("Enter buffer size : \n");
        scanf("%d", &bsize);
        bdata[0] = bsize;
        printf("init bdata[0] : %d \n", bdata[0]);
        bdataid = shmget(ftok("key", 300), sizeof(int) * 4, IPC_CREAT | 0644);
        void *shmaddr1 = shmat(bdataid, (void *)0, 0);
        if (*((int *)shmaddr1) == -1)
        {
            perror("Consumer : Error in attach in writer");
            exit(-1);
        }
        printf("Created shared memory \n");
        pdata = (int *)shmaddr1;
        for (int i = 0; i < 4; i++)
        {
            pdata[i] = bdata[i];
        }
    }
    if (producer != -1)
        up(producer);
    up(consumer);
    ////////////////////// wait and attach the shared memory ////////////////////////////////
    void *shmaddr = shmat(bdataid, (void *)0, 0);
    printf("attached to shared memory \n");

    if (*((int *)shmaddr) == -1)
    {
        perror("Error in attach in data writer");
        cleanSegements(bdataid, bufferid, msgq_id, mutex);
        exit(-1);
    }

    pdata = (int *)shmaddr;
    bufferid = shmget(ftok("key", 301), sizeof(int) * pdata[BSZ], IPC_CREAT | 0644);
    void *shmaddr2 = shmat(bufferid, (void *)0, 0);
    if (*((int *)shmaddr2) == -1)
    {
        perror("Error in attach in buffer writer");
        cleanSegements(bdataid, bufferid, msgq_id, mutex);
        exit(-1);
    }
    int *pbuffer = (int *)shmaddr2;
    ////////////////////// ================================= ////////////////////////////////
    printf("Consumer is attached to the shared memory\n");
    struct msgbuff message;
    int i;
    while (1)
    {
        rec_val = msgrcv(msgq_id, &message, sizeof(message.mtext), 2, IPC_NOWAIT); // clear buffer
        down(mutex);
        if (pdata[NUM] < 0)
        {
            exit(1); /* underflow */
            cleanSegements(bdataid, bufferid, msgq_id, mutex);
        }
        else if (pdata[NUM] == 0) /* block if buffer empty */
        {
            up(mutex);
            rec_val = msgrcv(msgq_id, &message, sizeof(message.mtext), 2, !IPC_NOWAIT);
            if (rec_val == -1)
                perror("Error in receive");
            down(mutex);
        }
        /* if executing here, buffer not empty so remove element */
        i = pbuffer[pdata[REM]];
        pdata[REM] = (pdata[REM] + 1) % pdata[BSZ];
        pdata[NUM]--;
        printf("Consume value %d\n", i);
        if (pdata[NUM] == pdata[BSZ] - 1) /* buffer was full */
        {
            char str[] = "produce";
            message.mtype = 1; /* arbitrary value */
            strcpy(message.mtext, str);
            send_val = msgsnd(msgq_id, &message, sizeof(message.mtext), !IPC_NOWAIT);
            if (send_val == -1)
                perror("Errror in send");
        }
        up(mutex);
    }

    return 0;
}
