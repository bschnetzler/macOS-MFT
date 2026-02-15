//! THIS MAIN FILE IS FAIRLY A MESS ATM SINCE I WAS STILL TESTING WITH DIFFERENT DATA STRUCTURES !//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fts.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "crawl.h"
#include "file_entry.h"
#include "threads.h"
#include "shm_queue/shm_queue.h"
#include "fork_processes.h"
#include "cpu_info.h"
#include "enqueue.h"
#include "dequeue.h"

#define UF_RESTRICTED 0x00080000

#define MFT_FILE ".mft"

double seconds_since( clock_t start ) {
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

#include <sys/time.h>

double time_diff(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + 
           (end.tv_usec - start.tv_usec) / 1000000.0;
}


#include <dirent.h>

/* int main() {
    printf( "macOS Master File Table\n" );

    clock_t total_start = clock();
    clock_t write_time = 0;

    FILE *mft_file = NULL;

    mft_file = fopen( MFT_FILE, "wb" );

    struct timeval start, end;

    gettimeofday(&start, NULL);
    // crawl(ROOT, mft_file);


    start_threads();

    join_threads();
    gettimeofday(&end, NULL);

    printf("Elapsed Time: %.2f seconds\n", time_diff(start, end));



    fclose( mft_file );
    printf("done.\n");
    // printf("Time Reading :   %.2fs\n", seconds_since(thread_time) / CLOCKS_PER_SEC );
    // printf("Write Time :     %.2fs\n", (double)write_time / CLOCKS_PER_SEC );
    return 0;


} */

// BSD blah blah versioning
// 
//  struct stat {
//      dev_t           st_dev;           /* ID of device containing file */
//      mode_t          st_mode;          /* Mode of file */
//      nlink_t         st_nlink;         /* Number of hard links */
//      ino_t           st_ino;           /* File serial number */
//      uid_t           st_uid;           /* User ID of the file */
//      gid_t           st_gid;           /* Group ID of the file */
//      dev_t           st_rdev;          /* Device ID */
//      struct timespec st_atimespec;     /* time of last access */
//      struct timespec st_mtimespec;     /* time of last data modification */
//      struct timespec st_ctimespec;     /* time of last status change */
//      struct timespec st_birthtimespec; /* time of file creation(birth) */
//      off_t           st_size;          /* file size, in bytes */
//      blkcnt_t        st_blocks;        /* blocks allocated for file */
//      blksize_t       st_blksize;       /* optimal blocksize for I/O */
//      uint32_t        st_flags;         /* user defined flags for file */
//      uint32_t        st_gen;           /* file generation number */
//      int32_t         st_lspare;        /* RESERVED: DO NOT USE! */
//      int64_t         st_qspare[2];     /* RESERVED: DO NOT USE! */
//  };
//

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include "file_entry.h"
#include "queue.h"
#include "process_init.h"


#include <stdarg.h>
#include <sys/time.h>


static double wall_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void wait_until_idle(long idle_ms) {
    size_t last_h = queue->head, last_t = queue->tail;
    double idle_start = now_sec();
    const struct timespec tick = { .tv_sec=0, .tv_nsec=100*1000*1000 }; // 100ms

    for (;;) {
        // avoid sem_getvalue (deprecated on macOS); just check indices
        size_t h = queue->head, t = queue->tail;
        int moving = (h != last_h) || (t != last_t);
        last_h = h; last_t = t;

        if (!moving) {
            if ((now_sec() - idle_start) * 1000.0 >= idle_ms) return; // idle long enough
        } else {
            idle_start = now_sec(); // progress seen
        }
        nanosleep(&tick, NULL);
    }
}

int
main ( int argc, char *argv[] ) 
{

    char *root = argv[1];

    //! DEBUG !//
    printf( "macOS Master File Table \n");


    double t0 = wall_now();
    
    // Shared Memory Queue Initializer
    shm_queue_init();

    fork_processes( ( long ) configured_processors() );

    enqueue( queue, root );

    int status = 0;
    pid_t wpid;

    //! BLAH !//
    wait_until_idle( 1000 );

    long n = (long)configured_processors();

    //! BLAH !//
    for (long i = 0; i < n; i++) enqueue(queue, STOP_MARKER);

    while ((wpid = wait(&status)) > 0);


    //! DEBUG !//
    // Program Runtime
    double t1 = wall_now();
    printf("Total wall time: %.3f s\n", t1 - t0);
    double runtime = (double) ( clock() ) / CLOCKS_PER_SEC;
    printf( "Program Runtime : %f \n", runtime );

}