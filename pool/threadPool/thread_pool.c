#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "thread_pool.h"
#include "spinlock.h"


/**
 * 
 * shell: gcc thrd_pool.c -c -fPIC
 * shell: gcc -shared thrd_pool.o -o libthrd_pool.so -I./ -L./ -lpthread
 * usage: include thrd_pool.h & link libthrd_pool.so
 * 
*/

typedef struct spinlock spinlock_t;

//任务结构
typedef struct task_s
{
    //指向下一个任务
    void* next;
    //函数指针
    handler_ptr func;
    //func的参数
    void* arg;
}task_t;

//调度线程池中消费者线程  到底由谁来管理
//职责划分清楚
//阻塞类型的队列
//谁来取任务，如果此时队列为空，谁应该阻塞休眠
//任务队列
typedef struct task_queue_s
{
    // 任务头指针.
    void* head;
    //任务尾指针的指针 
    void** tail;
    // 当前是否下是阻塞的，谁来取任务，如果此时队列为空，谁应该阻塞休眠
    int block;
    // 自旋锁   -- 多线程访问队列
    spinlock_t lock;
    // 互斥锁
    pthread_mutex_t mutex;
    // 条件变量
    pthread_cond_t cond;
}task_queue_t;

//线程池数据结构
struct threadpool_s
{
    //任务队列
    task_queue_t* task_queue;
    //标记，是否让整个线程池退出
    atomic_int quit;
    // 线程数量
    int thread_count;
    //数组  --  维持线程，创建的时候会把它写入到threads里面
    pthread_t* threads;
};
 
/*
    使用static 修饰函数，避免名字冲突，只在.c中可见
*/

//创建对象的时候，回滚式的，创建的时候要有销毁
static task_queue_t* 
__taskqueue_create()
{
    //sizeof(*queue)  是一个表达式，编译器会帮我们计算他的大小，换成task_queue_t也一样
    task_queue_t* queue = (task_queue_t*)malloc(sizeof(*queue));
    if (!queue)
    {
        return NULL;
    }

    //这里使用ret接收返回值，而不是直接if(pthread_mutex_init()),方便后期调试
    int ret;
    ret = pthread_mutex_init(&queue->mutex, NULL);
    if (ret == 0)
    {
        ret = pthread_mutex_init(&queue->mutex, NULL);
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

//添加任务      --生产者线程调用
static inline void 
__add_task(task_queue_t* queue, void* task)
{
    void** link = (void**)task;     
    *link = NULL;
    //C里面的惯用写法：首先void* 编译器不知道指向的类型大小，转换成void**类型的时指向的就是一个void*类型的地址8（或者4）字节大小，task对应内存的开始sizeof(void*)是next，所以相当于task->next = NULL;
    spinlock_lock(&queue->lock);
    *queue->tail = link;        //*queue->tail 是next指针
    queue->tail = link;         //尾指针指向最后一个数据结构的地址      这两行的作用的在链表末尾插入数据
    spinlock_unlock(&queue->lock);
    pthread_cond_signal(&queue->cond);
}

//取出任务
static inline void* 
__pop_task(task_queue_t* queue)
{
    spinlock_lock(&queue->lock);
    if (queue->head == NULL)
    {
        spinlock_unlock(&queue->lock);
        return NULL;
    }
    
    task_t task;
    task = queue->head;
    queue->head = task.next;
    if (queue->head == NULL)
    {
        queue->tail = &queue->head;
    }
    spinlock_unlock(&queue->lock);

    return task;
}

//      --消费者线程调用
static inline void* 
__get_task(task_queue_t* queue)
{
    task_t *task;
    while ((task = __pop_task(queue)) == NULL)
    {
        pthread_mutex_lock(&queue->mutex);
        if (queue->block == 0)
        {
            //break;
            pthread_mutex_unlock(&queue->mutex);
            return NULL;            
        }
        /*
            1、先unlock(&mutex);
            2、再cond休眠，条件变量休眠     
            3、生产者产生任务，signal唤醒（ps：有可能会虚假唤醒，所以需要while循环确保task != NULL;）
            4、再cond唤醒
            5、加上 lock(&mutex);
        */
        pthread_cond_wait(&queue->cond, &queue->mutex);        //条件变量唤醒的锁
        pthread_mutex_unlock(&queue->mutex);
    }
    return task;
}

static void 
__taskqueue_destroy(task_queue_t* queue)
{
    task_t* task;
    while ((task = __pop_task(queue)))  //任务的生命周期由 threadpool 管理
    {
        free(task);
    }
    spinlock_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

//消费者线程
static void* 
__threadpool_worker(void* arg)
{
    threadpool_t* pool = (threadpool_t*)arg;
    task_t* task;
    void *ctx;

    //原子操作的作用在这里
    while (atomic_load(&pool->quit) == 0)
    {
        task = (task_t*)__get_task(pool->task_queue);
        if (!task)
        {
            break;
        }

        handler_ptr func = task.func;
        ctx = task->arg;
        free(task);
        func(ctx);
    }

    return NULL;
}

static void 
__threads_terminate(threadpool_t* pool)
{
    atomic_store(&pool->quit, 1);
    __nonblock(pool->task_queue);
    int i;
    for (i=0; i<pool->thrd_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

//创建消费者线程
static int 
__threads_create(threadpool_t* pool, size_t thread_count)
{
    pthread_attr_t attr;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret == 0)
    {
         pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*thread_count);
         if (!pool->threads)
         {
            int i = 0;
            for (; i < thread_count; i++)
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
            __threads_terminate(pool);
            free(pool->threads);            
         }
        ret = -1;
    }
    
    return ret;    
}

void 
threadpool_terminate(threadpool_t* pool)
{
    atomic_store(&pool->quit, 1);
    __nonblock(pool->task_queue);
}

threadpool_t* 
threadpool_create(int thread_count)
{
    threadpool_t* pool;

    pool = (threadpool_t*)malloc(sizeof(*pool));
    if(!pool)   return NULL;

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

int 
threadpool_post(threadpool_t* pool, handler_ptr func, void* arg)
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
        pthread_join(pool->thread_count[i], NULL);
    }

    __taskqueue_destroy(pool->task_queue);    
    free(pool->threads);
    free(pool);
}