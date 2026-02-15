// shm_queue.c

#include <pthread.h> // #define pthread_mutex_t
#include <semaphore.h> // #define sem_t*

#include <limits.h> // #define PATH_MAX 1024

#include "shm_queue.h"

// Shared Memory Work Queue 
struct SHMQueue {
    char paths[MAX_ENTRIES][PATH_MAX];
    int head;       // Beginning of Work Queue 
    int tail;       // End of Work Queue
    int size;       // Size of Work Queue

    // Dont think these are necessary tbh
    int done;       // parent set to 1 when no more roots will be enqueued
    int inflight;   // number of directories currently being processed


    // Mutex Lock
    pthread_mutex_t lock;

    // Semaphores
    sem_t *dirs;
    sem_t *capacity;
};

SHMQueue *queue = NULL;