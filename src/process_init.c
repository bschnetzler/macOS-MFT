#include <sys/fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include "dequeue.h"
#include "enqueue.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "process_init.h"
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>

#define NUM_THREADS 4

static pthread_t worker_threads[NUM_THREADS];
static volatile int shutdown_flag = 0;

typedef struct {
    int thread_id;
    int output_fd;
    volatile int *shutdown_flag;
} ThreadData;

static ThreadData thread_data[NUM_THREADS];

static int should_skip(const char *p, unsigned char d_type) {
    if (d_type == DT_LNK) return 1;
    if (strstr(p, "/Library/CloudStorage/")) return 1;
    if (strstr(p, "/Library/Mobile Documents/")) return 1;
    if (!strncmp(p, "/Volumes/GoogleDrive", 20)) return 1;
    if (strstr(p, "/CoreSimulator/")) return 1;
    return 0;
}

// Simple enqueue with minimal overhead
static void simple_enqueue(const char *path) {
    if (sem_trywait(queue->capacity) == 0) {
        pthread_mutex_lock(&queue->lock);
        strcpy(queue->paths[queue->tail], path);
        queue->tail = (queue->tail + 1) % MAX_ENTRIES;
        pthread_mutex_unlock(&queue->lock);
        sem_post(queue->dirs);
    }
}

void *worker_thread(void *arg) {
    ThreadData *data = (ThreadData*)arg;
    char dir[PATH_MAX];
    
    while (!*(data->shutdown_flag)) {
        dequeue(queue, dir);
        
        if (strcmp(dir, STOP_MARKER) == 0) {
            enqueue(queue, STOP_MARKER);
            *(data->shutdown_flag) = 1;
            break;
        }
        
        process_dir(dir, data->output_fd);
    }
    
    return NULL;
}


void 
process_init( pid_t pid ) 
{


    char filename[64];
    ( snprintf )( 
        ( char * ) filename, 
        ( size_t ) sizeof( filename ), 
        ( const char * ) ".mft.%i", 
        ( pid_t ) pid 
    );

    int file_descriptor = ( int ) open( 
        ( const char * ) filename, 
        ( int ) O_CREAT | O_TRUNC | O_WRONLY, 
        ( int ) 0600
    );
    
    for (;;) {

        char dir[PATH_MAX];

        dequeue( queue, dir );
        
        if (strcmp(dir, STOP_MARKER) == 0) {
            enqueue(queue, STOP_MARKER);
            break;
        }
        
        process_dir(dir, file_descriptor);
    }

    
    close(file_descriptor);
    _exit(0);
}

void process_dir(char *dir, int fd) {
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *e;
    char path[PATH_MAX];

    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;

        int n;
        if (strcmp(dir, "/") == 0)
            n = snprintf(path, sizeof(path), "/%s", e->d_name);
        else
            n = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (n <= 0 || n >= (int)sizeof(path)) continue;

        while (path[0] == '/' && path[1] == '/') memmove(path, path+1, strlen(path));

        if (should_skip(path, e->d_type)) continue;


        if (e->d_type == DT_DIR) {
            if ( (enqueue(queue, path)) == 0 ) {
                process_dir( path, fd );
            }
        } else if (e->d_type == DT_UNKNOWN) {
            struct stat st;
            if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                if ( (enqueue(queue, path)) == 0) {
                    process_dir( path, fd );
                }
            }
        }

        struct stat st;
        if (lstat(path, &st) != 0) continue;
        if (S_ISREG(st.st_mode)) {
            int m = snprintf(path, sizeof(path), "%s\n", path);
            if (m > 0 && m < (int)sizeof(path)) {
                (void)write(fd, path, (size_t)m);
            }
        }
    }
    
    closedir(d);
}