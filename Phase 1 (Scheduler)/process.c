#include "headers.h"

/* Modify this file as needed*/
int remainingTime, perviousClock, currentClock;

void stopHandler(int signum);
void continueHandler(int signum);

int main(int argc, char *argv[])
{
    initClk();
    //initliaize the remaining time with the running time passed in commandline arguments
    if (argc <= 1)
    {
        printf("too few arguments\n");
        return 0;
    }
    int runningTime = atoi(argv[1]);
    remainingTime = runningTime;

    //pervious and current clock to calculate the remaining time
    perviousClock = getClk();
    currentClock = getClk();

    //attach handlers to used singals
    signal(SIGTSTP, stopHandler);
    signal(SIGCONT, continueHandler);

    while (remainingTime > 0)
    {
        perviousClock = currentClock;
        currentClock = getClk();
        // if one or more clocks passed decrease the remaining time
        remainingTime -= currentClock - perviousClock;
    }

    destroyClk(false);
    //send signal to parent on termination after finishing the remaining time
    kill(getppid(), SIGUSR1);
    exit(0);
}
void stopHandler(int signum)
{
    /*it may happens that a new clock cycle will begin 
    right before blocking  the process */
    remainingTime -= currentClock - perviousClock;

    //blocking the process
    kill(getpid(), SIGSTOP);
}
void continueHandler(int signum)
{
    // reset the pervious and current clock
    perviousClock = getClk();
    currentClock = getClk();
}