/**
 * 
 * threadPool.h
 * 
 * 暴露给用户的接口
 * 
 * 
*/


#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H


typedef struct threadpool_s threadpool_t;

typedef void (*handler_Ptr)(void* arg);

#ifdef __cplusplus
extern "C"
{
#endif  //__cplusplus


//用户接口
threadpool_t* threadpool_create(int thread_count);

void threadpool_terminate(threadpool_t* pool);

int threadpool_post(threadpool_t* pool, handler_Ptr func, void* arg);

void threadpool_waitdone(threadpool_t* pool);


#ifdef __cplusplus
}
#endif  //__cplusplus

#endif  //_THREAD_POOL_H