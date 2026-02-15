#include <semaphore.h>
#include <limits.h>

#ifndef LOCAL_QUEUE_H
#define LOCAL_QUEUE_H

#define MAX_ENTRIES SEM_VALUE_MAX

typedef struct LocalQueue {

    char paths[MAX_ENTRIES][PATH_MAX];
    int  head;       // Beginning of Work Queue 
    int  tail;       // End of Work Queue
    int  size;       // Size of Work Queue

    // Mutex Lock
    pthread_mutex_t lock;

    // Semaphores
    sem_t *dirs;
    sem_t *capacity;
} LocalQueue;

extern LocalQueue *local_queue;

#endif