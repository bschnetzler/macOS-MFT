#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define BUFFER_SIZE 8192
static char buffer[BUFFER_SIZE];
static size_t buf_used = 0;

typedef struct {
    dev_t dev;
    ino_t ino;
} inode_key;

#define MAX_SEEN 100000
inode_key seen[MAX_SEEN];
size_t seen_len = 0;

int already_seen(dev_t dev, ino_t ino) {
    for (size_t i = 0; i < seen_len; i++) {
        if (seen[i].dev == dev && seen[i].ino == ino)
            return 1;
    }
    if (seen_len < MAX_SEEN) {
        seen[seen_len++] = (inode_key){ dev, ino };
    }
    return 0;
}

void 
crawl( char *path, FILE *out ) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        // Skip . and ..
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' || 
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        // Build full path
        char fullpath[1024];
        int len = snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        if (len <= 0 || len >= 1024) continue;

        fprintf(stderr, "%s\n", fullpath);


        if (entry -> d_type == DT_LNK ) {
            continue;
        }

        if (strstr(fullpath, "CloudStorage") != NULL) {
            continue;
        }

        // Recurse if it's a directory (and we actually know it is one)
        if (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) {
            struct stat st;
            if (lstat(fullpath, &st) == 0) {
                if (!already_seen(st.st_dev, st.st_ino)) {
                    crawl(fullpath, out);
                }
            }
        }

        // Always write the path (file or dir, doesn't matter)
        if (len + 1 + buf_used >= BUFFER_SIZE) {
            fwrite(buffer, 1, buf_used, out);
            buf_used = 0;
        }
        memcpy(buffer + buf_used, fullpath, len + 1);
        buf_used += len + 1;




        // If d_type is DT_UNKNOWN (common on some FS), we don't recurse.
        // You can enable recursion for unknowns if you're feeling bold.
    }

    if (buf_used > 0) {
        fwrite(buffer, 1, buf_used, out);
        buf_used = 0;
    }


    closedir(dir);
}