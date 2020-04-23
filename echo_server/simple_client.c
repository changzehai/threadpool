/*****************************************************************************/
/* 文件名:    simple_client.c                                                */
/* 描  述:    简单客户端实现                                                  */
/* 创  建:    2020-04-12 changzehai                                          */
/* 更  新:    无                                                             */
/* Copyright 1998 - 2020 CZH. All Rights Reserved                            */
/*****************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>


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
	int sockfd;
	int len;
	struct sockaddr_in address;
	int result;
	int cnt = 0;
	char buf[128] = {0};
	
    /* 创建客户端socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(8000);
	len = sizeof(address);

    /* 连接客户端 */
	result = connect(sockfd, (struct sockaddr *)&address, len);
	if (result == -1)
	{
	    perror("oops: client1");
	    return -1;
	}
	
	while (1)
	{
		sprintf(buf, "%d\n", cnt++);
		write(sockfd, buf, strlen(buf));
		printf("客户端发送: %s\n", buf);
		if (0 >= read(sockfd, buf, sizeof(buf)))
        {
            break;
        }
		printf("服务器回复: %s\n", buf);
		sleep(1);
	}
	
	close(sockfd);
	return 0;
}
