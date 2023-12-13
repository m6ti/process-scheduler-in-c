#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

void * consumer(void* p);
void * producer(void* p);
void * consumerWasteDisposal(void* p);


sem_t delay_consumer,sync1;

int items = 0, runs = 0;

int main() {
    pthread_t consumerThread, producerThread;

    sem_init(&delay_consumer, 0,  1);
    sem_init(&sync1, 0, 1);

    pthread_create((&producerThread), NULL, producer, NULL);
    pthread_create((&consumerThread), NULL, consumer, NULL);


    pthread_join(consumerThread, NULL);
    pthread_join(producerThread, NULL);

}
void * consumer(void* p){
    sem_wait(&delay_consumer);
    while(runs<50){
        sem_wait(&sync1);
        items -- ;
        runs++;

        printf("items: %d\n", items);

        sem_post(&sync1);

        if(items == 0){
            sem_wait(&delay_consumer);
        }
    }
}

void * producer(void* p){
    while(runs<50){
        sem_wait(&sync1);
        items++;
        runs++;

        printf("items: %d\n", items);

        if(items == 1)
            sem_post(&delay_consumer);
        sem_post(&sync1);
    }
}