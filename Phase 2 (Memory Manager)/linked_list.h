#include "headers.h"

enum processState
{
  RUNNING,
  WAITING,
  TERMINATED,
  NO_STATES
};

/* PCP struct */
struct PCB_Struct
{
  enum processState processState;
  int executionTime;
  int remainingTime;
  int waitingTime;
  int PID;
  int shmid;
  int semid;
};

typedef struct PCB_Struct PCB;

/* processes data node struct */
struct NodeStruct
{
  process process;
  PCB PCB;
  struct NodeStruct *next;
  struct NodeStruct *prev;
};

typedef struct NodeStruct Node;

/* memory block struct for Buddy Memory Allocation Algorithm*/
struct memoryBlockStruct
{
  int start, end;
  struct memoryBlockStruct *next;
};

typedef struct memoryBlockStruct memoryBlock;

/* create empty linked list */
Node *createLinkedList()
{
  Node *head = NULL;
  return head;
}

/* insert newNode to linked list with head */
void insert(Node **head, Node **newNode)
{
  if (*head == NULL)
  {
    *head = *newNode;
    return;
  }

  Node *tmpNode = *head;
  while (tmpNode->next != NULL)
    tmpNode = tmpNode->next;
  tmpNode->next = *newNode;
  (*newNode)->prev = tmpNode;
}

/*
insert new Node in the linked list according to it's priorty 
*/
void insertionSortWithPriority(Node **head, Node **newNode)
{
  if (*head == NULL)
  {
    *head = *newNode;
    return;
  }
  if ((*head)->process.priority > (*newNode)->process.priority)
  {
    (*newNode)->next = *head;
    (*head)->prev = *newNode;
    *head = *newNode;
    return;
  }
  Node *tmpNode = *head;

  while (tmpNode->next && tmpNode->next->process.priority <= (*newNode)->process.priority)
    tmpNode = tmpNode->next;

  Node *nxt = tmpNode->next;

  if (nxt)
  {
    tmpNode->next = *newNode;
    (*newNode)->prev = tmpNode;
    (*newNode)->next = nxt;
    nxt->prev = *newNode;
  }
  else
  {
    tmpNode->next = *newNode;
    (*newNode)->prev = tmpNode;
  }
}
/* 
remove a node from linkec list with the given id
returns the removed Node 
(if the node is not found returns NULL)
*/
Node *removeNodeWithID(Node **head, int id)
{
  if (*head == NULL)
    return *head;

  Node *tmpNode = *head;

  if ((*head)->process.id == id)
  {
    *head = (*head)->next;
    if (*head != NULL)
      (*head)->prev = NULL;
    return tmpNode;
  }

  while (tmpNode->next != NULL && tmpNode->next->process.id != id)
    tmpNode = tmpNode->next;

  if (tmpNode->next == NULL)
    return NULL;

  Node *removedNode = tmpNode->next;

  tmpNode->next = removedNode->next;
  removedNode->prev = tmpNode;

  return removedNode;
}

/* find Node with the given id (returns NULL if not found) */
Node *findNodeWithID(Node *head, int id)
{
  Node *tmpNode = head;
  while (tmpNode != NULL && tmpNode->process.id != id)
    tmpNode = tmpNode->next;
  return tmpNode;
}

void printList(Node **head)
{
  Node *tmpNode = *head;
  while (tmpNode)
  {
    printf("id = %d, p = %d\n", tmpNode->process.id, tmpNode->process.priority);
    tmpNode = tmpNode->next;
  }
}
