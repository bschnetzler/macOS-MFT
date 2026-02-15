#include <sys/types.h>
#include <unistd.h>
#include "process_init.h"

void 
fork_processes ( long processors ) 
{
    for ( int wid = 0; wid < processors; wid++ ) {

        pid_t pid = ( pid_t ) fork();

        if ( ( pid_t ) pid == ( int ) 0 ) {

            // Currently passes Worker ID and Process ID for unique file names, but
            // will probably switch to Process ID and Thread ID when we start
            // using Threads in each Process
            process_init( ( pid_t ) getpid() );

        }
    }
}