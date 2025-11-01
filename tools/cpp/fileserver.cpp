/*
    程序名：fileserver.cpp 用于自定义的tcp连接的服务器，上传与下载文件
*/

#include "_public.h"
using namespace idc;

//程序运行的参数结构体
struct st_arg
{
    int clienttype;             //客户端类型 ，1-上传文件；2-下载文件，本程序固定填1。
    char ip[31];                //服务器的ip地址
    int port;                   //服务器的端口
    char clientpath[256];       //客户端的根目录
    int ptype;                  //文件上传成功后，本地文件的处理方式 1：删除文件 2：备份文件
    char pathbak[256];          //客户端文件的备份位置，只有当ptype = 2时，才有用
    bool andchild;              //是否上传根目录下子文件夹的文件
    char matchname[256];        //上传文件夹的匹配规则
    char srvpath[256];          //服务器文件的存放位置
    int timetvl;                //扫描本地目录文件的时间间隔
    int timeout;                //进程心跳的超时时间
    char pname[51];             //进程名，建议用“tcpputfiles_后缀”的方式

    bool init(const string& args);
    void show();
}starg;

void FAEXIT(int sig);
void CHEXIT(int sig);

ctcpserver tcpserver;
clogfile logfile;
cpactive pactive;

string strrecvbuffer;  //接受报文的buffer
string strsendbuffer;  //传出报文的buffer

bool clientlogin();     //客户端登录的函数
bool recvfilemain();    //接受文件的主函数
bool sendfilemain();    //发送文件的主函数
bool recvfile(const string &filename,const string &mtime,const int filesize);

bool _tcpputfiles(bool &bcontinue);  //传输文件的函数
bool activetest();
bool ackmessage(const string &strrecvbuffer); //处理服务端的确认报文
bool sendfile(const string &filename,const int filesize);  //发送文件的函数


int main(int argc,char *argv[])
{
    signal(SIGINT,FAEXIT);signal(SIGTERM,FAEXIT);
    signal(SIGCHLD,SIG_IGN);   //对于子进程退出时忽略防止僵尸进程
    //1.给出帮助文档
    if(argc != 3)
    {
        cout << "\n";
        cout << "Using:/home/lz6years/backend/myproject/tools/cpp/fileserver port logfilepath\n";
        cout << "Example:/home/lz6years/backend/myproject/tools/cpp/fileserver 5005 /log/idc/fileserver.log\n";

        cout << "port:服务器所打开的端口号\n";
        cout << "logfilepath:记录的日志文件所在的位置\n\n";
        return -1;
    }

    if(!logfile.open(argv[2])){
        printf("logfile.open(%s) failed\n",argv[2]);FAEXIT(0);
    }    
    //2.初始化tcpserver
    if(!tcpserver.initserver(atoi(argv[1]))){
        logfile.write("tcpserver.initserver(%s) failed\n",argv[1]);FAEXIT(0);
    }

    while (true)
    {
        if(!tcpserver.accept()){
            logfile.write("tcpserver.accept() failed\n");FAEXIT(0);
        }
        //3.登录成功
        logfile.write("客户端(%s) 连接成功\n",tcpserver.getip());

        if(fork() > 0){tcpserver.closeclient(); continue;}

        //以下为子进程的处理内容
        signal(SIGINT,CHEXIT);signal(SIGTERM,CHEXIT);

        //关闭listen的socket
        tcpserver.closelisten();

        //先接受客户端的登录报文，进行登录操作
        if(!clientlogin()){ logfile.write("客户端登录失败！\n");CHEXIT(0);}

        pactive.addpinfo(starg.timeout,starg.pname);
        //starg.show();

        //接受文件的主函数
        if(starg.clienttype == 1) recvfilemain();
        
        //发送文件的主函数
        if(starg.clienttype == 2) sendfilemain();
    
        CHEXIT(0);  //子进程退出（需要在while中）
    }
    
}


void FAEXIT(int sig)
{
    signal(SIGINT,SIG_IGN);signal(SIGTERM,SIG_IGN);

    logfile.write("收到sig=%d信号，父进程退出\n",sig);

    tcpserver.closelisten();

    kill(0,15);

    exit(0);
}

void CHEXIT(int sig)
{
    signal(SIGINT,SIG_IGN);signal(SIGTERM,SIG_IGN);

    logfile.write("收到sig=%d信号，子进程退出\n",sig);

    tcpserver.closeclient();

    exit(0);
}


bool recvfilemain()
{
    while(true)
    {
        pactive.uptatime();
        if(!tcpserver.read(strrecvbuffer,starg.timetvl + 10))
        {
            logfile.write("tcpserver.read(%s) failed\n",strrecvbuffer.c_str());return false;
        }
        // xxxxxxxxxxlogfile.write("接受：%s\n",strrecvbuffer.c_str());

        //处理心跳的报文
        if(strrecvbuffer == "<activetest>ok</activetest>")
        {
            strsendbuffer = "ok";
            // xxxxxxxxxxlogfile.write("发送：%s\n",strsendbuffer.c_str());
            if(!tcpserver.write(strsendbuffer))
            {
                logfile.write("tcpserver.write(%s) failed\n",strsendbuffer.c_str());return false;
            }
        }

        if(strrecvbuffer.find("<filename>") != string::npos)
        {
            //首先，获取文件信息
            string clientfilename,mtime;
            int filesize = 0;
            getxmlbuffer(strrecvbuffer,"filename",clientfilename);
            getxmlbuffer(strrecvbuffer,"mtime",mtime);
            getxmlbuffer(strrecvbuffer,"size",filesize);

            pactive.uptatime();
            //接受文件的内容
            logfile.write("recv %s 下载中...",clientfilename.c_str());
            string serverfilename = clientfilename;   //得到服务器端文件的全路径
            replacestr(serverfilename,starg.clientpath,starg.srvpath,false);
            if(recvfile(serverfilename,mtime,filesize))
            {
                logfile << " ok\n";
                //接受文件内容后，回复确认报文
                strsendbuffer = sformat("<filename>%s</filename><result>ok</result>",clientfilename.c_str());   
            }
            else{
                logfile << "failed\n";
                //接受文件内容后，回复确认报文
                strsendbuffer = sformat("<filename>%s</filename><result>failed</result>",clientfilename.c_str());
            }

            // 把确认报文返回给对端。
            // xxxxxxxxxxlogfile.write("接受：%s\n",strsendbuffer.c_str());
            if (!tcpserver.write(strsendbuffer))
            {
                logfile.write("tcpserver.write() failed.\n"); return false;
            }
        }
    }
    
    return true;
}

bool sendfilemain()
{
    bool bcontinue=true;   // 如果调用_tcpputfiles()发送了文件，bcontinue为true，否则为false。

    //3.与服务端通讯
    while (true)
    {
        pactive.uptatime();
        //向服务端发送文件
        if(!_tcpputfiles(bcontinue)){ logfile.write("_tcpputfiles() failed.\n"); FAEXIT(-1); }

        if(!bcontinue)
        {
            sleep(starg.timetvl);

            //发送心跳报文
            if(!activetest()) break; //表示当心跳结束，就断开与服务器的通讯
        }
    } 

    return true;
}

bool sendfile(const string &filename,const int filesize)  //发送文件的函数
{
    char buffer[1024];   //用来存储一次tcp连接接受到的报文内容
    int totalbytes = 0;  //用来存储接受到的字节数
    int onread = 0;      //用来得到此次需要读取的字节数
    cifile ifile;        //ofile对象

    //打开文件
    if(!ifile.open(filename,ios::binary | ios::in)){  // 注意：这里必须指定ios::in,即使时infil,也要指定
        logfile.write("ifile.open(%s) failed\n",filename.c_str());return false;
    }

    while (true)
    {
        pactive.uptatime();

        //将缓存区清空
        memset(buffer,0,sizeof buffer);

        //计算这次所需要读取的字节数
        if(filesize - totalbytes > 1024) onread = 1024;
        else onread = filesize - totalbytes;

        //将buffer的内容输出到文件中
        ifile.read(buffer,onread);

        //用tcp将其内容读取到buffer中
        if(!tcpserver.write(buffer,onread)) return false;

        //更新totalbytes，并根据字节数看是否读取完毕
        totalbytes += onread;

        if(totalbytes == filesize) break;
    }
    
    return true;
}

bool ackmessage(const string &strrecvbuffer) //处理服务端的确认报文
{
    // <filename>/tmp/client/2.xml</filename><result>ok</result>
    //解析确认报文的内容
    string filename;
    string result;
    getxmlbuffer(strrecvbuffer,"filename",filename);
    getxmlbuffer(strrecvbuffer,"result",result);

    if(result != "ok") return true; //若回应报文不是ok,则需要将文件重传，这里不处理

    //下面为回应报文为ok的处理
    if(starg.ptype == 1)
    {   
        //删除本地文件
        if(remove(filename.c_str()) != 0){logfile.write("remove(%s) failed.\n",filename.c_str()); return false;}
        // xxxxxxxxxxlogfile.write("remove(%s) success\n",filename.c_str());
    }
    if(starg.ptype == 2)
    {
        //将本地文件拷贝到bak目录下
        string bakfilename = filename;
        replacestr(bakfilename,starg.srvpath,starg.pathbak,false);
        if(!renamefile(filename,bakfilename)) 
        { logfile.write("renamefile(%s,%s) failed.\n",filename.c_str(),bakfilename.c_str()); return false; }
        // xxxxxxxxxxlogfile.write("rename(%s) success\n",filename.c_str());
    }

    return true;
}

bool _tcpputfiles(bool &bcontinue)  //传输文件的函数
{
    //首先发送，对应文件的文件名，文件时间，文件大小
    cdir dir;

    bcontinue = false;
    if(!dir.opendir(starg.srvpath,starg.matchname,10000,starg.andchild)){
        logfile.write("dir.opendir(%s) failed\n",starg.srvpath);return false;
    }

    //将文件传输，改为异步传输
    int delayed=0;        // 未收到对端确认报文的文件数量，发送了一个文件就加1，接收到了一个回应就减1。

    //获取每一个文件
    while (dir.readdir())
    {
        pactive.uptatime();

        //拼接发送的报文信息
        strsendbuffer = sformat("<filename>%s</filename><mtime>%s</mtime><size>%d</size>",dir.m_ffilename.c_str(),dir.m_mtime.c_str(),dir.m_filesize);

        bcontinue = true;
        //将报文发送给服务端
        // xxxxxxxxxxlogfile.write("发送：%s\n",strsendbuffer.c_str());
        if(!tcpserver.write(strsendbuffer)){
            logfile.write("tcpclient.write(%s) failed\n",strsendbuffer.c_str());return false;
        }

        //传输文件内容
        logfile.write("send %s 上传中...",dir.m_ffilename.c_str());
        if(sendfile(dir.m_ffilename,dir.m_filesize))
        {
            logfile << " ok\n"; delayed++; //传输文件成功，将delayed++
        }
        else
        {   
            //发送失败，就是网络的原因，一旦有这个原因，就没必要发送了
            logfile << "failed\n";tcpserver.closeclient();return false;
        }
        //实时处理，方式缓存区满了
        while(delayed > 0)
        {
            //接受到服务端的确认报文
            if(!tcpserver.read(strrecvbuffer,-1)){break;}
            // xxxxxxxxxxlogfile.write("接受：%s ok\n",strrecvbuffer.c_str());
            delayed--;
            //处理服务端的确认报文
            ackmessage(strrecvbuffer);
        }
    }

    pactive.uptatime();
    //对最后上面中还没有传输过来的文件，作处理
    while(delayed > 0)
    {
        //接受到服务端的确认报文
        if(!tcpserver.read(strrecvbuffer,10)){
            logfile.write("tcpclient.read(%s) failed\n",strrecvbuffer.c_str());break;
        }
        // xxxxxxxxxx logfile.write("接受：%s ok\n",strrecvbuffer.c_str());
        delayed--;
        //处理服务端的确认报文
        ackmessage(strrecvbuffer);
    }
    
    
    return true;
}

bool clientlogin()     //客户端登录的函数
{
    if(!tcpserver.read(strrecvbuffer,10)){return false;}

    //对读到的内容进行解析
    starg.init(strrecvbuffer);

    if(starg.clienttype != 1 && starg.clienttype != 2)
        strsendbuffer = "failed";
    else
        strsendbuffer = "ok";

    if(!tcpserver.write(strsendbuffer)){return false;}

    logfile.write("%s login %s.\n",tcpserver.getip(),strsendbuffer.c_str());
    return true;
}

bool st_arg::init(const string &args)
{
    memset(this,0,sizeof(struct st_arg));

    //解析其中的参数
    getxmlbuffer(args,"clienttype",clienttype);
    if(clienttype != 1 && clienttype != 2){
        logfile.write("clienttype is error(only 1,2)\n");return false;
    }

    //解析其中的参数
    getxmlbuffer(args,"ip",ip,30);
    if(strlen(ip) == 0){
        logfile.write("ip is error\n");return false;
    }

    //解析其中的参数
    getxmlbuffer(args,"port",port);
    if(port == 0){
        logfile.write("port is error\n");return false;
    }

    //解析其中的参数
    getxmlbuffer(args,"clientpath",clientpath,255);
    if(strlen(clientpath) == 0){
        logfile.write("clientpath is none\n");return false;
    }

    //解析其中的参数
    getxmlbuffer(args,"ptype",ptype);
    if(ptype != 1 && ptype != 2){
        logfile.write("ptype is error(only 1,2)\n");return false;
    }

    if(ptype == 2){
        getxmlbuffer(args,"pathbak",pathbak,255);
        if(strlen(pathbak) == 0){
             logfile.write("pathbak is none\n");return false;
        }
    }
    
    //解析其中的参数
    if(!getxmlbuffer(args,"andchild",andchild))
    {
        andchild = false;  //默认情况下，andchild的值为false
    }

    //解析其中的参数
    getxmlbuffer(args,"matchname",matchname,255);
    if(strlen(matchname) == 0){
        logfile.write("matchname is none\n");return false;
    }

    //解析其中的参数
    getxmlbuffer(args,"srvpath",srvpath,255);
    if(strlen(srvpath) == 0){
        logfile.write("srvpath is none\n");return false;
    }

    //解析其中的参数
    getxmlbuffer(args,"timetvl",timetvl);
    if(timetvl > 30){timetvl = 30;}  //若时长 > 30 则，将其时间定位30

    //解析其中的参数
    getxmlbuffer(args,"timeout",timeout);
    if(timeout == 0){
        logfile.write("timeout is error\n");return false;
    }
    if(timeout < timetvl){
        logfile.write("timeout < timetvl error!\n");return false;
    }

    //解析其中的参数
    getxmlbuffer(args,"pname",pname,50);
    if(strlen(pname) == 0){
        strncpy(pname,sformat("tcpputfiles_%d",getpid()).c_str(),50);
    }

    return true;
}

void st_arg::show()
{
    printf("\nclienttype=%d,ip=%s,port=%d,clientpath=%s,"\
            "ptype=%d,pathbak=%s,andchild=%d,matchname=%s,"\
            "srvpath=%s,timetvl=%d,timeout=%d,pname=%s\n",clienttype,
            ip,port,clientpath,ptype,pathbak,andchild,matchname,srvpath,timetvl,timeout,pname);
}

bool recvfile(const string &filename,const string &mtime,const int filesize)
{
    char buffer[1024];   //用来存储一次tcp连接接受到的报文内容
    int totalbytes = 0;  //用来存储接受到的字节数
    int onread = 0;      //用来得到此次需要读取的字节数
    cofile ofile;        //ofile对象

    //打开文件
    if(!ofile.open(filename,true,ios::binary|ios::out,true)){
        logfile.write("ofile.open(%s) failed\n",filename.c_str());return false;
    }

    while (true)
    {
        pactive.uptatime();

        //将缓存区清空
        memset(buffer,0,sizeof buffer);

        //计算这次所需要读取的字节数
        if(filesize - totalbytes > 1024) onread = 1024;
        else onread = filesize - totalbytes;

        //用tcp将其内容读取到buffer中
        if(!tcpserver.read(buffer,onread)) return false;

        //将buffer的内容输出到文件中
        ofile.write(buffer,onread);

        //更新totalbytes，并根据字节数看是否读取完毕
        totalbytes += onread;

        if(totalbytes == filesize) break;
    }
    
    ofile.closeandrename();

    //设置文件时间，用对端文件的时间，用当前端的时间没有意义
    setmtime(filename,mtime);
    return true;
}

bool activetest()
{
    //向服务端发送心跳报文
    strsendbuffer = "<activetest>ok</activetest>";
    // xxxxxxxxxx logfile.write("发送：%s\n",strsendbuffer.c_str());
    if(!tcpserver.write(strsendbuffer))
    {
        logfile.write("tcpclient.write(%s) failed\n",strsendbuffer.c_str());
        return false;
    }

    if(!tcpserver.read(strrecvbuffer,10))
    {
        logfile.write("tcpclient.read(%s)",strsendbuffer.c_str());
        return false;
    }
    // xxxxxxxxxx logfile.write("读取：%s\n",strrecvbuffer.c_str());

    return true;
}