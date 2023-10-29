/**************************************************************************************************************************
      DO NOT CHANGE THE CONTENTS OF THIS FILE FOR YOUR COURSEWORK. ONLY WORK WITH THE OFFICIAL VERSION
**************************************************************************************************************************/

#define LINKED_LIST_INITIALIZER {NULL, NULL}
#define ELEMENT_INITIALIZER {NULL, NULL, NULL}


/*
* Definition of a node of the linked list
*/
typedef struct element
{
  void * pData;
  struct element * pPrevious;
  struct element * pNext;
} Element;

typedef struct {
  Element * pHead;
  Element * pTail;
} LinkedList;

Element * getHead(LinkedList oList);

Element * getNext(Element * pCurrent);

/**
* This function adds an element oTemp to the end from the linked list. 
*/
void addLast(void * pData, LinkedList * pList);

/**
* This function adds an element oTemp to the beginning from the linked list. 
*/
void addFirst(void * oTemp, LinkedList * pList);

/**
* This function removes the first element from the linked list  
*/
void * removeFirst(LinkedList * pList);

/**
* This function removes the first element containing the pData from the list  
*/
void * removeData(void * pData, LinkedList * pList);
