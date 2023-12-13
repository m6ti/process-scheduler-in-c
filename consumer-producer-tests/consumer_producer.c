#include "../linkedlist.c"
#include "../coursework.c"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

void * consumer(void* p);
void * producer(void* p);
void * consumerWasteDisposal(void* p);


sem_t delay_consumer,sync1,sync2,delay_consumer_waste_disposal;

int items = 0, maxNumProcesses = 5, runs = 0;

int main() {
    pthread_t consumerThread, producerThread,disposalThread;

    sem_init(&delay_consumer_waste_disposal, 0, 0);
    sem_init(&delay_consumer, 0, 0);
    sem_init(&sync1, 0, 1);
    sem_init(&sync2, 0, 1);


    pthread_create((&producerThread), NULL, producer, NULL);
    pthread_create((&consumerThread), NULL, consumer, NULL);
    pthread_create((&disposalThread), NULL, consumerWasteDisposal, NULL);


    pthread_join(consumerThread, NULL);
    pthread_join(producerThread, NULL);
    pthread_join(disposalThread, NULL);

}


void * consumer(void* p){
    sem_wait(&delay_consumer);
    while(runs<75){
        sem_wait(&sync1);
        items -- ;
        runs++;
        printf("SIMULATOR: %d\n", items);
        if(items == 4){
//            printf("Posted Terminator...Now\n");
            sem_post(&delay_consumer_waste_disposal);
            sem_wait(&sync2);
//            printf(" finished waiting for sync2\n");

            sem_post(&sync1);
            sem_wait(&delay_consumer);
        }
        else{
            sem_post(&sync1);
        }
    }
}

void * consumerWasteDisposal(void* p){
    sem_wait(&sync2);
    sem_wait(&delay_consumer_waste_disposal);
    while(runs<75){
//        sem_wait(&sync2);
        printf("TERMINATOR $$\n");
//        sem_post(&sync2);
        sem_post(&sync2);
        sem_wait(&delay_consumer_waste_disposal);
//        printf("finished terminator loop\n");
    }
}

void * producer(void* p){
    while(runs<75){
        sem_wait(&sync1);
        while(items != 5){
            items++;
            runs++;
            printf("GENERATOR: %d\n", items);
        }
        sem_post(&delay_consumer);
        sem_post(&sync1);
    }
    //After execution to stop the thing waiting forever.
    sem_post(&sync2);
    sem_post(&delay_consumer_waste_disposal);
}