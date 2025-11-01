/*
    程序名：tcpgetfiles.cpp 用tcp来从服务端下载文件，下载到本地中
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
    char pathbak[256];    //客户端文件的备份位置，只有当ptype = 2时，才有用
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
ctcpclient tcpclient;
clogfile logfile;
cpactive pactive;

string strrecvbuffer;
string strsendbuffer;

void _help();
bool _tcpgetfiles();
bool recvfile(const string &filename,const string &mtime,const int filesize);  //发送文件的函数

//向服务器发送登录报文，将客户端的参数传递到服务端
bool login(const char* argv);

int main(int argc,char *argv[])
{
    signal(SIGINT,FAEXIT);signal(SIGTERM,FAEXIT);
    //1.给出帮助文档
    if(argc != 3){_help();return-1;}

    if(!logfile.open(argv[1])){
        printf("logfile.open(%s) failed\n",argv[1]);FAEXIT(0);
    }
    //解析参数中的xml内容
    if(!starg.init(argv[2])){
        printf("starg.init() failed\n");return -1;
    }

    pactive.addpinfo(starg.timeout,starg.pname);

    //2.将客户端与服务器连接
    if(!tcpclient.connect(starg.ip,starg.port)){
        logfile.write("tcpclient.connect(%s) failed\n",argv[2]);FAEXIT(0);
    }
    logfile.write("客户端%s与服务器连接成功\n",starg.ip);

    //登录服务端
    if(!login(argv[2])){
        logfile.write("login(%s) failed\n",argv[2]);
    }

    pactive.uptatime();
    //调用文件下载的主函数
    _tcpgetfiles();

    FAEXIT(0);    
}


void FAEXIT(int sig)
{
    logfile.write("客户端收到信号sig=%d,已退出。",sig);
    exit(0);
}

bool activetest()
{
    //向服务端发送心跳报文
    strsendbuffer = "<activetest>ok</activetest>";
    // xxxxxxxxxx logfile.write("发送：%s\n",strsendbuffer.c_str());
    if(!tcpclient.write(strsendbuffer))
    {
        logfile.write("tcpclient.write(%s) failed\n",strsendbuffer.c_str());FAEXIT(0);
    }
    if(!tcpclient.read(strrecvbuffer,60))
    {
        logfile.write("tcpclient.read(%s)",strsendbuffer.c_str());FAEXIT(0);
    }
    // xxxxxxxxxx logfile.write("读取：%s\n",strrecvbuffer.c_str());

    return true;
}


void _help()
{
    printf("\n");
    printf("Using:/home/lz6years/backend/myproject/tools/bin/tcpputfiles logfilename xmlbuffer\n\n");

printf("Sample:/home/lz6years/backend/myproject/tools/bin/procctl 20 /home/lz6years/backend/myproject/tools/bin/tcpgetfiles /log/idc/tcpgetfiles_surfdata.log "\
              "\"<ip>192.168.171.121</ip><port>5005</port>"\
              "<clientpath>/tmp/client</clientpath><ptype>2</ptype>"\
              "<pathbak>/tmp/bak</pathbak>"\
              "<srvpath>/tmp/server</srvpath>"\
              "<andchild>true</andchild><matchname>*.xml,*.txt</matchname><timetvl>10</timetvl>"\
              "<timeout>50</timeout><pname>tcpputfiles_surfdata</pname>\"\n\n");

    printf("本程序是数据中心的公共功能模块，采用tcp协议把文件上传给服务端。\n");
    printf("logfilename   本程序运行的日志文件。\n");
    printf("xmlbuffer     本程序运行的参数，如下：\n");
    printf("ip            服务端的IP地址。\n");
    printf("port          服务端的端口。\n");
    printf("ptype         文件上传成功后的处理方式：1-删除文件；2-移动到备份目录。\n");
    printf("clientpath    本地文件存放的根目录。\n");
    printf("pathbak 文件成功上传后，本地文件备份的根目录，当ptype==2时有效。\n");
    printf("andchild      是否上传clientpath目录下各级子目录的文件，true-是；false-否，缺省为false。\n");
    printf("matchname     待上传文件名的匹配规则，如\"*.TXT,*.XML\"\n");
    printf("srvpath       服务端文件存放的根目录。\n");
    printf("timetvl       扫描本地目录文件的时间间隔，单位：秒，取值在1-30之间。\n");
    printf("timeout       本程序的超时时间，单位：秒，视文件大小和网络带宽而定，建议设置50以上。\n");
    printf("pname         进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
}

bool st_arg::init(const string &args)
{
    memset(this,0,sizeof(struct st_arg));

    // //解析其中的参数(由于其为上传客户端，其lclienttype固定为1)
    // getxmlbuffer(args,"clienttype",clienttype);
    // if(clienttype != 1 && clienttype != 2){
    //     logfile.write("clienttype is error(only 1,2)\n");return false;
    // }

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

bool login(const char* argv)
{
    //向服务端发送参数内容
    strsendbuffer = sformat("<clienttype>2</clienttype>%s",argv);
    // xxxxxxxxxx logfile.write("发送：%s\n",strsendbuffer.c_str());
    if(!tcpclient.write(strsendbuffer)) return false;

    if(!tcpclient.read(strrecvbuffer,20)) return false; // 接收服务端的回应报文。
    // xxxxxxxxxx logfile.write("接收：%s\n",strrecvbuffer.c_str());

    logfile.write("登录(%s:%d)成功。\n",starg.ip,starg.port); 

    return true;
}

void st_arg::show()
{
    printf("\nclienttype=%d,ip=%s,port=%d,clientpath=%s,"\
            "ptype=%d,pathbak=%s,andchild=%d,matchname=%s,"\
            "srvpath=%s,timetvl=%d,timeout=%d,pname=%s\n",clienttype,
            ip,port,clientpath,ptype,pathbak,andchild,matchname,srvpath,timetvl,timeout,pname);
}

bool _tcpgetfiles()
{
    while(true)
    {
        pactive.uptatime();
        if(!tcpclient.read(strrecvbuffer,starg.timetvl + 10))
        {
            logfile.write("tcpclient.read(%s) failed\n",strrecvbuffer.c_str());return false;
        }
        // xxxxxxxxxxlogfile.write("接受：%s\n",strrecvbuffer.c_str());

        //处理心跳的报文
        if(strrecvbuffer == "<activetest>ok</activetest>")
        {
            strsendbuffer = "ok";
            // xxxxxxxxxxlogfile.write("发送：%s\n",strsendbuffer.c_str());
            if(!tcpclient.write(strsendbuffer))
            {
                logfile.write("tcpclient.write(%s) failed\n",strsendbuffer.c_str());return false;
            }
        }

        pactive.uptatime();
        if(strrecvbuffer.find("<filename>") != string::npos)
        {
            //首先，获取文件信息
            string serverfilename,mtime;
            int filesize = 0;
            getxmlbuffer(strrecvbuffer,"filename",serverfilename);
            getxmlbuffer(strrecvbuffer,"mtime",mtime);
            getxmlbuffer(strrecvbuffer,"size",filesize);

            //接受文件的内容
            logfile.write("recv %s 下载中...",serverfilename.c_str());
            string clientfilename = serverfilename;   //得到用户端文件的全路径
            replacestr(clientfilename,starg.srvpath,starg.clientpath,false);
            if(recvfile(clientfilename,mtime,filesize))
            {
                logfile << " ok\n";
                //接受文件内容后，回复确认报文
                strsendbuffer = sformat("<filename>%s</filename><result>ok</result>",serverfilename.c_str());   
            }
            else{
                logfile << "failed\n";
                //接受文件内容后，回复确认报文
                strsendbuffer = sformat("<filename>%s</filename><result>failed</result>",serverfilename.c_str());
            }

            // 把确认报文返回给对端。
            // xxxxxxxxxxlogfile.write("接受：%s\n",strsendbuffer.c_str());
            if (!tcpclient.write(strsendbuffer))
            {
                logfile.write("tcpclient.write() failed.\n"); return false;
            }
        }
    }
    
    return true;
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
        if(!tcpclient.read(buffer,onread)) return false;

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
