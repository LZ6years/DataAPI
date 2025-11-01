/*
 * 程序名：inetd.cpp，正向网络代理服务程序。
 * 作者：吴从周
*/
#include "_public.h"
using namespace idc;

struct st_route
{
    int  srcport;        // 源端口
    char dstip[31];      // 目标主机的地址
    int  dstport;        // 目标主机的端口
    int listensock;      // 源端口接听的socket
}stroute;
vector<struct st_route> vroute;
bool loadroute(const char *inifile);

int initserver(const int port);

const int MAXSOCK = 1024;       //最大的sock数量
int    clientsocks[MAXSOCK];    // 存放每个socket连接对端的socket的值。
time_t clientatime[MAXSOCK];    // 存放每个socket连接最后一次收发报文的时间。
string clientbuffer[MAXSOCK];   // 存放每个socket接受到的内容

int epollfd = 0;           // epoll的句柄。

clogfile logfile;
cpactive pactive;

void _help();
void EXIT(int sig);

void _inetd();      //主函数
int conntodst(const char* dstip, const int dstport);    // 连接的目标地址

int main(int argc, char *argv[])
{
    if (argc != 3){
        _help();
        return -1;
    }

    // 关闭全部的信号和输入输出。
    // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
    // 但请不要用 "kill -9 +进程号" 强行终止。
    closeioandsignal(true);  signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

    // 打开日志文件。
    if (!logfile.open(argv[1])){
        printf("打开日志文件失败（%s）。\n",argv[1]); return -1;
    }

    if(!loadroute(argv[2])){
        logfile.write("loadroute(%s) failed.\n", argv[2]);
    }

    // 初始化服务段用于监听的socket
    for(auto& val : vroute){
        if((val.listensock = initserver(val.srcport)) == false){
            // 如果某一个socket初始化失败，忽略它。
            logfile.write("initserver(%d) failed.\n", val.srcport);
            continue;
        }

        // 把监听socket设置成非阻塞。
        fcntl(val.listensock, F_SETFL, fcntl(val.listensock, F_GETFD, 0) | O_NONBLOCK);
    }

    _inetd();


    return 0;
}

void _inetd()
{
    // 创建epoll句柄
    epollfd = epoll_create1(0);
    struct epoll_event ev; // 声明事件的数据结构

    // 为每一个监听的socket准备读事件
    for (auto &val : vroute)
    {
        if (val.listensock < 0)
            continue;

        ev.events = EPOLLIN;
        ev.data.fd = val.listensock;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, val.listensock, &ev);
    }

    /*
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // 把定时器加入epoll。
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC); // 创建timerfd。
    struct itimerspec timeout;                                             // 定时时间的数据结构。
    memset(&timeout, 0, sizeof(struct itimerspec));
    timeout.it_value.tv_sec = 10; // 定时时间为10秒。
    timeout.it_value.tv_nsec = 0;
    timerfd_settime(tfd, 0, &timeout, 0); // 开始计时。alarm(10)
    ev.data.fd = tfd;                     // 为定时器准备事件。
    ev.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, tfd, &ev); // 把定时器fd加入epoll。
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // 把信号加入epoll。
    sigset_t sigset;                      // 创建信号集。
    sigemptyset(&sigset);                 // 初始化（清空）信号集。
    sigaddset(&sigset, SIGINT);           // 把SIGINT信号加入信号集。
    sigaddset(&sigset, SIGTERM);          // 把SIGTERM信号加入信号集。
    sigprocmask(SIG_BLOCK, &sigset, 0);   // 对当前进程屏蔽信号集（当前程将收不到信号集中的信号）。
    int sigfd = signalfd(-1, &sigset, 0); // 创建信号集的fd。
    ev.data.fd = sigfd;                   // 为信号集的fd准备事件。
    ev.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sigfd, &ev); // 把信号集fd加入epoll。
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    */


    struct epoll_event evs[10]; // 存放epoll返回的事件。

    while(true)
    {
        int infds = epoll_wait(epollfd, evs, 10, -1);

        // 返回失败。
        if (infds < 0){
            logfile.write("epoll() failed。\n");
            EXIT(-1);
        }

        // 遍历epoll返回的已发生事件的数组evs。
        for (int i = 0; i < infds; ++i)
        {
            logfile.write("已发生事件的socket=%d\n", evs[i].data.fd);

            /*
            ////////////////////////////////////////////////////////
            // 如果定时器的时间已到，有两件事要做：1）更新进程的心跳；2）清理空闲的客户端socket，路由器也会这么做。
            if (evs[i].data.fd == tfd)
            {
                logfile.write("定时器时间已到。\n");

                timerfd_settime(tfd, 0, &timeout, 0); // 重新开始计时。 alarm(10)

                pactive.uptatime(); // 1）更新进程心跳。

                // 2）清理空闲的客户端socket。
                for (int j = 0; j < MAXSOCK; j++) // 可以把最大的socket记下来，这个循环不必遍历整个数组。
                {
                    // 如果客户端socket空闲的时间超过80秒就关掉它。
                    if ((clientsocks[j] > 0) && ((time(0) - clientatime[j]) > 80))
                    {
                        logfile.write("client(%d,%d) timeout。\n", clientsocks[j], clientsocks[clientsocks[j]]);
                        close(clientsocks[clientsocks[j]]);
                        close(clientsocks[j]);
                        // 把数组中对端的socket置空，这一行代码和下一行代码的顺序不能乱。
                        clientsocks[clientsocks[j]] = 0;
                        // 把数组中本端的socket置空，这一行代码和上一行代码的顺序不能乱。
                        clientsocks[j] = 0;
                    }
                }

                continue;
            }
            ////////////////////////////////////////////////////////

            ////////////////////////////////////////////////////////
            // 如果收到了信号。
            if (evs[i].data.fd == sigfd)
            {
                struct signalfd_siginfo siginfo;                                // 信号的数据结构。
                int s = read(sigfd, &siginfo, sizeof(struct signalfd_siginfo)); // 读取信号。
                logfile.write("收到了信号=%d。\n", siginfo.ssi_signo);

                // 在这里编写处理信号的代码。

                continue;
            }
            ////////////////////////////////////////////////////////
            */

            // 查找返回的事件数组有无在listensock中的
            int j;
            for (j = 0; j < vroute.size(); ++j)
            {
                if(evs[i].data.fd == vroute[j].listensock)
                {
                    // 从已连接队列中获取客户端连上来的socket。 // socket是7
                    struct sockaddr_in client;
                    socklen_t len = sizeof(client);
                    int srcsock = accept(vroute[j].listensock, (struct sockaddr *)&client, &len);
                    if (srcsock < 0) break;
                    if (srcsock >= MAXSOCK){
                        logfile.write("连接数已超过最大值%d。\n", MAXSOCK);
                        close(srcsock);
                        break;
                    }

                    // 向目的地址和端口发起连接，若连接失败，会被epoll发现并关闭通道
                    int dstsock = conntodst(vroute[j].dstip, vroute[j].dstport); // socket是8
                    if (dstsock < 0){  close(srcsock);  break;}
                    if (dstsock >= MAXSOCK){
                        logfile.write("连接数已超过最大值%d。\n", MAXSOCK);
                        close(srcsock); close(dstsock);
                        break;
                    }

                    logfile.write("accept on port %d,client(%d,%d) ok。\n", vroute[j].srcport, srcsock, dstsock);

                    // 为新连接的两个socket准备读事件
                    ev.data.fd = srcsock; ev.events = EPOLLIN;
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, srcsock, &ev);
                    ev.data.fd = dstsock; ev.events = EPOLLIN;
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, dstsock, &ev);

                    // 记录相互通讯的socket的fd关系
                    clientsocks[srcsock] = dstsock; clientatime[srcsock] = time(0);
                    clientsocks[dstsock] = srcsock; clientatime[dstsock] = time(0);

                    break;
                }
            }

            if (j < vroute.size())
                continue;

            if(evs[i].events & EPOLLIN)
            {
                // 客户端连接有socket事件
                char buffer[5120];
                int buflen = 0;

                if ((buflen = recv(evs[i].data.fd, buffer, sizeof(buffer), 0)) <= 0)
                {
                    // 如果连接已断开，需要关闭通道两端的socket。
                    logfile.write("client(%d,%d) disconnected。\n", evs[i].data.fd, clientsocks[evs[i].data.fd]);
                    close(evs[i].data.fd);
                    close(clientsocks[evs[i].data.fd]);
                    clientsocks[evs[i].data.fd] = 0;
                    evs[i].data.fd = 0;

                    continue;
                }

                // // 成功的读取到了数据，把接收到的报文内容原封不动的发给通道的对端。
                // logfile.write("from %d to %d,%d bytes。\n", evs[i].data.fd, clientsocks[evs[i].data.fd], buflen);
                // send(clientsocks[evs[i].data.fd], buffer, buflen, 0);

                logfile.write("from %d,%d bytes\n", evs[i].data.fd, buflen);

                // 将读取到的内容加入到对端的socket的buffer中
                clientbuffer[clientsocks[evs[i].data.fd]].append(buffer, buflen);

                ev.data.fd = clientsocks[evs[i].data.fd];
                ev.events = EPOLLIN | EPOLLOUT;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, ev.data.fd, &ev);

                // 更新通道两端socket的活动时间。
                clientatime[evs[i].data.fd] = time(0);
                clientatime[clientsocks[evs[i].data.fd]] = time(0);
            }

            if(evs[i].events & EPOLLOUT)
            {
                int writen = send(evs[i].data.fd, clientbuffer[evs[i].data.fd].data(), clientbuffer[evs[i].data.fd].length(), 0);

                // // 以下代码模拟不能一次发完全部数据的场景。
                // int ilen;
                // if (clientbuffer[evs[i].data.fd].length() > 10)
                //     ilen = 10;
                // else
                //     ilen = clientbuffer[evs[i].data.fd].length();
                // int writen = send(evs[i].data.fd, clientbuffer[evs[i].data.fd].data(), ilen, 0);

                logfile.write("to %d,%d bytes\n", evs[i].data.fd, writen);

                // 将已经发送的内容删除
                clientbuffer[evs[i].data.fd].erase(0, writen);

                // 若当前已经没有数据了
                if (clientbuffer[evs[i].data.fd].length() == 0)
                {
                    // 还原
                    ev.data.fd=evs[i].data.fd;
                    ev.events=EPOLLIN;
                    epoll_ctl(epollfd,EPOLL_CTL_MOD,ev.data.fd,&ev);

                }
            }
        }
    }
}

int conntodst(const char *dstip, const int dstport)
{
    // 创建客户端的socket。
    int sockfd;
    if ( (sockfd = socket(AF_INET,SOCK_STREAM,0))==-1) return -1;

    // 创建对方的地址信息（支持域名方式）
    struct hostent *h;
    if ((h = gethostbyname(dstip)) == 0){ close(sockfd); return -1; }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(dstport); // 指定服务端的通讯端口。
    memcpy(&servaddr.sin_addr, h->h_addr, h->h_length);

    // 把socket设置为非阻塞。
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK);
    
    // 连接的目标地址
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        if (errno != EINPROGRESS){
            logfile.write("connect(%s,%d) failed.\n", dstip, dstport);
            return -1;
        }
    }

    return sockfd;
}

int initserver(const int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        logfile.write("socket(%d) failed.\n", port);
        return -1;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        logfile.write("bind(%d) failed.\n", port);
        close(sock);
        return -1;
    }

    if (listen(sock, 5) != 0){
        logfile.write("listen(%d) failed.\n", port);
        close(sock);
        return -1;
    }

    return sock;
}

bool loadroute(const char *inifile)
{
    cifile ifile;

    if (ifile.open(inifile) == false){
        logfile.write("打开代理路由参数文件(%s)失败。\n", inifile);
        return false;
    }

    string strbuffer;
    ccmdstr cmdstr;

    while(true)
    {
        if(!ifile.readline(strbuffer))
            break;

        cmdstr.splittocmd(strbuffer, " ", true);

        memset(&stroute, 0, sizeof stroute);
        cmdstr.getvalue(0, stroute.srcport);
        cmdstr.getvalue(1, stroute.dstip);
        cmdstr.getvalue(2, stroute.dstport);

        vroute.push_back(stroute);
    }

    // // 用于测试vroute是否装入正确的内容
    // for (int i = 0; i < vroute.size(); ++i){
    //     logfile.write("%d: (srcport: %d),(dstip: %s),(dstport: %d)\n",
    //                   i + 1, vroute[i].srcport, vroute[i].dstip, vroute[i].dstport);
    // }

    logfile.write("loadroute success(%s).\n", inifile);
    return true;
}

void _help()
{
    printf("\n");
    printf("Using :./inetd logfile inifile\n\n");
    printf("Sample:./inetd /tmp/inetd.log /etc/inetd.conf\n\n");
    printf("        /home/lz6years/backend/myproject/tools/bin/procctl 5 /home/lz6years/backend/myproject/tools/bin/inetd /log/inetd/inetd.log /etc/inetd.conf\n\n");
    printf("本程序的功能是正向代理，如果用到了1024以下的端口，则必须由root用户启动。\n");
    printf("logfile 本程序运行的日是志文件。\n");
    printf("inifile 路由参数配置文件。\n");
}

void EXIT(int sig)
{
    logfile.write("程序退出，sig=%d。\n\n",sig);

    // 关闭全部监听的socket。
    for (auto &aa:vroute)
        if (aa.listensock>0) close(aa.listensock);

    // 关闭全部客户端的socket。
    for (auto aa:clientsocks)
        if (aa>0) close(aa);

    close(epollfd);   // 关闭epoll。

    exit(0);
}
