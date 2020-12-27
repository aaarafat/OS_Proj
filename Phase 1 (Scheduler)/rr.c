#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX 100

int main()
{
  FILE *inFile;
  inFile = fopen("processes", "r");
  if (!inFile)
  {
    printf("Error\n");
    exit(1);
  }

  int quantum = 2;

  char idTemp[50], arrivalTemp[50], runtimeTemp[50], priorityTemp[50];
  int id[MAX], arrival[MAX], runtime[MAX], priority[MAX];
  int n = 0;

  while (fscanf(inFile, "%s\t%s\t%s\t%s", idTemp, arrivalTemp, runtimeTemp, priorityTemp) != EOF)
  {
    if (idTemp[0] == '#')
      continue;
    id[n] = atoi(idTemp);
    arrival[n] = atoi(arrivalTemp);
    runtime[n] = atoi(runtimeTemp);
    priority[n] = atoi(priorityTemp);
    n++;
  }

  fclose(inFile);

  FILE *outFile;
  outFile = fopen("out", "w");
  if (!outFile)
  {
    printf("Error\n");
    exit(1);
  }

  fprintf(outFile, "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n");

  int remaining = n;
  int i = 0;
  int time = arrival[0];
  int *rem = (int *)malloc(n * sizeof(int));

  for (int i = 0; i < n; i++)
    rem[i] = runtime[i];

  char *states[] = {
      "started",
      "resumed",
  };

  int lastIdx = -1;
  int wt;

  while (1)
  {
    int timeSlot = quantum <= rem[i] ? quantum : rem[i];

    wt = time - arrival[i] - (runtime[i] - rem[i]);

    if (lastIdx != i)
      fprintf(outFile, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", time, id[i], states[rem[i] != runtime[i]], arrival[i], runtime[i], rem[i], wt);

    rem[i] -= timeSlot;
    time += timeSlot;

    if (rem[i] == 0)
    {
      int ta = time - arrival[i];
      int wta = ta / runtime[i];
      fprintf(outFile, "At\ttime\t%d\tprocess\t%d\tfinished\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\tTA\t%d\tWTA\t%d\n", time, id[i], arrival[i], runtime[i], rem[i], wt, ta, wta);
      remaining--;
      if (!remaining)
        break;
    }

    lastIdx = i;

    if (i < n - 1 && arrival[i + 1] <= time)
      i++;
    else
      i = 0;

    while (rem[i] == 0)
      i = (i + 1) % n;

    if (arrival[i] > time)
      time = arrival[i];

    if (rem[lastIdx] != 0 && i != lastIdx)
    {
      fprintf(outFile, "At\ttime\t%d\tprocess\t%d\tstopped\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n", time, id[lastIdx], arrival[lastIdx], runtime[lastIdx], rem[lastIdx], wt);
    }
  }

  fclose(outFile);
  free(rem);
}