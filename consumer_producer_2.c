#include "linkedlist.c"
#include "coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

void * consumer(void* p);
void * producer(void* p);
void * disposal(void* p);


sem_t
    empty,
    full,
    sync1,disposalSync,disposalDone;

int items = 0, runs = 0;

int main() {
    pthread_t consumerThread, producerThread, disposalThread;

    sem_init(&sync1, 0, 1);
    sem_init(&full, 0, 0);
    sem_init(&empty, 0, 1);
    sem_init(&disposalSync, 0, 0);
    sem_init(&disposalDone, 0, 0);

    pthread_create((&producerThread), NULL, producer, NULL);
    pthread_create((&consumerThread), NULL, consumer, NULL);
    pthread_create((&disposalThread), NULL, disposal, NULL);

    pthread_join(consumerThread, NULL);
    pthread_join(producerThread, NULL);
    pthread_join(disposalThread, NULL);
}

void * consumer(void* p){

    while(runs<50){
        sem_wait(&full);
        sem_wait(&sync1);

        runs ++;
        items -- ;
        printf("Simulator: %d\n", items);

        sem_post(&disposalSync);
        sem_wait(&disposalDone);

        sem_post(&sync1);
        sem_post(&empty);
    }
}

void *disposal(void *p) {

    while (runs < 50) {
        sem_wait(&disposalSync);

        printf("Disposal Thread: Executing after Simulator\n");

        sem_post(&disposalDone);
    }

}

void * producer(void* p){
    while(runs<50){
        sem_wait(&empty);
        sem_wait(&sync1);
        while(items<1) {
            runs++;
            items++;
            printf("Generator: %d\n", items);
        }
        sem_post(&sync1);
        sem_post(&full);
    }
}