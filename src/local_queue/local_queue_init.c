

void
local_queue_init () 
{

}

// shm_queue_init.c

#include "../shm_queue/shm_queue.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include "local_queue.h"

LocalQueue *local_queue = NULL;

void 
local_queue_init() {

    local_queue -> head = 0;
    local_queue -> tail = 0;
    local_queue -> size = 0;

    // Creates new Mutex (Mutual Exclusion) and puts it in [pthread_mutex_t]
    // Returns 0 on Success, otherwise and error # is returned
    pthread_mutex_init( &(( LocalQueue * ) local_queue ) -> lock , NULL );
    // TODO : Logging / Debug / Error Handling

    // Since sem_init() is not supported on macOS, not implemented, we must use
    // sem_open, sem_close, and sem_unlink instead of sem_init and sem_destroy

    // sem_t*
    // sem_open ( const char *name, int oflag, ... )
    //
    // ... -> mode_t mode, unsigned int value
    //
    // const char *name 
    //   Semaphore Name
    // int oflag
    //   options : 
    //      O_CREAT // Create Semaphore if it does not exist
    //      O_EXCL  // Error if Create & Semaphore exists
    //
    // mode_t mode ~> permission bitmask (octal)
    // Defined in sys/stat.h
    // Bitmask breakdown :
    //   Special Mode Bits :
    //     S_ISUID (04000) - set-user-id
    //     S_ISGID (02000) - set-group-id
    //     S_ISVTX (01000) - sticky bit
    //   Permission Bits :
    //     USER (Owner) :
    //       S_IRWXU (0700) - RWX mask for users
    //       S_IRUSR (0400) - read
    //       S_IWUSR (0200) - write
    //       S_IXUSR (0100) - execute/search
    //     GROUP : 
    //       S_IRWXG (0070) - RWX mask for groups
    //       S_IRGRP (0040) - read
    //       S_IWGRP (0020) - write
    //       S_IXGRP (0010) - execute/search
    //     OTHERS :
    //       S_IRWXO (0007) - RWX mask for others
    //       S_IROTH (0004) - read
    //       S_IWOTH (0002) - write
    //       S_IXOTH (0001) - execute/search
    // Example :
    //  0[600] --rwxr-x---
    //    User  = 0600 (0400 + 0200 + 0000) - read, write, -----
    //    Group = 0000 (0000 + 0000 + 0000) - ----, -----, -----
    //    Other = 0000 (0000 + 0000 + 0000) - ----, -----, ----- 
    //  mode = ( 
    //   S_IRUSR | S_IWUSR | ------- | 
    //   ------- | ------- | ------- | 
    //   ------- | ------- | -------
    //  )

    // unsigned int value ~> Initial value of semaphore when created <= SEM_VALUE_MAX 

    //
    {
        const char *name   = ( const char * ) "/mftlq.dirs";
        int oflag          = ( int ) O_CREAT | O_EXCL;
        mode_t mode        = ( mode_t ) S_IRUSR | S_IWUSR;
        unsigned int value = ( unsigned int ) 0;

        queue -> dirs = ( sem_t * )
            sem_open( 
                ( const char * ) name,
                ( int )          oflag,
                ( mode_t )       mode,
                ( unsigned int ) value
            );

        if ( queue -> dirs == SEM_FAILED ) {
            // TODO : Logging / Debug
            exit( EXIT_FAILURE );
        }

        if ( ( int ) sem_unlink( ( const char * ) name ) != 0 ) {
            // TODO : Logging / Debug
            exit( EXIT_FAILURE );
        }
    }

    //
    {
        const char *name   = ( const char * ) "/mftlq.capacity";
        int oflag          = ( int ) O_CREAT | O_EXCL;
        mode_t mode        = ( mode_t ) S_IRUSR | S_IWUSR;
        unsigned int value = ( unsigned int ) MAX_ENTRIES;

        queue -> capacity = ( sem_t * )
            sem_open(
                ( const char * ) name,
                ( int )          oflag,
                ( mode_t )       mode,
                ( unsigned int ) value
            );

        if ( queue -> capacity == SEM_FAILED ) {
            // TODO : Logging / Debug
            exit( EXIT_FAILURE );
        }

        if ( ( int ) sem_unlink( ( const char * ) name ) != 0 ) {
            // TODO : Logging / Debug
            exit( EXIT_FAILURE );
        }

    }


    // Returns SEM_FAILED on error and sets errno accordingly
    // 

}