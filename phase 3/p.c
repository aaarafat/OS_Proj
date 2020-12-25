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
#define BSZ 0
#define NUM 1
#define ADD 2
#define REM 3
union Semun
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};

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

struct msgbuff
{
    long mtype;
    char mtext[70];
};

int main()
{

    int send_val, rec_val;
    int bsize = 5;
    int bdata[4];
    bdata[0] = bsize;
    bdata[1] = 0;    // current full entries
    bdata[2] = 0;    // add here
    bdata[3] = 0;    // remove from here
    int buff[bsize]; // buffer array itself
    int bdataid = shmget(ftok("key", 300), sizeof(int) * 4, IPC_CREAT | 0644);
    int bufferid = shmget(ftok("key", 301), sizeof(int) * bsize, IPC_CREAT | 0644);
    int msgq_id = msgget(ftok("key", 302), 0666 | IPC_CREAT);
    int mutex = semget(ftok("key", 303), 1, 0666 | IPC_CREAT);
    if (bdataid == -1 || bufferid == -1 || mutex == -1 || msgq_id == -1)
    {
        perror("Error in create");
        exit(-1);
    }

    union Semun semun;
    semun.val = 1; /* initial value of the semaphore, Binary semaphore */
    if (semctl(mutex, 0, SETVAL, semun) == -1)
    {

        perror("Error in semctl");
        exit(-1);
    }
    ////////////////////// write and attach the shared memory ////////////////////////////////
    void *shmaddr1 = shmat(bdataid, (void *)0, 0);
    if (*((int *)shmaddr1) == -1)
    {
        perror("Error in attach in writer");
        exit(-1);
    }
    int *pdata = (int *)shmaddr1;
    for (int i = 0; i < 4; i++)
    {
        pdata[i] = bdata[i];
    }
    void *shmaddr2 = shmat(bufferid, (void *)0, 0);
    if (*((int *)shmaddr2) == -1)
    {
        perror("Error in attach in writer");
        exit(-1);
    }
    int *pbuffer = (int *)shmaddr2;
    for (int i = 0; i < bsize; i++)
    {
        pbuffer[i] = buff[i];
    }
    ////////////////////// ================================= ////////////////////////////////
    ///////////////////// produce ///////////////////////////////////////////////////////////
    struct msgbuff message;
    for (int i = 1; i <= 20; i++)
    {
        rec_val = msgrcv(msgq_id, &message, sizeof(message.mtext), 1, IPC_NOWAIT); // clear buffer
        /* Insert into buffer */
        down(mutex);
        if (pdata[NUM] > pdata[BSZ])
            exit(1);                       /* overflow */
        else if (pdata[NUM] == pdata[BSZ]) /* block if buffer is full */
        {
            up(mutex);
            rec_val = msgrcv(msgq_id, &message, sizeof(message.mtext), 1, !IPC_NOWAIT);
            if (rec_val == -1)
                perror("Error in receive");
            down(mutex);
        }

        /* if executing here, buffer not full so add element */
        pbuffer[pdata[ADD]] = i;
        pdata[ADD] = (pdata[ADD] + 1) % pdata[BSZ];
        pdata[NUM]++;
        up(mutex);
        printf("producer: inserted %d\n", i);
        if (pdata[NUM] == 1) /*the buffer was empty */
        {
            char str[] = "consume";
            message.mtype = 2; /* arbitrary value */
            strcpy(message.mtext, str);
            send_val = msgsnd(msgq_id, &message, sizeof(message.mtext), !IPC_NOWAIT);
            if (send_val == -1)
                perror("Errror in send");
        }
    }
    printf("producer quiting\n");
    ////////////////////// ================================= ////////////////////////////////
    return 0;
}
