// shm_queue_init.c

#include "shm_queue.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>

SHMQueue *queue = NULL;

void 
shm_queue_init() {

    // mmap( void *addr, size_t len, int prot, int flags, int fd, off_t offset );
    // We split arguments into named local variables for readability. 
    // mmap() returns 
    //      - the starting address of the region on success
    //      - MAP_FAILED on error (sets errno)

    // void *addr - Starting address for the Memory Mapping
    //     - NULL : Kernel picks an address that avoids overlap
    //     - MAP_FIXED : Force Memory Mapping @ void *addr (overwrites existing mappings)
    //     - Without MAP_FIXED : Attempt Memory Mapping within [ void *addr, void *addr + size_t len ),
    //                           and chooses an alternative rather than overwriting any existing mappings.
    void *addr = (void *)NULL;

    // size_t lent - Bytes to map - Must be >0.
    //     - If not a page multiple, the kernel rounds up to the nearest page (extra bytes zeroed).
    size_t len = (size_t)sizeof( SHMQueue );

    // int prot - Memory Protection Settings
    int prot = (int)PROT_READ | (int)PROT_WRITE;
    // PROT_NONE  -- 0x00 (00000000)    // PROT_READ | PROT_WRITE 
    // PROT_READ  -- 0x01 (00000001)    //      0x01 | 0x02
    // PROT_WRITE -- 0x02 (00000010)    //  00000001 | 00000010
    // PROT_EXEC  -- 0x04 (00000100)    //       [00000011]

    // int flags - Object type, Mapping options, and whether changes are Shared or Private.
    int flags = (int)MAP_SHARED | (int)MAP_ANONYMOUS; 
    // MAP_FILE         -- 0x0000 (0000000000000000)    //       MAP_SHARED | MAP_ANONYMOUS
    // MAP_SHARED       -- 0x0001 (0000000000000001)    //           0x0001 | 0x1000
    // MAP_PRIVATE      -- 0x0002 (0000000000000010)    // 0000000000000001 | 0001000000000000
    // -----            --        (............X...)    //          [0001000000000001]
    // MAP_FIXED        -- 0x0010 (0000000000010000)
    // -----            --        (..........X.....)
    // -----            --        (.........X......)
    // -----            --        (........X.......)
    // -----            --        (.......X........)
    // MAP_HASSEMAPHORE -- 0x0200 (0000001000000000)
    // MAP_NOCACHE      -- 0x0400 (0000010000000000)
    // MAP_JIT          -- 0x0800 (0000100000000000)
    // MAP_ANONYMOUS    -- 0x1000 (0001000000000000)
    // MAP_ANON         -- 0x1000 (0001000000000000)
    // -----            --        (..X.............)
    // -----            --        (.X..............)
    // MAP_32BIT        -- 0x8000 (1000000000000000)

    // File descriptor for the Mapped File
    //   - must be a valid open()’d fd, mapping reflects file contents starting at `offset`
    //   - For MAP_ANONYMOUS: pass -1 (fd is ignored, memory is zero-initialized)
    //   - On macOS, can also be used to pass Mach VM flags when MAP_ANONYMOUS is used
    int fd = (int)-1;

    // Offset into the Mapped File
    //   - Must be a multiple of the system page size (page-aligned)
    //   - Mapping begeins at file[offset]
    //   - For MAP_ANONYMOUS: ignored (mapping starts at the beginning of the new region)
    //   - Combined with `addr`, defines the starting virtual address if MAP_FIXED is used
    off_t offset = (off_t)0;

    // Map Shared Memory Region for SHMQueue
    queue = (SHMQueue *)
        mmap(
            ( void * ) addr, 
            ( size_t ) len,
            ( int )    prot,
            ( int )    flags,
            ( int )    fd,
            ( off_t )  offset
        );
    
    // MAP_FAILED returned on mmap() error
    // TODO : Logger
    if ( queue == MAP_FAILED ) { 
        //! DEBUG !//
        perror( "mmap : " );
        exit( EXIT_FAILURE ); 
    }

    // ???
    memset(
        ( void * ) queue, 
        ( int ) 0, 
        ( size_t ) sizeof( *( SHMQueue * ) queue )
    );


    // pthread_mutexattr_t is an opaque type, and as such, 
    // we use provided functions for modifying it.
    pthread_mutexattr_t mutexattr; 
    // Initialize [mutexattr] with DEFAULT Mutex Attributes
    pthread_mutexattr_init( &mutexattr );
    // Sets [process-shared] mutex attribute 
    //    Options : 
    //          PTHREAD_PROCESS_PRIVATE (2) - Can only be used in threads of the process that created it
    //          PTHREAD_PROCESS_SHARED (1) - Can be used in other processes threads as long as they share
    //                                       access to the memory where the mutex resides
    pthread_mutexattr_setpshared( &mutexattr, PTHREAD_PROCESS_SHARED );
    // Creates new Mutex (Mutual Exclusion) w/ attributes [pthread_mutexattr_t] and puts it in [pthread_mutex_t]
    // Returns 0 on Success, otherwise an error # is returned
    pthread_mutex_init( &(( SHMQueue * ) queue ) -> lock , &mutexattr );
    // TODO : Logging / Debug / Error Handling

    // Since sem_init() is not supported on macOS (not implemented), we must use
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
        const char *name   = ( const char * ) "/mftq.dirs";
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
        const char *name   = ( const char * ) "/mftq.capacity";
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