/*
        程序名: demo04.cpp   此程序用来实现网上银行通讯的服务端
*/
#include "_public.h"
using namespace idc;

clogfile logfile;     //日志文件对象
ctcpserver tcpserver; //tcp连接对象

string strrecvbuffer;
string strsendbuffer;

void FAEXIT(int sig);
void CHEXIT(int sig);

void bizmain();    // 业务处理主函数。
void biz001();     // 业务1：登录
void biz002();     // 业务2：余额查询
void biz003();     // 业务3：转账

int main(int argc,char *argv[])
{

    signal(SIGINT,FAEXIT);signal(SIGTERM,FAEXIT);
    signal(SIGCHLD,SIG_IGN);   //对于子进程退出时忽略防止僵尸进程
    //1.给出帮助文档
    if(argc != 3)
    {
        cout << "\n";
        cout << "Using:./demo04 port logfilepath\n";
        cout << "Example:./demo04 5005 /home/lz6years/backend/log/idc/demo04.log\n";

        cout << "port:服务器所打开的端口号\n";
        cout << "logfilepath:记录的日志文件所在的位置";
    }

    //打开log文件
    if(!logfile.open(argv[2])){
        printf("logfile.open(%s) is failed\n",argv[2]);FAEXIT(0);
    }

    //2.对服务端初始化
    if(!tcpserver.initserver(atoi(argv[1])))
    {
        logfile.write("tcpserver.initserver(%s) isfailed",argv[1]);FAEXIT(0);
    }

    while (true)
    { 
        //3.打开服务端的接听接口
        if(!tcpserver.accept())
        {
            logfile.write("tcpserver.accept() is failed");FAEXIT(0);
        }

        logfile.write("客户端(%s)连接成功！\n",tcpserver.getip());
        //4.连接到客户端后，fork一个子进程来处理
        if(fork() > 0){tcpserver.closeclient();continue;}        //5.父进程关闭client接口，继续回去接听

        //6.子进程关闭listen接口，开始处理业务
        tcpserver.closelisten();

        signal(SIGINT,CHEXIT);signal(SIGTERM,CHEXIT);  //设置子进程退出函数

        //处理业务代码
        while (true)
        {
            // 子进程与客户端进行通讯，处理业务。
            if (tcpserver.read(strrecvbuffer)==false)
            {
                logfile.write("tcpserver.read() failed.\n"); CHEXIT(0);
            }
            logfile.write("接收：%s\n",strrecvbuffer.c_str());

            bizmain();    // 业务处理主函数。

            if (tcpserver.write(strsendbuffer)==false)
            {
                logfile.write("tcpserver.send() failed.\n"); CHEXIT(0);
            }
            logfile.write("发送：%s\n",strsendbuffer.c_str());
        }

        CHEXIT(0);
    }
       return 0;
}


void CHEXIT(int sig)
{
    //0的信号也会传给自己，需要下一次忽略此信号
    signal(SIGINT,SIG_IGN);signal(SIGTERM,SIG_IGN);

    //写入日志
    logfile.write("子进程(%d)收到sig = %d的信号，已退出\n",getpid(),sig);

    //关闭listen接口
    tcpserver.closeclient();

    exit(0);
}

void FAEXIT(int sig)
{
    //0的信号也会传给自己，需要下一次忽略此信号
    signal(SIGINT,SIG_IGN);signal(SIGTERM,SIG_IGN);

    //写入日志
    logfile.write("父进程(%d)收到sig = %d的信号，通知子进程退出\n",getpid(),sig);

    //关闭listen接口
    tcpserver.closelisten();

    //父进程退出先通知子进程退出
    kill(0,15);

    exit(0);
}


void bizmain()    // 业务处理主函数。
{
    //接受到报文中的内容 bizid:(1登录)(2查询余额)(3转账)
    int bizid;
    getxmlbuffer(strrecvbuffer,"bizid",bizid);

    logfile.write("业务代码: %d\n", bizid);

    switch (bizid)
    {
    case 1:{ biz001(); break;}  //登录
    case 2:{ biz002(); break;}  //查询余额
    case 3:{ biz003(); break;}  //转账
    default:
        strsendbuffer="<retcode>9</retcode><message>业务不存在</message>";
        break;
    }
    
}

void biz001()     // 业务1：登录
{
    //对方报文<bizid>1</bizid><username></username><password></password>
    //解析出对方内容
    string username,password;
    getxmlbuffer(strrecvbuffer,"username",username);
    getxmlbuffer(strrecvbuffer,"password",password);

    // <username>13922200001</username><password>123456</password>

    logfile.write("接受到的username: %s, password: %s\n", username.c_str(), password.c_str());

    if (username == "13922200001" && password == "123456")
        strsendbuffer = "<retcode>0</retcode><message>成功。</message>";
    else
        strsendbuffer="<retcode>-1</retcode><message>用户名或密码不正确。</message>";
}

void biz002()     // 业务2：余额查询
{
    //对方报文<bizid></bizid><cardid></cardid>
    string cardid;
    getxmlbuffer(strrecvbuffer,"cardid",cardid);  // 获取卡号。

    // 假装操作了数据库，得到了卡的余额。

    strsendbuffer="<retcode>0</retcode><ye>128.83</ye>";
}

void biz003()     // 业务3：转账
{
    string cardid1,cardid2;
    getxmlbuffer(strrecvbuffer,"cardid1",cardid1);
    getxmlbuffer(strrecvbuffer,"cardid2",cardid2);
    double je;
    getxmlbuffer(strrecvbuffer,"je",je);

    // 操作数据库,得到了第一个账户的余额
    logfile.write("接受到的转账金额为: %lf\n", je);

    if ( je<100 )
        strsendbuffer="<retcode>0</retcode><message>成功。</message>";
    else
        strsendbuffer="<retcode>-1</retcode><message>余额不足。</message>";

    // 假装操作了数据库，更新了两个账户的金额，完成了转帐操作。
}