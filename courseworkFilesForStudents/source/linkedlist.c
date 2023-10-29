#include <stdlib.h>
#include "linkedlist.h"
#include <pthread.h>

Element * getHead(LinkedList oList) {
  return oList.pHead;
}

Element * getNext(Element * pCurrent) {
  if(pCurrent != NULL)
    return pCurrent->pNext;
  return NULL;
}

void addLast(void * pData, LinkedList * pList) {
  Element ** pHead =  &(pList->pHead);
  Element ** pTail = &(pList->pTail); 
  Element * pNewElement = (Element *) malloc (sizeof(Element));
  (*pNewElement) = (Element) ELEMENT_INITIALIZER;
  pNewElement->pData = pData;
  
  if((*pHead) == NULL)
  {
    // element becomes first one in list
    pNewElement->pPrevious = (*pHead);
    pNewElement->pNext = (*pHead);
    (*pTail) = (*pHead) = pNewElement;
  }
  else 
  {
    pNewElement->pPrevious = *pTail;
    pNewElement->pNext = NULL;
    (*pTail)->pNext = pNewElement;
    (*pTail) = pNewElement;
  }
}

void addFirst(void * pData, LinkedList * pList) {
  Element ** pHead = &(pList->pHead);
  Element ** pTail = &(pList->pTail); 
  Element * pNewElement = (Element *) malloc (sizeof(Element));
  (*pNewElement) = (Element) ELEMENT_INITIALIZER;
  pNewElement->pData = pData;
  
  if((*pHead) == NULL)
  {
    // element is the first one in the list
    pNewElement->pPrevious = NULL;
    pNewElement->pNext = (*pHead);
    (*pTail) = (*pHead) = pNewElement;
  }
  else 
  {
    (*pHead)->pPrevious = pNewElement;
    pNewElement->pNext = (*pHead);
    (*pHead) = pNewElement;
  }
}

void * removeFirst(LinkedList * pList) {
  Element ** pHead =  &(pList->pHead);
  Element ** pTail = &(pList->pTail); 
  void * pData = NULL;
  Element * pTemp = NULL;
  if((*pHead) != NULL) {
    pTemp = (*pHead);
    (*pHead) = (*pHead)->pNext;
    if((*pHead) == NULL)
      (*pTail) = NULL;
    else 
      (*pHead)->pPrevious = NULL;
    pData =  pTemp->pData;
    free(pTemp);
  }
  return pData;
}

void * removeData(void * pData, LinkedList * pList) {
  void * pTemp = NULL;
  Element * pCurrent = pList->pHead;
  while(pCurrent != NULL && pCurrent->pData != pData) {
    pCurrent = pCurrent->pNext;
  }
  if(pCurrent != NULL) {
    if(pCurrent->pPrevious != NULL)
      pCurrent->pPrevious->pNext = pCurrent->pNext;
    if(pCurrent->pNext != NULL) 
      pCurrent->pNext->pPrevious = pCurrent->pPrevious;
    if(pCurrent == pList->pHead)
      pList->pHead = pCurrent->pNext;
    if(pCurrent == pList->pTail)
      pList->pTail = pCurrent->pPrevious;
    pTemp = pCurrent->pData;
    free(pCurrent);
  }
  return pData;
}
