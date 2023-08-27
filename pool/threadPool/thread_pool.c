/**
 * 
 * threadpool.c     
 * 接口代码实现
 * 
*/


#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include "thread_pool.h"
#include "spinlock.h"

/**
 * 
 * shell: gcc thread_pool.c -c -fPIC
 * shell: gcc -shared thread_pool.o -o libthread_pool.so -I./ -L./ -lpthread
 * usage: include thread_pool.h & link libthread_pool.so
 * 
*/


typedef struct spinlock spinlock_t;

//任务结构
typedef struct task_s
{
    //指向下一个任务
    void* next;
    //函数指针
    handler_Ptr func;
    //func参数
    void* arg;
}task_t;

//任务队列
typedef struct task_queue_s
{
    //任务头指针.
    void* head;
    //任务尾指针    --注意这里的void** 二级指针
    void** tail;
    //当前是否是阻塞的
    int block;
    //自旋锁
    spinlock_t lock;
    //互斥锁
    pthread_mutex_t mutex;
    //条件变量  互斥锁的条件变量
    pthread_cond_t cond;
}task_queue_t;

//线程池数据结构
struct threadpool_s
{
    //任务队列
    task_queue_t* task_queue;
    //标记      -- 是否让整个线程池退出
    atomic_int quit;
    //线程数量
    int thread_count;
    //数组指针  -- 维持线程
    pthread_t* threads;
};

//创建任务队列
static task_queue_t* 
__taskqueue_create()
{
    task_queue_t* queue;
    queue = (task_queue_t*)malloc(sizeof(*queue));
    if (!queue)
    {
        return NULL;
    }

    int ret;
    ret = pthread_mutex_init(&queue->mutex, NULL);
    if (ret == 0)
    {
        ret = pthread_cond_init(&queue->cond, NULL);
        if (ret == 0)
        {
            spinlock_init(&queue->lock);
            queue->head = NULL;
            queue->tail = &queue->head;
            queue->block = 1;
            return queue;
        }
        pthread_cond_destroy(&queue->cond);
    }
    pthread_mutex_destroy(&queue->mutex);
    return NULL;
}

static void
__nonblock(task_queue_t* queue)
{
    pthread_mutex_lock(&queue->mutex);
    queue->block = 0;
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->cond);
}

static inline void
__add_task(task_queue_t* queue, task_t* task)
{
    void** link = (void**)task;
    *link = NULL;       //相当于task->next = NULL;
    spinlock_lock(&queue->lock);
    *queue->tail = link;
    queue->tail = link;     // 链表尾插
    spinlock_unlock(&queue->lock);
    pthread_cond_signal(&queue->cond);
}

static inline void*     //注意这里的void*  
__pop_task(task_queue_t* queue)
{
    spinlock_lock(&queue->lock);
    if (queue->head == NULL)
    {
        spinlock_unlock(&queue->lock);
        return NULL;
    }

    task_t* task;
    task = queue->head;
    queue->head = task->next;   //删除链表的头数据
    if (queue->head == NULL)
    {
        queue->tail = &queue->head;
    }
    
    spinlock_unlock(&queue->lock);
    return task;
}

static void
__taskqueue_destroy(task_queue_t* queue)
{
    task_t* task;
    while ((task = __pop_task(queue)))
    {
        free(task);
    }

    spinlock_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

static inline void * 
__get_task(task_queue_t* queue)
{
    spinlock_lock(&queue->lock);
    if (queue->head == NULL)
    {
        spinlock_unlock(&queue->lock);
        return NULL;
    }

    task_t* task;
    task = queue->head;
    queue->head = task->next;
    if (queue->head == NULL)
    {
        queue->tail = &queue->head;
    }
    spinlock_unlock(&queue->lock);
    return task;
}

static void *
__threadpool_worker(void* arg)
{
     threadpool_t* pool = (threadpool_t*)arg;
     task_t* task;
     void* ctx;

     while (atomic_load(&pool->quit) == 0)
     {
        task = (task_t*)__get_task(pool->task_queue);
        if (!task)
        {
            break;
        }

        handler_Ptr func = task->func;
        ctx = task->arg;
        free(task);
        func(ctx);
     }

    return NULL;     
}
static void
__thread_terminate(threadpool_t* pool)
{
    atomic_store(&pool->quit, 1);
    __nonblock(pool->task_queue);
    int i;
    for (i = 0; i < pool->thread_count; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }
}


static int
__threads_create(threadpool_t* pool, size_t thread_count)
{
    pthread_attr_t attr;
    int ret;

    ret = pthread_attr_init(&attr);    
    if (ret == 0)
    {
        pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
        if (pool->threads)
        {
            int i = 0;
            for (i = 0; i < thread_count; i++)
            {
                if (pthread_create(&pool->threads[i], &attr, __threadpool_worker, pool) != 0)
                {
                    break;
                }
            }
            pool->thread_count = i;
            pthread_attr_destroy(&attr);
            if (i == thread_count)
            {
                return 0;
            }
            __thread_terminate(pool);
            free(pool->threads);
        }
        ret = -1;        
    }
    
    return -1;
}

threadpool_t* 
threadpool_create(int thread_count)
{
    threadpool_t* pool;

    pool = (threadpool_t*)malloc(sizeof(*pool));    //这里使用sizeof(*pool) == sizeof(threadpool_t);
    if (!pool)
    {
        return NULL;
    }
        
    task_queue_t* queue = __taskqueue_create();
    if (queue)
    {
        pool->task_queue = queue;
        atomic_init(&pool->quit, 0);
        if (__threads_create(pool, thread_count) == 0)
        {
            return pool;
        }

        __taskqueue_destroy(pool->task_queue);
    }

    free(pool);    
    
    return NULL;
}


void 
threadpool_terminate(threadpool_t* pool)
{
    atomic_store(&pool->quit, 1);
    __nonblock(pool->task_queue);
}

int 
threadpool_post(threadpool_t* pool, handler_Ptr func, void* arg)
{
    if (atomic_load(&pool->quit) == 1)
    {
        return -1;
    }

    task_t* task = (task_t*)malloc(sizeof(task_t));
    if (!task)
    {
        return -1;
    }

    task->func = func;
    task->arg = arg;
    __add_task(pool->task_queue, task);

    return 0;
}

void 
threadpool_waitdone(threadpool_t* pool)
{
    int i;
    for (i = 0; i < pool->thread_count; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }
    __taskqueue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool);
}


