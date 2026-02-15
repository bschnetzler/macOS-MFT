// process_init.c

#include <sys/fcntl.h>

#ifndef PROCESS_INIT_H
#define PROCESS_INIT_H

#define STOP_MARKER "__STOP__"

static void wait_until_idle(long idle_ms);

void
process_init ( pid_t );

void 
process_dir ( char *, int );

#endif