/*
* 程序名：tcpselect.cpp，此程序用于演示采用select模型实现网络通讯的服务端。
* 作者：吴从周
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

// 初始化网络
int initserver(int port);

int main(int argc, char *argv[])
{
    if (argc != 2){
        printf("usage: ./tcpselect port\n");
        return -1;
    }

    // 初始化监听的socket
    int listensock = initserver(atoi(argv[1]));
    printf("listensock=%d\n", listensock);

    // 读事件： 1）已连接队列中有已经准备好的socket（有新的客户端连上来了）；
    //          2）接收缓存中有数据可以读（对端发送的报文已到达）；
    //          3）tcp连接已断开（对端调用close()函数关闭了连接）。
    // 写事件：发送缓冲区没有满，可以写入数据（可以向对端发送报文）。

    fd_set readfds;
    FD_ZERO(&readfds); FD_SET(listensock, &readfds);

    int maxfd = listensock;

    while (true)
    {
        // 用于表示超时时间的结构体。
        struct timeval timeout;
        timeout.tv_sec = 10; // 秒
        timeout.tv_usec = 0; // 微秒。

        fd_set tmpfds = readfds;

        // 调用select() 等待事件的发生
        int infds = select(maxfd + 1, &tmpfds, NULL, NULL, 0);

        if (infds < 0){
            perror("select() failed");
            break;
        }
        // 如果infds==0，表示select()超时。
        if (infds == 0){
            printf("select() timeout.\n");
            continue;
        }

        // 如果infds > 0, 说明有事件发生， infds存放了已经发生事件的个数。
        for (int eventfd = 0; eventfd <= maxfd; eventfd++)
        {
            if(FD_ISSET(eventfd, &tmpfds) == 0)
                continue;

            // 若是监听的fd发生了事件，表示连接队列中已有准备好的socket
            if(eventfd == listensock){
                // 连接上accept对方，然后将clientsocket的内容记录到fd_set表中
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int clientsock = accept(listensock, (struct sockaddr *)&client, &len);
                if (clientsock < 0){
                    perror("accept() failed");
                    continue;
                }
                printf("accept client(socket=%d) ok.\n", clientsock);

                FD_SET(clientsock, &readfds); // 把bitmap中新连上来的客户端的标志位置为1。

                if (maxfd < clientsock)
                    maxfd = clientsock; // 更新maxfd的值。
            }
            else
            {
                // 若是客户端socket事件，表示接受缓存中有数据可以读取
                char buffer[1024]; // 存放从接收缓冲区中读取的数据。
                memset(buffer, 0, sizeof(buffer));
                if (recv(eventfd, buffer, sizeof(buffer), 0) <= 0)  // 关闭socket请求 or 已断开
                {
                    // 如果客户端的连接已断开。
                    printf("client(eventfd=%d) disconnected.\n", eventfd);
                    
                    close(eventfd); // 关闭客户端的socket

                    FD_CLR(eventfd, &readfds); // 把bitmap中已关闭客户端的标志位清空。

                    if (eventfd == maxfd) // 重新计算maxfd的值，注意，只有当eventfd==maxfd时才需要计算。
                    {
                        for (int ii = maxfd; ii > 0; ii--) // 从后面往前找。
                        {
                            if (FD_ISSET(ii, &readfds))
                            {
                                maxfd = ii;
                                break;
                            }
                        }
                    }
                }
                else  // 接受对端请求，并
                {
                    // 如果客户端有报文发过来。
                    printf("recv(eventfd=%d):%s\n", eventfd, buffer);

                    // 把接收到的报文内容原封不动的发回去。
                    send(eventfd, buffer, strlen(buffer), 0);
                }
            }
        }
    }
}

int initserver(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("listen socket create failed.\n");
        return -1;
    }

    // 使 服务器 申请断开后，可以立刻重用端口，不要有time_wait状态
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    // 准备sock的绑定端口信息
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if(bind(sock, (struct sockaddr *)&servaddr, sizeof servaddr)){
        perror("bind() failed");
        close(sock);
        return -1;
    }

    if(listen(sock, 5)){
        perror("listen() failed");
        close(sock);
        return -1;
    }

    return sock;
}
