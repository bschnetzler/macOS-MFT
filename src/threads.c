#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include "threads.h"
#include "crawl.h"

#define MAX_THREADS 50
#define ROOT "/Users/billyschnetzler/"

pthread_t threads[MAX_THREADS];
int thread_count = 0;

void
start_threads ( void ) {
    struct dirent *entry;

    DIR *dir = opendir( ROOT );

    while (( entry = readdir( dir ) )) {

        // Shouldnt be needed if crawling from root
        if ( strcmp( entry -> d_name, "." ) == 0 || strcmp( entry -> d_name, ".." ) == 0 ) {
            continue;
        }

        // Need to add functionality that adds files in top level to mft structure
        if ( entry -> d_type != DT_DIR ) {
            continue;
        }

        create_thread( entry );

    }

    closedir( dir );
}

void 
create_thread(struct dirent *entry) {
    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s%s", ROOT, entry->d_name);

    // strdup so each thread gets its own copy of the path
    char *path = strdup(fullpath);
    if (!path) {
        perror("strdup");
        return;
    }

    pthread_create(&threads[thread_count], NULL, thread_function, path);
    thread_count++;
}


void 
join_threads ( void ) {
    for ( int i = 0; i < thread_count; i++ ) {
        pthread_join( threads[i], NULL );
    }
}

void 
*thread_function ( void *path ) {

    char *dirpath = path;
    char filename[64];
    snprintf(filename, sizeof(filename), ".mft.%lu", (unsigned long)pthread_self());
    FILE *out = fopen(filename, "wb");

    // Traverse Directory
    crawl( path, out );

    free( dirpath );
    return EXIT_SUCCESS;
}