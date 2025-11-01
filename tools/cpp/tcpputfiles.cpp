/*
    程序名：tcpputfiles.cpp 用tcp来上传文件，上传到服务器fileserver中
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
    char clientpathbak[256];    // 服务端文件的备份位置，只有当ptype = 2时，才有用
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

bool activetest();
void _help();
bool _tcpputfiles(bool &bcontinue);  //传输文件的函数
bool ackmessage(const string &strrecvbuffer); //处理服务端的确认报文
bool sendfile(const string &filename,const int filesize);  //发送文件的函数

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
        logfile.write("starg.init() failed\n"); return -1;
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

    bool bcontinue=true;   // 如果调用_tcpputfiles()发送了文件，bcontinue为true，否则为false。
    pactive.uptatime();

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
    if(!tcpclient.read(strrecvbuffer,10))
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

    printf("Sample:/home/lz6years/backend/myproject/tools/bin/procctl 20 /home/lz6years/backend/myproject/tools/bin/tcpputfiles /log/idc/tcpputfiles_surfdata.log "\
              "\"<ip>192.168.171.121</ip><port>5005</port>"\
              "<clientpath>/tmp/client</clientpath><ptype>2</ptype>"\
              "<clientpathbak>/tmp/bak</clientpathbak>"\
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
    printf("clientpathbak 文件成功上传后，本地文件备份的根目录，当ptype==2时有效。\n");
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
        getxmlbuffer(args, "clientpathbak", clientpathbak, 255);
        if (strlen(clientpathbak) == 0)
        {
            logfile.write("clientpathbak is none\n");
            return false;
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
    strsendbuffer = sformat("<clienttype>1</clienttype>%s",argv);
    // xxxxxxxxxx logfile.write("发送：%s\n",strsendbuffer.c_str());
    if(!tcpclient.write(strsendbuffer)) return false;

    if(!tcpclient.read(strrecvbuffer,10)) return false; // 接收服务端的回应报文。
    // xxxxxxxxxx logfile.write("接收：%s\n",strrecvbuffer.c_str());

    logfile.write("登录(%s:%d)成功。\n",starg.ip,starg.port); 

    return true;
}

void st_arg::show()
{
    printf("\nclienttype=%d,ip=%s,port=%d,clientpath=%s,"\
            "ptype=%d,clientpathbak=%s,andchild=%d,matchname=%s,"\
            "srvpath=%s,timetvl=%d,timeout=%d,pname=%s\n",clienttype,
            ip,port,clientpath,ptype,clientpathbak,andchild,matchname,srvpath,timetvl,timeout,pname);
}

bool _tcpputfiles(bool &bcontinue)  //传输文件的函数
{
    //首先发送，对应文件的文件名，文件时间，文件大小
    cdir dir;

    bcontinue = false;
    if(!dir.opendir(starg.clientpath,starg.matchname,10000,starg.andchild)){
        logfile.write("dir.opendir(%s) failed\n",starg.clientpath);return false;
    }

    //将文件传输，改为异步传输
    int delayed=0;        // 未收到对端确认报文的文件数量，发送了一个文件就加1，接收到了一个回应就减1。

    //获取每一个文件
    while (dir.readdir()){
        pactive.uptatime();

        //拼接发送的报文信息
        strsendbuffer = sformat("<filename>%s</filename><mtime>%s</mtime><size>%d</size>",dir.m_ffilename.c_str(),dir.m_mtime.c_str(),dir.m_filesize);

        bcontinue = true;
        //将报文发送给服务端
        // xxxxxxxxxxlogfile.write("发送：%s\n",strsendbuffer.c_str());
        if(!tcpclient.write(strsendbuffer)){
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
            logfile << "failed\n";tcpclient.close();return false;
        }
        //实时处理，方式缓存区满了
        while(delayed > 0)
        {
            //接受到服务端的确认报文
            if(!tcpclient.read(strrecvbuffer,-1)){break;}
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
        if(!tcpclient.read(strrecvbuffer,10)){
            logfile.write("tcpclient.read(%s) failed\n",strrecvbuffer.c_str());break;
        }
        // xxxxxxxxxx logfile.write("接受：%s ok\n",strrecvbuffer.c_str());
        delayed--;
        //处理服务端的确认报文
        ackmessage(strrecvbuffer);
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
        if(!tcpclient.write(buffer,onread)) return false;

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
        replacestr(bakfilename,starg.clientpath,starg.clientpathbak,false);
        if (!renamefile(filename, bakfilename))
        { logfile.write("renamefile(%s,%s) failed.\n",filename.c_str(),bakfilename.c_str()); return false; }
        // xxxxxxxxxxlogfile.write("rename(%s) success\n",filename.c_str());
    }

    return true;
}