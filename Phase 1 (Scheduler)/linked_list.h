#include "headers.h"

enum processState
{
  RUNNING,
  WAITING,
  NO_STATES
};

/* PCP struct */
struct PCB_Struct
{
  processState processState;
  int executionTime;
  int remainingTime;
  int waitingTime;
  int PID;
};

typedef struct PCB_Struct PCB;

/* processes data node struct */
struct Node
{
  process process;
  PCB PCB;
  Node *next;
};

/* create empty linked list */
Node *createLinkedList()
{
  Node *head = NULL;
  return head;
}

/* insert newNode to linked list with head */
void insert(Node *head, Node *newNode)
{
  if (head == NULL)
  {
    head = newNode;
    return;
  }

  Node *tmpNode = head;
  while (tmpNode->next != NULL)
    tmpNode = tmpNode->next;
  tmpNode->next = newNode;
}

/* 
remove a node from linkec list with the given id
returns the removed Node 
(if the node is not found returns NULL)
*/
Node *removeNodeWithID(Node *head, int id)
{
  if (head == NULL)
    return head;

  Node *tmpNode = head;

  if (head->process.id == id)
  {
    head = head->next;
    return tmpNode;
  }

  while (tmpNode->next != NULL && tmpNode->next->process.id != id)
    tmpNode = tmpNode->next;

  if (tmpNode->next == NULL)
    return NULL;

  Node *removedNode = tmpNode->next;

  tmpNode->next = removedNode->next;

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