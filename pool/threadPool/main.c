
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "thread_pool.h"

/**
 * 测试
 * shell: gcc -Wl,-rpath=./ main.c -o main -I./ -L./ -lthread_pool -lpthread
 */

int done = 0;

pthread_mutex_t lock;

void do_task(void *arg) {
    threadpool_t *pool = (threadpool_t*)arg;
    pthread_mutex_lock(&lock);
    done++;
    printf("doing %d task\n", done);
    pthread_mutex_unlock(&lock);
    if (done >= 1000) {
        threadpool_terminate(pool);
    }
}

void test_threadpool_basic() {
    int threads = 8;
    pthread_mutex_init(&lock, NULL);
    threadpool_t *pool = threadpool_create(threads);
    if (pool == NULL) {
        perror("thread pool create error!\n");
        exit(-1);
    }

    while (threadpool_post(pool, &do_task, pool) == 0) {
    }

    threadpool_waitdone(pool);
    pthread_mutex_destroy(&lock);
}

int main(int argc, char **argv) {
    test_threadpool_basic();
    return 0;
}
