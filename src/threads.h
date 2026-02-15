#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <stdio.h>

void
start_threads ( void );

void
create_thread ( struct dirent *entry );

void
join_threads ( void );

void
*thread_function ( void *path );