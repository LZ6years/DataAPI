/*
 * 程序名：rinetdin.cpp，反向网络代理服务程序-内网端。
*/
#include "_public.h"
using namespace idc;

int  cmdconnsock;  // 内网程序与外网程序的命令通道的socket。

int epollfd=0;     // epoll的句柄。
int tfd=0;            // 定时器的句柄。

#define MAXSOCK  1024
int clientsocks[MAXSOCK];       // 存放每个socket连接对端的socket的值。
int clientatime[MAXSOCK];       // 存放每个socket连接最后一次收发报文的时间。
string clientbuffer[MAXSOCK]; // 存放每个socket发送内容的buffer。

// 向目标ip和端口发起socket连接，bio取值：false-非阻塞io，true-阻塞io。
int conntodst(const char *ip,const int port,bool bio=false);

void _rinetdin(char *argv[]);

void EXIT(int sig);   // 进程退出函数。

clogfile logfile;

// cpactive pactive;     // 进程心跳。

int main(int argc,char *argv[])
{
    if (argc != 4)
    {
        printf("\n");
        printf("Using :./rinetdin logfile ip port\n\n");
        printf("Sample:./rinetdin /log/rinetd/rinetdin.log 192.168.171.121 5001\n\n");
        printf("        /home/lz6years/backend/myproject/tools/bin/procctl 5 /home/lz6years/backend/myproject/tools/bin/rinetdin /log/rinetd/rinetdin.log 192.168.171.121 5001\n\n");
        printf("logfile 本程序运行的日志文件名。\n");
        printf("ip      外网代理程序的ip地址。\n");
        printf("port    外网代理程序的通讯端口。\n\n\n");
        return -1;
    }

    // 关闭全部的信号和输入输出。
    // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
    // 但请不要用 "kill -9 +进程号" 强行终止。
    closeioandsignal(); signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

    // 打开日志文件。
    if (logfile.open(argv[1])==false)
    {
        printf("打开日志文件失败（%s）。\n",argv[1]); return -1;
    }

    // pactive.addpInfo(30,"inetd");       // 设置进程的心跳超时间为30秒。

    // 建立与外网程序的命令通道，采用阻塞的socket。
    if ((cmdconnsock=conntodst(argv[2],atoi(argv[3]),true))<0)
    {
        logfile.write("tcpclient.connect(%s,%s) 失败。\n",argv[2],argv[3]); return -1;
    }

    logfile.write("与外部的命令通道已建立(cmdconnsock=%d)。\n",cmdconnsock);

    // 命令通道建立之后，再设置为非阻塞的。
    fcntl(cmdconnsock,F_SETFL,fcntl(cmdconnsock,F_GETFD,0)|O_NONBLOCK);

    _rinetdin(argv);

    return 0;
}

void _rinetdin(char *argv[])
{
    // 创建epoll句柄
    epollfd = epoll_create1(0);
    struct epoll_event ev; // 声明事件的数据结构

    ev.events = EPOLLIN;
    ev.data.fd = cmdconnsock;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, cmdconnsock, &ev);

    // 创建定时器。
    tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC); // 创建timerfd。
    struct itimerspec timeout;
    memset(&timeout, 0, sizeof(struct itimerspec));
    timeout.it_value.tv_sec = 20; // 超时时间为20秒。
    timeout.it_value.tv_nsec = 0;
    timerfd_settime(tfd, 0, &timeout, NULL); // 开始计时。

    // 为定时器准备事件。
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd, &ev); // 把定时器的读事件加入epollfd中。

    struct epoll_event evs[10];
    while(true)
    {
        int infds = epoll_wait(epollfd, evs, 10, -1);
        // 返回失败。
        if (infds < 0)
        {
            logfile.write("epoll() failed。\n");
            EXIT(-1);
        }

        // 进入事件处理流程
        for (int i = 0; i < infds; ++i)
        {

            // 如果定时器的时间已到，有三件事要做：1）更新进程的心跳；2）向命令通道发送心跳报文；3）清理空闲的客户端socket。
            if (evs[i].data.fd == tfd)
            {
                // logfile.write("定时器时间已到。\n");

                timerfd_settime(tfd, 0, &timeout, 0); // 重新开始计时。

                // pactive.uptatime();        // 1）更新进程心跳；

                // 2） 清理空闲的客户端socket。
                for (int jj = 0; jj < MAXSOCK; jj++)
                {
                    // 如果客户端socket空闲的时间超过80秒就关掉它。
                    if ((clientsocks[jj] > 0) && ((time(0) - clientatime[jj]) > 80))
                    {
                        logfile.write("client(%d,%d) timeout。\n", clientsocks[jj], clientsocks[clientsocks[jj]]);
                        close(clientsocks[jj]);
                        close(clientsocks[clientsocks[jj]]);
                        // 把数组中对端的socket置空，这一行代码和下一行代码的顺序不能乱。
                        clientsocks[clientsocks[jj]] = 0;
                        // 把数组中本端的socket置空，这一行代码和上一行代码的顺序不能乱。
                        clientsocks[jj] = 0;
                    }
                }

                continue;
            }

            if(evs[i].data.fd == cmdconnsock)
            {
                // 解析控制内容
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));
                if (recv(cmdconnsock, buffer, sizeof(buffer), 0) <= 0)
                {
                    logfile.write("recv(fd=%d) failed.\n", cmdconnsock); EXIT(-1);
                }

                // 心跳报文
                if (strcmp(buffer, "<activetest>") == 0)
                    continue;

                // 解析其中的ip和port
                char dstip[22];
                memset(dstip, 0, sizeof(dstip));
                int dstport;
                getxmlbuffer(buffer, "dstip", dstip, 21);
                getxmlbuffer(buffer, "dstport", dstport);

                // 向外网服务器发送连接请求
                int srcfd = conntodst(argv[2], atoi(argv[3]), false);
                if(srcfd < 0){
                    logfile.write("conntodst(%s, %s) failed.\n", argv[2], argv[3]);
                    continue;
                }
                if (srcfd >= MAXSOCK){
                    logfile.write("连接数已超过最大值%d。\n", MAXSOCK);
                    close(srcfd);
                    continue;
                }

                // 向内网发送连接请求
                int dstfd = conntodst(dstip, dstport, false);
                if (dstfd < 0){
                    logfile.write("conntodst(%s, %d) failed.\n", dstip, dstport);
                    close(srcfd);
                    continue;
                }
                if (dstfd >= MAXSOCK) {
                    logfile.write("连接数已超过最大值%d。\n", MAXSOCK);
                    close(dstfd); close(srcfd);
                    continue;
                }

                // 注册两个fd的事件
                ev.events = EPOLLIN; ev.data.fd = srcfd;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, srcfd, &ev);
                ev.events = EPOLLIN; ev.data.fd = dstfd;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, dstfd, &ev);


                // 保存相互的连接通道
                clientsocks[srcfd] = dstfd; clientsocks[dstfd] = srcfd;
                clientatime[srcfd] = time(0); clientatime[dstfd] = time(0);

                // 把内网和外网的socket对接在一起。
                logfile.write("新建内外网通道(%d,%d[%s:%d]) ok。\n", srcfd, dstfd, dstip, dstport);

                continue;
            }

            if (evs[i].events & EPOLLIN) // 判断是否为读事件。
            {
                char buffer[5000]; // 存放从接收缓冲区中读取的数据。
                int buflen = 0;    // 从接收缓冲区中读取的数据的大小。

                // 从通道的一端读取数据。
                if ((buflen = recv(evs[i].data.fd, buffer, sizeof(buffer), 0)) <= 0)
                {
                    // 如果连接已断开，需要关闭通道两端的socket。
                    logfile.write("client(%d,%d) disconnected。\n", evs[i].data.fd, clientsocks[evs[i].data.fd]);
                    close(evs[i].data.fd);                        // 关闭客户端的连接。
                    close(clientsocks[evs[i].data.fd]);           // 关闭客户端对端的连接。
                    clientsocks[clientsocks[evs[i].data.fd]] = 0; // 把数组中对端的socket置空，这一行代码和下一行代码的顺序不能乱。
                    clientsocks[evs[i].data.fd] = 0;              // 把数组中本端的socket置空，这一行代码和上一行代码的顺序不能乱。

                    continue;
                }

                // 成功的读取到了数据，把接收到的报文内容原封不动的发给通道的对端。
                // logfile.write("from %d to %d,%d bytes。\n",evs[i].data.fd,clientsocks[evs[i].data.fd],buflen);
                // send(clientsocks[evs[i].data.fd],buffer,buflen,0);

                logfile.write("from %d,%d bytes\n", evs[i].data.fd, buflen);

                // 把读取到的数据追加到对端socket的buffer中。
                clientbuffer[clientsocks[evs[i].data.fd]].append(buffer, buflen);

                // 修改对端socket的事件，增加写事件。
                ev.data.fd = clientsocks[evs[i].data.fd];
                ev.events = EPOLLIN | EPOLLOUT;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev);

                // 更新通道两端socket的活动时间。
                clientatime[evs[i].data.fd] = time(0);
                clientatime[clientsocks[evs[i].data.fd]] = time(0);
            }

            // 判断客户端的socket是否有写事件（发送缓冲区没有满）。
            if (evs[i].events & EPOLLOUT)
            {
                // 把socket缓冲区中的数据发送出去。
                int writen = send(evs[i].data.fd, clientbuffer[evs[i].data.fd].data(), clientbuffer[evs[i].data.fd].length(), 0);

                // 以下代码模拟不能一次发完全部数据的场景。
                // int ilen;
                // if (clientbuffer[evs[i].data.fd].length()>10) ilen=10;
                // else ilen=clientbuffer[evs[i].data.fd].length();
                // int writen=send(evs[i].data.fd,clientbuffer[evs[i].data.fd].data(),ilen,0);

                logfile.write("to %d,%d bytes\n", evs[i].data.fd, writen);

                // 删除socket缓冲区中已成功发送的数据。
                clientbuffer[evs[i].data.fd].erase(0, writen);

                // 如果socket缓冲区中没有数据了，不再关心socket的写件事。
                if (clientbuffer[evs[i].data.fd].length() == 0)
                {
                    ev.data.fd = evs[i].data.fd;
                    ev.events = EPOLLIN;
                    epoll_ctl(epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev);
                }
            }
        }
    }

}

// 向目标地址和端口发起socket连接。
int conntodst(const char *ip,const int port,bool bio)
{
    // 第1步：创建客户端的socket。
    int sockfd;
    if ( (sockfd = socket(AF_INET,SOCK_STREAM,0))==-1) return -1; 

    // 第2步：向服务器发起连接请求。
    struct hostent* h;
    if ( (h = gethostbyname(ip)) == 0 ) { close(sockfd); return -1; }
  
    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port); // 指定服务端的通讯端口。
    memcpy(&servaddr.sin_addr,h->h_addr,h->h_length);

    // 把socket设置为非阻塞。
    if (bio==false) fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFD,0)|O_NONBLOCK);

    if (connect(sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr))<0)
    {
        if (errno!=EINPROGRESS)
        {
            logfile.write("connect(%s,%d) failed.\n",ip,port); return -1;
        }
    }

    return sockfd;
}

void EXIT(int sig)
{
    logfile.write("程序退出，sig=%d。\n\n",sig);

    // 关闭内网程序与服务端的命令通道。
    close(cmdconnsock);

    // 关闭全部客户端的socket。
    for (auto aa : clientsocks)
        if (aa > 0)
            close(aa);

    close(epollfd); // 关闭epoll。

    close(tfd); // 关闭定时器。

    exit(0);
}