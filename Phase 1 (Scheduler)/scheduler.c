#include "headers.h"


int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    printf("scheduler starting...\n");
    sleep(5);
    printf("scheduler terminates...\n");
    destroyClk(true);
}
