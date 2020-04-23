/*****************************************************************************/
/* 文件名:    thread_pool.h                                                  */
/* 描  述:    轻量HTTP服务器                                                  */
/* 创  建:    2020-04-12 changzehai                                          */
/* 更  新:    无                                                             */
/* Copyright 1998 - 2020 CZH. All Rights Reserved                            */
/*****************************************************************************/
#include <stdio.h>

#ifndef __THREAD_POOL_H_
#define __THREAD_POOL_H_


/*-----------------------------------*/
/* API函数声明                       */
/*-----------------------------------*/
extern int thread_pool_init(int max_thread_num);
extern int thread_pool_add_task(void *(*task_process) (void *arg), void *arg);
extern int thread_pool_destory();
extern void thread_pool_worker_id_print();
extern void thread_pool_task_queue_print();
#endif
