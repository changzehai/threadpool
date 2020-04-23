/*****************************************************************************/
/* 文件名:    echo_server.c                                                  */
/* 描  述:    简单回显服务器                                                  */
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
#include <signal.h>
#include "thread_pool.h"




/*****************************************************************************
 * 函  数:    echo_server_error_exit
 * 功  能:    记录错误信息并关闭服务器程序
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai(DTT)
 * 更  新:    无
 ****************************************************************************/
static void echo_server_error_exit(const char *error)
{
    perror(error);
    exit(1);
}


/*****************************************************************************
 * 函  数:    echo_server_startup
 * 功  能:    创建TCP服务监听
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai(DTT)
 * 更  新:    无
 ****************************************************************************/
static int echo_server_startup(void)
{
    int server_sock = -1;
    int on = 1;
    struct sockaddr_in server_addr;

    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == server_sock)
    {
        //echo_server_error_exit("socket failed");
        return -1;
    }

    memset(&server_addr, 0x00, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* 设置socket属性 */
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        echo_server_error_exit("setsockopt failed");
    }

    /* bind */
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        echo_server_error_exit("bind failed");
    }

    /* listen */
    if (listen(server_sock, 5) < 0)
    {
        echo_server_error_exit("listen failed");
    }

    return (server_sock);
}

/*****************************************************************************
 * 函  数:    echo_server_accpet_client_request
 * 功  能:    处理客户端请求
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai(DTT)
 * 更  新:    无
 ****************************************************************************/
void *echo_server_accpet_client_request(void *arg)
{
    int client_sock = *(int *)arg;
    int nbytes = 0;
    char buf[256] = {0};


    while(1)
    {
        /* 接收客户端数据并原样返回给客户端 */
        nbytes = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (nbytes > 0)
        {
            buf[nbytes] = '\0';
            write(client_sock, buf, nbytes);
        }
        else
        {
            /* 对端客户端退出，关闭客户端套接字 */
            close(client_sock);
            printf("客户端%d 退出\n", (client_sock - 3));
            free(arg);
            break;
        }
        
    }
   
    return NULL;
}

/*****************************************************************************
 * 函  数:    sigint_handler
 * 功  能:    ctrl+c信号处理
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai(DTT)
 * 更  新:    无
 ****************************************************************************/
void sigint_handler(int signum)
{
    (void)signum;

    printf("接收到服务器退出信号，服务器开始退从...\n");

    /* 销毁线程池 */
    thread_pool_destory();

    printf("服务器退出完毕!\n");
    exit(0);
}


/*****************************************************************************
 * 函  数:    main
 * 功  能:    主函数
 * 输  入:    无
 * 输  出:    无
 * 返回值:    无  
 * 创  建:    2020-04-12 changzehai(DTT)
 * 更  新:    无
 ****************************************************************************/
int main()
{
    int server_sock = -1;
    int client_sock = -1;
    socklen_t client_addr_len = 0;
    struct sockaddr_in client_addr;



    client_addr_len = sizeof(client_addr);
    memset(&client_addr, 0x00, sizeof(client_addr));

    /* 监听ctrl+c信号 */
    signal(SIGINT, sigint_handler);

    /* 启动server socket */
    server_sock = echo_server_startup();
    if (-1 == server_sock)
    {
        echo_server_error_exit("socket failed");
    }

    printf("echo server running on 8000 !!!\n");

    /* 初始化线程池 */
    if (-1 == thread_pool_init(4))
    {
        echo_server_error_exit("thread pool init failed"); 
    }

    /* 打印创建的线程ID */   
    thread_pool_worker_id_print();


    while (1)
    {
        /* 接受客户端连接 */
        client_sock = accept(server_sock,
                          (struct sockaddr *)&client_addr,
                          &client_addr_len);
        if (-1 == client_sock)
        {
            echo_server_error_exit("accept");
        }
        printf("客户端%d 上线\n", (client_sock - 3));

        void *arg = (int *)malloc(sizeof(client_sock));
        *(int *)arg = client_sock;


        /* 添加客户端请求任务到线程池中处理 */
        if (-1 == thread_pool_add_task(echo_server_accpet_client_request, arg))
        {
            perror("thread_pool_add_task failed");
        }

        printf("客户端%d 放入线程池\n", (client_sock - 3) );
        

    }

    /* 销毁线程池 */
    thread_pool_destory();

    /* 关闭服务端socket */
    close(server_sock);

    return 0;
}
