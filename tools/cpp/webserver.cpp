/*
 * 程序名：webserver.cpp，数据访问接口模块。
 * 作者：吴从周
 */
#include "_public.h"
#include "_ooci.h"
using namespace idc;

void EXIT(int sig); // 进程退出函数。

clogfile logfile; // 本程序运行的日志。

// 初始化服务端的监听端口。
int initserver(const int port);

// 从GET请求中获取参数的值：strget-GET请求报文的内容；name-参数名；value-参数值；len-参数值的长度。
bool getvalue(const string &strget, const string &name, string &value);

class Worker
{
private:

    ////  保存客户端的传来的和发送的信息
    struct st_client{
        string clientip;      // 客户端的ip。
        time_t clientatime;    // 客户端最后一次活动的时间。
        string recvbuffer;    // 接收端的buffer
        string sendbuffer;    // 发送端的buffer
    };
    unordered_map<int, struct st_client> clientmap; // 存放客户端对象的哈希表，俗称状态机。

    //// 入队信息
    // 接收/发送队列的结构体。
    struct st_msg
    {
        int sock = 0;   // 客户端的socket。
        string message; // 接收/发送的报文。

        st_msg(int in_sock, string &in_message) : sock(in_sock), message(in_message) { logfile.write("构造了报文。\n"); }
    };

    // 接收队列
    queue< shared_ptr<st_msg> > m_rq;     // 接收队列
    mutex m_mutex_rq;                   // 接收队列的互斥锁(为操作上锁)
    condition_variable m_cond_rq;       // 接收队列的条件变量(用于唤醒工作线程)


    // 发送队列
    queue< shared_ptr<st_msg> > m_sq;     // 接收队列
    mutex m_mutex_sq;                   // 接收队列的互斥锁(为操作上锁)
    int m_sendpipe[2] = {0};            // 工作线程通知发送线程的无名管道。

    atomic_bool m_exit;                                             // 如果m_exit==true，工作线程和发送线程将退出。
public:
    int m_recvpipe[2] = {0}; // 主进程通知接收线程退出的管道。主进程要用到该成员，所以声明为public。
    Worker()
    {
        pipe(m_sendpipe); // 创建工作线程通知发送线程的无名管道。
        pipe(m_recvpipe); // 创建主进程通知接收线程退出的管道。
        m_exit = false;
    }

public:

    // 接收外端的IO请求连接, 发送消息到消息队列中
    void recvWorker(const int port)
    {
        // 创建port的接听服务器
        int listenfd = initserver(port);

        int epollfd = epoll_create1(0);
        struct epoll_event ev;
        ev.events = EPOLLIN; ev.data.fd = listenfd;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);

        // 把接收主进程通知的管道加入epoll。
        ev.data.fd = m_recvpipe[0];
        ev.events = EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev);

        // 事件主循环
        struct epoll_event evs[10];
        while (true)
        {
            int rfds = epoll_wait(epollfd, evs, 10, -1);

            if(rfds < 0){
                logfile.write("epoll_wait return (%d).\n", rfds);
                EXIT(-1);
            }

            for (int i = 0; i < rfds; ++i)
            {
                // 监听的事件端发生, 建立连接
                if(evs[i].data.fd == listenfd)
                {
                    struct sockaddr_in client;
                    socklen_t len = sizeof(client);
                    int srcsock = accept(listenfd, (sockaddr *)&client, &len);
                    if(srcsock <= 0){
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                            continue;
                        logfile.write("客户端 %d 错误断开，errno=%d\n", srcsock, errno);
                        continue;
                    }
                    fcntl(srcsock, F_SETFL, fcntl(srcsock, F_GETFD, 0) | O_NONBLOCK); // 把socket设置为非阻塞。
                    clientmap[srcsock].clientip = inet_ntoa(client.sin_addr);
                    clientmap[srcsock].clientatime = time(0);

                    // 将其注册入epoll中
                    ev.events = EPOLLIN; ev.data.fd = srcsock;
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, srcsock, &ev);
                }

                // 如果是管道有读事件。
                if (evs[i].data.fd == m_recvpipe[0])
                {
                    logfile.write("接收线程：即将退出。\n");

                    m_exit = true; // 把退出的原子变量置为true。

                    m_cond_rq.notify_all(); // 通知全部的工作线程退出。

                    write(m_sendpipe[1], (char *)"o", 1); // 通知发送线程退出。

                    return;
                }

                // 若是读事件发生 1) 客户端以及断开连接  2) 来了http请求, 接收并发到接收队列中
                if(evs[i].events & EPOLLIN)
                {
                    char buffer[5120];
                    int buflen;
                    memset(buffer, 0, sizeof buffer);

                    if((buflen = recv(evs[i].data.fd, buffer, sizeof(buffer), 0)) <= 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            continue;

                        // 客户端已断开
                        clientmap.erase(evs[i].data.fd);  close(evs[i].data.fd);
                        logfile.write("客户端 %d 已经断开\n", evs[i].data.fd);
                        continue;
                    }

                    logfile.write("接收线程：recv %d,%d bytes\n", evs[i].data.fd, buflen);

                    // 接收到值, 拼接到clientbuffer[fd]的缓存中
                    clientmap[evs[i].data.fd].recvbuffer.append(buffer, buflen);

                    // 若接收到的内容最后已经按照\r\n\r\n结尾, 则已经接收完毕
                    if (clientmap[evs[i].data.fd].recvbuffer.compare(clientmap[evs[i].data.fd].recvbuffer.length()-4,4,"\r\n\r\n") == 0){
                        logfile.write("接收线程：接收到了一个完整的请求报文。\n\n %s", clientmap[evs[i].data.fd].recvbuffer.c_str());

                        _inrq((int)evs[i].data.fd, clientmap[evs[i].data.fd].recvbuffer);    // 把完整的请求报文入队，交给工作线程。

                        clientmap[evs[i].data.fd].recvbuffer.clear();  // 清空socket的recvbuffer。
                    }

                    clientmap[evs[i].data.fd].clientatime = time(0);  //更新时间
                }
            }
        }
    }


    // 工作线程, 消费recvqueue中的数据, id为其中的编号,用于调试写日志
    void worker(const int id)
    {
        connection conn;
        if (conn.connecttodb("idc/root@snorcl11g_121", "Simplified Chinese_China.AL32UTF8") != 0)
        {
            logfile.write("connect database(idc/root@snorcl11g_121) failed.\n%s\n", conn.message());
            return;
        }

        while (true)
        {
            shared_ptr<st_msg> ptr;
            {
                unique_lock<mutex> lock(m_mutex_rq); // 把互斥锁转换成unique_lock<mutex>，并申请加锁。

                while (m_rq.empty()) // 如果队列空，进入循环，否则直接处理数据。必须用循环，不能用if
                {
                    m_cond_rq.wait(lock); // 等待生产者的唤醒信号。

                    if (m_exit == true){
                        logfile.write("工作线程（%d）：即将退出。\n", id);
                        return;
                    }
                }

                ptr = m_rq.front();
                m_rq.pop(); // 出队一个元素。
            }

            // 处理出队的元素，即客户端的请求报文。
            logfile.write("工作线程（%d）：sock=%d,msg=%s\n", id, ptr->sock, ptr->message.c_str());

            /////////////////////////////////////////////////////////////
            // 在这里增加处理客户端请求报文的代码（解析请求报文、判断权限、执行查询数据的SQL语句、生成响应报文）。
            string sendbuf;
            _main(conn, ptr->message, sendbuf);
            string message = sformat(
                                 "HTTP/1.1 200 OK\r\n"
                                 "Server: webserver\r\n"
                                 "Content-Type: text/html;charset=utf-8\r\n") +
                             sformat("Content-Length:%d\r\n\r\n", sendbuf.size()) + sendbuf;
            /////////////////////////////////////////////////////////////

            logfile.write("工作线程（%d）回应：sock=%d,mesg=%s\n", id, ptr->sock, message.c_str());

            // 把客户端的socket和响应报文放入发送队列。
            _insq(ptr->sock, message);
        }
    }

    void sendWorker()
    {
        int epollfd = epoll_create1(0);
        struct epoll_event ev;
        ev.events = EPOLLIN; ev.data.fd = m_sendpipe[0];
        epoll_ctl(epollfd, EPOLL_CTL_ADD, m_sendpipe[0], &ev);

        struct epoll_event evs[10]; // 存放epoll返回的事件。
        while (true) // 进入事件循环。
        {
            // 等待监视的socket有事件发生。
            int infds = epoll_wait(epollfd, evs, 10, -1);

            // 返回失败。
            if (infds < 0){
                logfile.write("发送线程：epoll() failed。\n");
                return;
            }

            // 遍历epoll返回的已发生事件的数组evs。
            for (int i = 0; i < infds; i++)
            {
                logfile.write("发送线程：已发生事件的fd=%d(%d)\n", evs[i].data.fd, evs[i].events);

                ////////////////////////////////////////////////////////
                // 如果发生事件的是管道，表示发送队列中有报文需要发送。
                if (evs[i].data.fd == m_sendpipe[0])
                {
                    if (m_exit == true){
                        logfile.write("发送线程：即将退出。\n");
                        return;
                    }

                    char cc;
                    read(m_sendpipe[0], &cc, 1); // 读取管道中的数据，只有一个字符，不关心其内容。

                    shared_ptr<st_msg> ptr;
                    lock_guard<mutex> lock(m_mutex_sq); // 申请加锁。

                    while (!m_sq.empty())
                    {
                        ptr = m_sq.front();
                        m_sq.pop(); // 出队一个元素（报文）。

                        // 把出队的报文保存到socket的发送缓冲区中。
                        clientmap[ptr->sock].sendbuffer.append(ptr->message);

                        // 关注客户端socket的写事件。
                        ev.data.fd = ptr->sock;
                        ev.events = EPOLLOUT;
                        epoll_ctl(epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
                    }

                    continue;
                }
                ////////////////////////////////////////////////////////

                ////////////////////////////////////////////////////////
                // 判断客户端的socket是否有写事件（发送缓冲区没有满）。
                if (evs[i].events & EPOLLOUT)
                {
                    // 把响应报文发送给客户端。
                    int writen = send(evs[i].data.fd, clientmap[evs[i].data.fd].sendbuffer.data(), clientmap[evs[i].data.fd].sendbuffer.length(), 0);

                    logfile.write("发送线程：向%d发送了%d字节。\n", evs[i].data.fd, writen);

                    // 删除socket缓冲区中已成功发送的数据。
                    clientmap[evs[i].data.fd].sendbuffer.erase(0, writen);

                    // 如果socket缓冲区中没有数据了，不再关心socket的写件事。
                    if (clientmap[evs[i].data.fd].sendbuffer.length() == 0)
                    {
                        ev.data.fd = evs[i].data.fd;
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, ev.data.fd, &ev);
                    }
                }
                ////////////////////////////////////////////////////////
            }
        }
    }
private:
    
    // 入队到接收消息对列中
    void _inrq(int fd, string& recvbuffer)
    {
        shared_ptr<st_msg> msg_ptr = make_shared<st_msg>(fd, recvbuffer);

        // 上锁
        lock_guard<mutex> lock(m_mutex_rq);

        m_rq.push(msg_ptr);   // 入队

        // 通知一个线程工作
        m_cond_rq.notify_one();
    }

    // 入队到发送消息队列中
    void _insq(int fd, string& sendbuffer)
    {
        shared_ptr<st_msg> msg_ptr = make_shared<st_msg>(fd, sendbuffer);
        {
            // 上锁
            lock_guard<mutex> lock(m_mutex_sq);

            m_sq.push(msg_ptr); // 入队
        }

        write(m_sendpipe[1], (char *)"o", 1); // 通知发送线程处理发送队列中的数据。
    }

    // 业务主函数
    // 处理客户端的请求报文，生成响应报文。
    // conn-数据库连接；recvbuf-http请求报文；sendbuf-http响应报文的数据部分，不包括状态行和头部信息。
    void _main(connection &conn, const string &recvbuf, string &sendbuf)
    {
        string username, passwd, intername;

        getvalue(recvbuf, "username", username);   // 解析用户名。
        getvalue(recvbuf, "passwd", passwd);       // 解析密码。
        getvalue(recvbuf, "intername", intername); // 解析接口名。

        // 1）验证用户名和密码是否正确。
        sqlstatement stmt(&conn);
        stmt.prepare("select ip from T_USERINFO where username=:1 and passwd=:2 and rsts=1");
        string ip;
        stmt.bindin(1, username);
        stmt.bindin(2, passwd);
        stmt.bindout(1, ip, 50);
        stmt.execute();
        if (stmt.next() != 0)
        {
            sendbuf = "<retcode>-1</retcode><message>用户名或密码不正确。</message>";
            return;
        }

        // 2）判断客户连上来的地址是否在绑定ip地址的列表中。
        if (ip.empty() == false)
        {
            // 略去十万八千行代码。
        }

        // 3）判断用户是否有访问接口的权限。
        stmt.prepare("select count(*) from T_USERANDINTER "
                     "where username=:1 and intername=:2 and intername in (select intername from T_INTERCFG where rsts=1)");
        stmt.bindin(1, username);
        stmt.bindin(2, intername);
        int icount = 0;
        stmt.bindout(1, icount);
        stmt.execute();
        stmt.next();
        if (icount == 0)
        {
            sendbuf = "<retcode>-1</retcode><message>用户无权限，或接口不存在。</message>";
            return;
        }

        // 4）根据接口名，获取接口的配置参数。
        // 从接口参数配置表T_INTERCFG中加载接口参数。
        string selectsql, colstr, bindin;
        stmt.prepare("select selectsql,colstr,bindin from T_INTERCFG where intername=:1");
        stmt.bindin(1, intername);        // 接口名。
        stmt.bindout(1, selectsql, 1000); // 接口SQL。
        stmt.bindout(2, colstr, 300);     // 输出列名。
        stmt.bindout(3, bindin, 300);     // 接口参数。
        stmt.execute();                   // 这里基本上不用判断返回值，出错的可能几乎没有。
        if (stmt.next() != 0)
        {
            sendbuf = "<retcode>-1</retcode><message>内部错误。</message>";
            return;
        }

        // http://192.168.174.132:8080?username=ty&passwd=typwd&intername=getzhobtmind3&
        // obtid=59287&begintime=20211024094318&endtime=20211024113920
        // SQL语句：   select obtid,to_char(ddatetime,'yyyymmddhh24miss'),t,p,u,wd,wf,r,vis from T_ZHOBTMIND
        //                     where obtid=:1 and ddatetime>=to_date(:2,'yyyymmddhh24miss') and ddatetime<=to_date(:3,'yyyymmddhh24miss')
        // colstr字段： obtid,ddatetime,t,p,u,wd,wf,r,vis
        // bindin字段：obtid,begintime,endtime

        // 5）准备查询数据的SQL语句。
        stmt.prepare(selectsql);

        //////////////////////////////////////////////////
        // 根据接口配置中的参数列表（bindin字段），从请求报文中解析出参数的值，绑定到查询数据的SQL语句中。
        // 拆分输入参数bindin。
        ccmdstr cmdstr;
        cmdstr.splittocmd(bindin, ",");

        // 声明用于存放输入参数的数组。
        vector<string> invalue;
        invalue.resize(cmdstr.size());

        // 从http的GET请求报文中解析出输入参数，绑定到sql中。
        for (int ii = 0; ii < cmdstr.size(); ii++)
        {
            getvalue(recvbuf, cmdstr[ii].c_str(), invalue[ii]);
            stmt.bindin(ii + 1, invalue[ii]);
        }
        //////////////////////////////////////////////////

        //////////////////////////////////////////////////
        // 绑定查询数据的SQL语句的输出变量。
        // 拆分colstr，可以得到结果集的字段数。
        cmdstr.splittocmd(colstr, ",");

        // 用于存放结果集的数组。
        vector<string> colvalue;
        colvalue.resize(cmdstr.size());

        // 把结果集绑定到colvalue数组。
        for (int ii = 0; ii < cmdstr.size(); ii++)
            stmt.bindout(ii + 1, colvalue[ii]);
        //////////////////////////////////////////////////

        if (stmt.execute() != 0)
        {
            logfile.write("stmt.execute() failed.\n%s\n%s\n", stmt.sql(), stmt.message());
            sformat(sendbuf, "<retcode>%d</retcode><message>%s</message>\n", stmt.rc(), stmt.message());
            return;
        }

        sendbuf = "<retcode>0</retcode><message>ok</message>\n";

        sendbuf = sendbuf + "<data>\n"; // xml内容开始的标签<data>。

        //////////////////////////////////////////////////
        // 获取结果集，每获取一条记录，拼接xml。
        while (true)
        {
            if (stmt.next() != 0)
                break; // 从结果集中取一条记录。

            // 拼接每个字段的xml。
            for (int ii = 0; ii < cmdstr.size(); ii++)
                sendbuf = sendbuf + sformat("<%s>%s</%s>", cmdstr[ii].c_str(), colvalue[ii].c_str(), cmdstr[ii].c_str());

            sendbuf = sendbuf + "<endl/>\n"; // 每行结束的标志。
        }
        //////////////////////////////////////////////////

        sendbuf = sendbuf + "</data>\n"; // xml内容结尾的标签</data>。

        logfile.write("intername=%s,count=%d\n", intername.c_str(), stmt.rpc());

        // 写接口调用日志表T_USERLOG，略去十万八千行代码。
    }
};

Worker worker;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("\n");
        printf("Using :./webserver logfile port\n\n");
        printf("Sample:./webserver /log/idc/webserver.log 5088\n\n");
        printf("        /home/lz6years/myproject/tools/bin/procctl 5 /home/lz6years/myproject/tools/bin/webserver /log/idc/webserver.log 5088\n\n");
        printf("基于HTTP协议的数据访问接口模块。\n");
        printf("logfile 本程序运行的日是志文件。\n");
        printf("port    服务端口，例如：80、8080。\n");

        return -1;
    }

    // 关闭全部的信号和输入输出。
    // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
    // 但请不要用 "kill -9 +进程号" 强行终止。
    closeioandsignal(); signal(SIGINT, EXIT); signal(SIGTERM, EXIT);

    // 打开日志文件。
    if (logfile.open(argv[1]) == false){
        printf("打开日志文件失败（%s）。\n", argv[1]); return -1;
    }

    thread t1(&Worker::recvWorker, &worker, atoi(argv[2]));
    thread t2(&Worker::worker, &worker, 1);
    thread t3(&Worker::worker, &worker, 2);
    thread t4(&Worker::worker, &worker, 3);
    thread t5(&Worker::sendWorker, &worker);

    logfile.write("已启动全部的线程。\n");

    while (true)
    {
        sleep(30);

        // 可以执行一些定时任务。
    }

    return 0;
}

// 初始化服务端的监听端口。
int initserver(const int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        logfile.write("socket(%d) failed.\n", port);
        return -1;
    }

    int opt = 1;
    unsigned int len = sizeof(opt);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, len);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        logfile.write("bind(%d) failed.\n", port);
        close(sock);
        return -1;
    }

    if (listen(sock, 5) != 0)
    {
        logfile.write("listen(%d) failed.\n", port);
        close(sock);
        return -1;
    }

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0) | O_NONBLOCK); // 把socket设置为非阻塞。

    return sock;
}

void EXIT(int sig)
{
    signal(sig, SIG_IGN);

    logfile.write("程序退出，sig=%d。\n\n", sig);

    write(worker.m_recvpipe[1], (char *)"o", 1); // 通知接收线程退出。

    usleep(500); // 让线程们有足够的时间退出。

    exit(0);
}

// 从GET请求中获取参数的值：strget-GET请求报文的内容；name-参数名；value-参数值；len-参数值的长度。
bool getvalue(const string &strget, const string &name, string &value)
{
    // http://192.168.150.128:8080/api?username=wucz&passwd=wuczpwd
    // GET /api?username=wucz&passwd=wuczpwd HTTP/1.1
    // Host: 192.168.150.128:8080
    // Connection: keep-alive
    // Upgrade-Insecure-Requests: 1
    // .......
    // logfile.write("strget=%s\n",strget.c_str());
    int startp = strget.find(name); // 在请求行中查找参数名的位置。
    logfile.write("fieldnamea=%s\n", name.c_str());
    if (startp == string::npos)
    {
        logfile.write("=%s= not found\n", name.c_str());
        // logfile.write("strget=%s\n",strget.c_str());
        return false;
    }


    logfile.write("fieldnameb=%s\n", name.c_str());
    int endp = strget.find("&", startp); // 从参数名的位置开始，查找&符号。
    if (endp == string::npos)
        endp = strget.find(" ", startp); // 如果是最后一个参数，没有找到&符号，那就查找空格。

    if (endp == string::npos)
        return false;

    // 从请求行中截取参数的值。
    // int itmplen=endp-startp-(name.length()+1);
    // if ( (len>0) && (len<itmplen) ) itmplen=len;
    value = strget.substr(startp + (name.length() + 1), endp - startp - (name.length() + 1));

    return true;
}
