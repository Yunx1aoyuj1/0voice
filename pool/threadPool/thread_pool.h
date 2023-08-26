
/*
 * 
 *      暴露给用户使用的接口
 * 
 * 
*/

#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

typedef struct threadpool_s threadpool_t;
typedef void (*handler_ptr)(void*);

#ifdef __cplusplus
extern "C"
{
#endif

threadpool_t* threadpool_create(int thread_count);

void threadpool_terminate(threadpool_t* pool);

int threadpool_post(threadpool_t* pool, handler_ptr func, void* arg);

void threadpool_waitdone(threadpool_t* pool);

#ifdef __cplusplus
}
#endif

#endif