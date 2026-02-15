#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#ifndef QUEUE_H
#define QUEUE_H

// Maximum # of Directory Entries in SHMQueue
#define MAX_ENTRIES SEM_VALUE_MAX // 2^16

// Foward Declared Shared Memory Queue
// Shared Memory Work Queue 
typedef struct SHMQueue {
    char paths[MAX_ENTRIES][PATH_MAX];
    int  head;       // Beginning of Work Queue 
    int  tail;       // End of Work Queue
    int  size;       // Size of Work Queue

    // Dont think these are necessary tbh
    int done;       // parent set to 1 when no more roots will be enqueued
    int inflight;   // number of directories currently being processed


    // Mutex Lock
    pthread_mutex_t lock;

    // Semaphores
    sem_t *dirs;
    sem_t *capacity;
} SHMQueue;

// Global Pointer
extern SHMQueue *queue;

void
shm_queue_init ( void );

/* void init_shared_queue( void);
void mark_done( SharedQueue *q);

// Blocks if full
void enqueue_blocking(SharedQueue *q, const char *path);

// Returns 1 and fills out if there’s work,
// Returns 0 when queue empty AND done AND inflight==0 (time to exit)
// Blocks when temporarily empty but more work may arrive
int dequeue_start(SharedQueue *q, char out[PATH_MAX]);

// Call after finishing processing a dequeued directory
void finish_dir(SharedQueue *q);
 */
#endif