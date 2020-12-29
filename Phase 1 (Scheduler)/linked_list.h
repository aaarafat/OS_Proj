#include "headers.h"

/* processes data node struct */
struct Node
{
  process process;
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
