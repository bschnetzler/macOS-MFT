
#ifndef CRAWL_H
#define CRAWL_H

void
crawl ( char *path, FILE *out );

int
already_seen ( dev_t dev, ino_t ino );

#endif // CRAWL_H