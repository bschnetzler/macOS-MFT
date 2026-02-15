#include <semaphore.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "shm_queue/shm_queue.h"


// returns 1 if enqueued, 0 if full, -1 on error
int
enqueue ( SHMQueue *queue, const char *dir ) 
{
    if (sem_trywait(queue->capacity) == -1) {
        if (errno == EAGAIN) return 0;   // queue full
        return -1;                       // some other error
    }

    pthread_mutex_lock(&queue->lock);

    (strncpy)(queue->paths[queue->tail], dir, PATH_MAX);
    queue->paths[queue->tail][PATH_MAX - 1] = '\0';
    queue->tail = (queue->tail + 1) % MAX_ENTRIES;

    pthread_mutex_unlock(&queue->lock);

    sem_post(queue->dirs);
    return 1;
}
