#include "headers.h"

void clearResources(int);

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.
    FILE *fptr;
    if ((fptr = fopen("processes.txt","r")) == NULL)
    {
       printf("Error! processes.txt is not found");
       exit(1);
    }

    /* get the number of process */
    int n = 0; char ctmp;
    while ((ctmp = fgetc(fptr)) != EOF)
    {
        if (ctmp == '#')
        {
            while ((ctmp = fgetc(fptr)) != EOF && ctmp != '\n');
            continue;
        }
        fseek(fptr, -1L, SEEK_CUR);
        while ((ctmp = fgetc(fptr)) != EOF && ctmp != '\n');
        n++;
    }

    /* read processes */
    fseek(fptr, 0, SEEK_SET);
    process parr[n];
    n = 0;
    while ((ctmp = fgetc(fptr)) != EOF)
    {
        if (ctmp == '#')
        {
            while ((ctmp = fgetc(fptr)) != EOF && ctmp != '\n');
            continue;
        }
        fseek(fptr, -1L, SEEK_CUR);
        fscanf(fptr, "%d\t%d\t%d\t%d\n", &parr[n].id, &parr[n].arrivaltime, &parr[n].runningtime, &parr[n].priority);
        n++;
    }

    // for(int i = 0; i < n; i++) 
    //     printf("%d\t%d\t%d\t%d\n", parr[i].id, parr[i].arrivaltime, parr[i].runningtime, parr[i].priority);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    // 3. Initiate and create the scheduler and clock processes.
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
}
