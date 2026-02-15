<h1 align = 'center'>
    <b>
        macOS Master File Table (WIP)
    </b>
</h1>

This project is meant to serve as a macOS-oriented alternative to the Windows Master File Table (MFT)-based search approach. Windows tools like 'Everything' are able to index the entire system quickly, by reading directly from the MFT. In contrast, macOS mainly relies on Spotlight indexing, which can be slow and unreliable. The aim is to build a similarly fast search solution for macOS by working with system metadata directly. **This project is currently a work in progress.**

## Features

- Built an early prototype for quick file indexing on macOS (cloud storage currently omitted).
- Trying out both multithreading and multiprocessing approaches.
- Comparing different ways to manage task queues (local vs. shared memory).
- Working on incremental updates so the index stays current as the file system changes.

## Dependencies

- macOS
- C compiler (clang/gcc)
- pthreads library
- Make (optional)

## Usage
Since the project is still a WIP, you can compile the program directly from the command-line either using Make or by running the compiler command manually.

```zsh
clang -o MFT mft.c cpu_info.c crawl.c dequeue.c enqueue.c shm_queue/shm_queue_init.c process_init.c fork_processes.c -lpthread
```
After compiling, run the program by providing the directory you want to index.

```zsh
./MFT <directory_to_index>
```