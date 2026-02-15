// mft_unbounded.c
#define _DARWIN_C_SOURCE
#include <stdatomic.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// ============================= Config =============================
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define PATH_MAX_STR  PATH_MAX          // fixed slot size
#define CHUNK_SLOTS   16384             // slots per chunk file (16k)
#define CHUNK_DIR     "/tmp/mftq_chunks"
#define SHM_NAME      "/mft_qhdr"
#define SEM_ITEMS     "/mft_items"

// ======================== Small SHM Header ========================
typedef struct {
    _Atomic unsigned long long head;   // next index to consume
    _Atomic unsigned long long tail;   // next index to produce
    _Atomic unsigned long long active; // outstanding dirs (discovered - finished)
    _Atomic int                stop;   // parent sets 1 to ask workers to exit
    sem_t *items;                      // wakeup semaphore (not capacity!)
} qhdr_t;

static qhdr_t *Q = NULL;

// unlink any stale IPC from previous runs (best-effort)
static void ipc_cleanup_start(void) {
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_ITEMS);
    // chunk files cleaned at end; here we ensure dir exists/empty-ish
    mkdir(CHUNK_DIR, 0700);
}

static int qhdr_init(void) {
    int fd = shm_open(SHM_NAME, O_CREAT|O_RDWR, 0600);
    if (fd < 0) return -1;
    if (ftruncate(fd, sizeof(qhdr_t)) != 0) { close(fd); return -1; }
    void *p = mmap(NULL, sizeof(qhdr_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return -1;

    Q = (qhdr_t*)p;
    memset(Q, 0, sizeof(*Q));

    // named semaphore for wakeups only; keep value tiny
    Q->items = sem_open(SEM_ITEMS, O_CREAT, 0600, 0);
    if (Q->items == SEM_FAILED) return -1;

    return 0;
}

// ======================== Chunked Queue I/O =======================
// We map indices -> (chunk_id, slot_id), and store each path into a fixed-size slot.
// We avoid mmap for portability; use pwrite/pread at computed offsets.
// Each thread/process caches its "current" chunk fd.

static inline void chunk_path(char *buf, size_t bufsz, unsigned long long cid) {
    snprintf(buf, bufsz, CHUNK_DIR "/chunk.%llu", cid);
}

static int ensure_chunk_fd(unsigned long long cid, int *fd_io) {
    if (*fd_io >= 0) return 0;
    char p[256]; chunk_path(p, sizeof(p), cid);

    int fd = open(p, O_RDWR | O_CREAT, 0600);
    if (fd < 0) return -1;

    off_t need = (off_t)CHUNK_SLOTS * PATH_MAX_STR;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return -1; }
    if (st.st_size != need) {
        if (ftruncate(fd, need) != 0) { close(fd); return -1; }
    }
    *fd_io = fd;
    return 0;
}

static inline off_t slot_offset(unsigned slot_id) {
    return (off_t)slot_id * PATH_MAX_STR;
}

// Producer: append one path. Returns 0 on success.
static int q_put(const char *path) {
    unsigned long long idx = atomic_fetch_add_explicit(&Q->tail, 1, memory_order_acq_rel);
    unsigned long long h   = atomic_load_explicit(&Q->head, memory_order_acquire);
    unsigned long long cid = idx / CHUNK_SLOTS;
    unsigned            sid = (unsigned)(idx % CHUNK_SLOTS);

    static __thread unsigned long long cur_cid = ~0ull;
    static __thread int pfd = -1;

    if (cid != cur_cid) {
        if (pfd >= 0) { close(pfd); pfd = -1; }
        if (ensure_chunk_fd(cid, &pfd) != 0) return -1;
        cur_cid = cid;
    }

    char buf[PATH_MAX_STR];
    size_t n = strnlen(path, PATH_MAX_STR - 1);
    memcpy(buf, path, n); buf[n] = '\0';

    if (pwrite(pfd, buf, PATH_MAX_STR, slot_offset(sid)) != PATH_MAX_STR) return -1;

    // Wake consumers iff queue transitioned from empty -> non-empty at this put.
    if (idx == h) sem_post(Q->items);
    return 0;
}

// Consumer: try get one path. Returns 1 if got item, 0 if empty.
static int q_get(char out[PATH_MAX_STR]) {
    for (;;) {
        unsigned long long h = atomic_load_explicit(&Q->head, memory_order_relaxed);
        unsigned long long t = atomic_load_explicit(&Q->tail, memory_order_acquire);
        if (h == t) return 0; // empty

        unsigned long long idx = h;
        if (!atomic_compare_exchange_strong_explicit(&Q->head, &h, h+1,
                memory_order_acq_rel, memory_order_relaxed))
            continue; // lost race, retry

        unsigned long long cid = idx / CHUNK_SLOTS;
        unsigned            sid = (unsigned)(idx % CHUNK_SLOTS);

        static __thread unsigned long long cur_cid = ~0ull;
        static __thread int cfd = -1;

        if (cid != cur_cid) {
            if (cfd >= 0) { close(cfd); cfd = -1; }
            if (ensure_chunk_fd(cid, &cfd) != 0) return 0; // rare
            cur_cid = cid;
        }

        char buf[PATH_MAX_STR];
        if (pread(cfd, buf, PATH_MAX_STR, slot_offset(sid)) != PATH_MAX_STR) return 0;
        // Ensure NUL-terminated (defensive)
        buf[PATH_MAX_STR-1] = '\0';
        strncpy(out, buf, PATH_MAX_STR);
        return 1;
    }
}

// ============================ Utils ===============================
static inline double now_sec(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static int should_skip(const char *p, unsigned char d_type) {
    (void)d_type;
    // Skip symlinks and some macOS special paths (tune as you like)
    if (strstr(p, "/Library/CloudStorage/")) return 1;
    if (strstr(p, "/Library/Mobile Documents/")) return 1;
    if (strstr(p, "/CoreSimulator/")) return 1;
    return 0;
}

static void write_shard_line(const char *s) {
    static __thread int sfd = -1;
    if (sfd < 0) {
        char name[64]; snprintf(name, sizeof(name), ".mft.%d", (int)getpid());
        sfd = open(name, O_CREAT|O_TRUNC|O_WRONLY, 0644);
        if (sfd < 0) return;
    }
    size_t n = strlen(s);
    (void)write(sfd, s, n);
    (void)write(sfd, "\n", 1);
}

// ========================== Worker Loop ===========================
static void worker_loop(void) {
    char dirpath[PATH_MAX_STR];

    for (;;) {
        // Try to get global work
        if (!q_get(dirpath)) {
            // If asked to stop, exit
            if (atomic_load_explicit(&Q->stop, memory_order_acquire)) break;
            // Otherwise sleep until producer posts
            sem_wait(Q->items);
            continue;
        }

        // We have taken a directory to process; it's "in flight"
        DIR *d = opendir(dirpath);
        if (!d) {
            atomic_fetch_sub_explicit(&Q->active, 1, memory_order_relaxed);
            continue;
        }

        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0]=='.' && (!e->d_name[1] || (e->d_name[1]=='.' && !e->d_name[2]))) continue;

            char full[PATH_MAX_STR];
            int n = snprintf(full, sizeof(full), "%s/%s", dirpath, e->d_name);
            if (n <= 0 || n >= (int)sizeof(full)) continue;

            struct stat st;
            if (lstat(full, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (should_skip(full, e->d_type)) continue;
                // New directory discovered → count it and enqueue
                atomic_fetch_add_explicit(&Q->active, 1, memory_order_relaxed);
                (void)q_put(full); // unbounded queue -> won't stall (disk-backed)
            } else if (S_ISREG(st.st_mode)) {
                write_shard_line(full);
            }
        }
        closedir(d);

        // Finished this directory
        atomic_fetch_sub_explicit(&Q->active, 1, memory_order_relaxed);
    }
}

// ============================ Main ================================
static void usage(const char *prog){
    fprintf(stderr, "Usage: %s <root_dir>\n", prog);
}

int main(int argc, char **argv) {
    const char *ROOT = (argc > 1) ? argv[1] : "/";
    if (argc < 2) {
        fprintf(stderr, "[info] defaulting to root '/'\n");
    }

    // prep
    ipc_cleanup_start();
    if (qhdr_init() != 0) {
        perror("qhdr_init");
        return 1;
    }
    mkdir(CHUNK_DIR, 0700); // ensure chunk dir exists

    // seed root
    atomic_store_explicit(&Q->active, 1, memory_order_relaxed);
    if (q_put(ROOT) != 0) {
        fprintf(stderr, "failed to enqueue root\n");
        return 1;
    }

    // fork workers (one per online CPU)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;

    printf("macOS Master File Table (unbounded queue)\n");
    printf("Workers: %ld\n", n);

    double t0 = now_sec();

    for (long i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            worker_loop();
            _exit(0);
        }
    }

    // wait for deterministic drain
    for (;;) {
        unsigned long long a = atomic_load_explicit(&Q->active, memory_order_acquire);
        if (a == 0) break;
        struct timespec ts = {0, 100*1000*1000}; // 100ms
        nanosleep(&ts, NULL);
    }

    // request stop and wake sleepers so they can see it
    atomic_store_explicit(&Q->stop, 1, memory_order_release);
    for (long i = 0; i < n; i++) sem_post(Q->items);

    // reap children
    while (wait(NULL) > 0) {}

    double t1 = now_sec();
    printf("Total wall time: %.3f s\n", t1 - t0);

    // best-effort cleanup of IPC (you can keep files to debug)
    sem_close(Q->items);
    sem_unlink(SEM_ITEMS);
    munmap(Q, sizeof(*Q));
    shm_unlink(SHM_NAME);

    // Optional: merge .mft.* into .mft here (left to you)
    // Optional: delete CHUNK_DIR files if you want a clean workspace

    return 0;
}
