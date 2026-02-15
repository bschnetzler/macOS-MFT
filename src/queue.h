#include <limits.h>
#include <pthread.h>

#ifndef QUEUE_H
#define QUEUE_H

#define MAX_ENTRIES 131072  // big ring; tweak as needed


typedef struct SHMQueue SHMQueue;

typedef struct SharedQueue {
    char paths[MAX_ENTRIES][PATH_MAX];
    int head;       // pop index
    int tail;       // push index
    int count;      // items in queue
    int done;       // parent set to 1 when no more roots will be enqueued
    int inflight;   // number of directories currently being processed

    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} SharedQueue;

extern SharedQueue *queue;

void init_shared_queue(void);
void mark_done( SharedQueue *q);

// Blocks if full
void enqueue_blocking(SharedQueue *q, const char *path);

// Returns 1 and fills out if there’s work,
// Returns 0 when queue empty AND done AND inflight==0 (time to exit)
// Blocks when temporarily empty but more work may arrive
int dequeue_start(SharedQueue *q, char out[PATH_MAX]);

// Call after finishing processing a dequeued directory
void finish_dir(SharedQueue *q);

#endif