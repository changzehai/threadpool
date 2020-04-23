/*****************************************************************************/
/* 文件名:    thread_pool.c                                                  */
/* 描  述:    实现线程池                                                     */
/* 创  建:    2020-04-12 changzehai                                          */
/* 更  新:    无                                                             */
/* Copyright 1998 - 2020 CZH. All Rights Reserved                            */
/*****************************************************************************/
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "thread_pool.h"




/*-----------------------------------*/
/* 数据结构定义                       */
/*-----------------------------------*/
/* 任务数据结构 */
typedef struct _task_t_
{
    void *(*task_process)(void *arg);
    void *arg;
    struct _task_t_ *next;
} task_t;

/* 任务队列数据结构 */
typedef struct _task_queue_t_
{
    task_t *head; /* 任务队列头 */
    task_t *tail; /* 任务队列尾 */
} task_queue_t;

/* 线程池数据结构定义 */
typedef struct _thread_pool_t_
{
    int max_thread_num;
    int shutdown;
    pthread_t *threads;
    task_queue_t *task_queue;
    pthread_mutex_t task_queue_lock;
    pthread_cond_t task_queue_ready;

} thread_pool_t;

/*-----------------------------------*/
/* 变量定义                          */
/*-----------------------------------*/
static thread_pool_t  *gs_thread_pool = NULL;

/*-----------------------------------*/
/* 内部函数声明                       */ 
/*-----------------------------------*/
static int thread_pool_task_queue_init(task_queue_t **task_queue);
static int thread_pool_task_queue_is_empty(task_queue_t *task_queue);
static void thread_pool_task_queue_push(task_queue_t *task_queue, task_t *task);
static task_t *thread_pool_task_queue_pop(task_queue_t *task_queue);
static void thread_pool_task_queue_destory(task_queue_t *task_queue);
static void *thread_worker_routine(void *arg);
static int thread_pool_create_worker(pthread_t **threads, int max_thread_num);

/*****************************************************************************
 * 函  数:    thread_pool_task_queue_init
 * 功  能:    创建并初始化任务队列
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
static int thread_pool_task_queue_init(task_queue_t **task_queue)
{

    if (NULL == task_queue)
    {
        printf("thread_pool_task_queue_init()参数有误,task_queue为NULL\n");
        return -1;
    }

    *task_queue = (task_queue_t *)malloc(sizeof(task_queue_t));
    if (NULL == (*task_queue))
    {
        printf("thread_pool_task_queue_init() malloc failed\n");
        return -1;
    }

    (*task_queue)->head = NULL;
    (*task_queue)->tail = NULL;

    return 0;
}

/*****************************************************************************
 * 函  数:    thread_pool_task_queue_is_empty
 * 功  能:    检查任务队列是否为空
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
static int thread_pool_task_queue_is_empty(task_queue_t *task_queue)
{
    int ret = 0;

    if (NULL == task_queue->head)
    {
        ret = 1;
    }
    else
    {
        ret = 0;
    }
    
    return ret;
}

/*****************************************************************************
 * 函  数:    thread_pool_task_queue_push
 * 功  能:    向任务队列中添加任务
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
static void thread_pool_task_queue_push(task_queue_t *task_queue, task_t *task)
{

    task->next = NULL;

    if (NULL != task_queue->tail)
    {
        task_queue->tail->next = task;
    }
    else
    {
        task_queue->head = task;
    }

    task_queue->tail = task;

}

/*****************************************************************************
 * 函  数:    thread_pool_task_queue_pop
 * 功  能:    从任务队列中取出任务
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
static task_t *thread_pool_task_queue_pop(task_queue_t *task_queue)
{
    task_t *task = NULL;

    
    if (NULL != task_queue->head)
    {
        task = task_queue->head;
        task_queue->head = task->next;
        task->next = NULL;
        if (NULL == task_queue->head)
        {
            task_queue->tail = NULL;
        }
    }

    printf("取出客户端%d\n", (*(int *)task->arg - 3));
    return task;
}

/*****************************************************************************
 * 函  数:    thread_pool_task_queue_destory
 * 功  能:    销毁任务队列
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
static void thread_pool_task_queue_destory(task_queue_t *task_queue)
{
    task_t *task = NULL;


    if (NULL != task_queue->head)
    {
        task = task_queue->head;
        task_queue->head = task_queue->head->next;
        free(task);
    }

    task_queue->tail = NULL;
    free(task_queue);

}


/*****************************************************************************
 * 函  数:    thread_worker_routine
 * 功  能:    工作线程处理
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
static void *thread_worker_routine(void *arg)
{
    (void)arg;

    while(1)
    {
        pthread_mutex_lock (&(gs_thread_pool->task_queue_lock));
        while((1 == thread_pool_task_queue_is_empty(gs_thread_pool->task_queue)) && 
              (1 != gs_thread_pool->shutdown))
        {
            pthread_cond_wait (&(gs_thread_pool->task_queue_ready), &(gs_thread_pool->task_queue_lock));
        }

        /* 线程池要销毁了，退出线程 */
        if (1 == gs_thread_pool->shutdown)
        {
            pthread_mutex_unlock (&(gs_thread_pool->task_queue_lock));
            printf("线程%lu 退出\n", (long unsigned int )pthread_self());
            pthread_exit(NULL);
        }

        /* 从队列中取出一个任务 */
        task_t *task = thread_pool_task_queue_pop(gs_thread_pool->task_queue);
        //thread_pool_task_queue_print();
        pthread_mutex_unlock (&(gs_thread_pool->task_queue_lock));


        printf ("由线程%lu 处理客户端%d\n\n", (long unsigned int )pthread_self(), (*(int *)task->arg - 3));
        /* 执行任务 */
        task->task_process(task->arg);
        free(task->arg);
        free(task);
        task = NULL;

    }

    /* 退出线程， 正常情况下这一句应该是不可达的 */
    pthread_exit(NULL);
}

/*****************************************************************************
 * 函  数:    thread_pool_create_worker
 * 功  能:    创建工作线程
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
static int thread_pool_create_worker(pthread_t **threads, int max_thread_num)
{
    int i = 0;

    *threads = (pthread_t *)malloc(max_thread_num * sizeof(pthread_t));
    if (NULL == (*threads))
    {
        return -1;
    }

    for (i = 0; i < max_thread_num; i++)
    {
        if (0 != pthread_create(&((*threads)[i]), NULL, thread_worker_routine, NULL))
        {
            return -1;
        }
    }

    return 0;
}



/*****************************************************************************
 * 函  数:    thread_pool_init
 * 功  能:    创建并初始化线程池
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
int thread_pool_init(int max_thread_num)
{
    //int i = 0;

    gs_thread_pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (NULL == gs_thread_pool)
    {
        return -1;
    }

    gs_thread_pool->max_thread_num  = max_thread_num;
    gs_thread_pool->shutdown = 0;


    /* 初始化任务队列 */
    if (-1 == thread_pool_task_queue_init(&gs_thread_pool->task_queue))
    {
        return -1;
    }

    /* 初始化任务对列锁 */
    pthread_mutex_init (&(gs_thread_pool->task_queue_lock), NULL);

    /* 初始化任务对列条件变量 */
    pthread_cond_init (&(gs_thread_pool->task_queue_ready), NULL);

    /* 创建工作线程 */
    if (-1 == thread_pool_create_worker(&gs_thread_pool->threads, gs_thread_pool->max_thread_num))
    {
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * 函  数:    thread_pool_add_task
 * 功  能:    向线程池中添加任务
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
int thread_pool_add_task(void *(*task_process) (void *arg), void *arg)
{
    /* 构造一个新任务 */
    task_t *new_task = (task_t *)malloc(sizeof(task_t));
    if(NULL == new_task)
    {
        return -1;
    }

    new_task->task_process = task_process;
    new_task->arg = arg;
    new_task->next = NULL;


    /* 将新任务添加到任务队列中 */
    pthread_mutex_lock(&(gs_thread_pool->task_queue_lock));
    thread_pool_task_queue_push(gs_thread_pool->task_queue, new_task);
    //thread_pool_task_queue_print();
    pthread_mutex_unlock(&(gs_thread_pool->task_queue_lock));

    /* 通知阻塞的空闲工作线程有新任务到了 */
    pthread_cond_signal (&(gs_thread_pool->task_queue_ready));

    return 0;
}

/*****************************************************************************
 * 函  数:    thread_pool_destory
 * 功  能:    销毁线程池
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
int thread_pool_destory()
{
    int i = 0;

    if (1 == gs_thread_pool->shutdown)
    {
        return -1;
    }

    /* 唤醒所有阻塞的线程，线程池要销毁了 */
    pthread_cond_broadcast (&(gs_thread_pool->task_queue_ready));

    /* 等待工作线程退出，销毁为工作线程分配的ID */
    for (i = 0; i < gs_thread_pool->max_thread_num; i++)
    {
        pthread_join(gs_thread_pool->threads[i], NULL);
    }
    free(gs_thread_pool->threads);


    /* 销毁任务队列 */
    thread_pool_task_queue_destory(gs_thread_pool->task_queue);

    /* 销毁任务队列互斥锁 */
    pthread_mutex_destroy(&(gs_thread_pool->task_queue_lock));

    /* 销毁任务队列条件变量 */
    pthread_cond_destroy(&(gs_thread_pool->task_queue_ready));

    free(gs_thread_pool);
    gs_thread_pool = NULL;
 
    return 0;
}


/*****************************************************************************
 * 函  数:    thread_pool_worker_id_print
 * 功  能:    打印创建的工作线程ID（测试用函数）
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
void thread_pool_worker_id_print()
{
    int i = 0;

    printf("创建%d个工作线程，线程ID分别为: \n", gs_thread_pool->max_thread_num);
    for (i = 0; i < gs_thread_pool->max_thread_num; i++)
    {
        printf("线程%d: %lu\n", (i+1), (long unsigned int )gs_thread_pool->threads[i]);
    }

    printf("\n");

}
/*****************************************************************************
 * 函  数:    thread_pool_task_queue_print
 * 功  能:    打印任务队列（测试用函数）
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai
 * 更  新:    无
 ****************************************************************************/
void thread_pool_task_queue_print()
{
    task_t *task = NULL;

    task = gs_thread_pool->task_queue->head;

    while(task != NULL)
    {

       printf("客户端%d\n", (*(int *)(task->arg) - 3));
        task = task->next;
    }

    printf("\n");
}
