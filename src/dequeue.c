#include <semaphore.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#include "shm_queue/shm_queue.h"


void
dequeue ( SHMQueue *queue, char *dir ) 
{

    //fprintf(stderr, "WAIT_DIRS [%ld]\n", (long)getpid());
    sem_wait(queue->dirs);                       // <- you'll stop here when "hung"
    //fprintf(stderr, "HAVE_DIR  [%ld]\n", (long)getpid());

    pthread_mutex_lock( &queue -> lock );

    (strncpy)( dir, queue -> paths[ queue -> head ], PATH_MAX );

    dir[ PATH_MAX - 1 ] = '\0';

    queue -> head = ( queue -> head + 1 ) % MAX_ENTRIES;

    pthread_mutex_unlock( &queue -> lock );

    sem_post( queue -> capacity );
    
}