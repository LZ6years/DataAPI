/*
        程序名: demo03.cpp   此程序用来实现网上银行通讯的客户端
*/
#include "_public.h"
using namespace idc;

ctcpclient tcpclient;     // tcp通讯的客户端。

string strsendbuffer;    // 发送报文的buffer。
string strrecvbuffer;    // 接收报文的buffer。

bool biz001();   // 登录。
bool biz002();   // 查询余额。
bool biz003();   // 转帐。

int main(int argc,char *argv[])
{
    if (argc!=3)
    {
      printf("Using:./demo03 ip port\n");
      printf("Example:./demo03 localhost 5005\n\n"); 
      return -1;
    }

    if (tcpclient.connect(argv[1],atoi(argv[2]))==false)
    {
        printf ("tcpclient.connect() failed.\n"); return -1;
    }

    biz001();  // 登录。
    
    biz002();   // 查询余额。

    biz003();   // 转帐。
    
}

bool biz001()   // 登录。
{
    strsendbuffer="<bizid>1</bizid><username>13922200001</username><password>123456</password>";
    if (tcpclient.write(strsendbuffer)==false)
    {
        printf("tcpclient.write() failed.\n"); return false;
    }
    cout << "发送：" << strsendbuffer << endl;

    if (tcpclient.read(strrecvbuffer)==false)
    {
        printf("tcpclient.read() failed.\n"); return false;
    }
    cout << "接收：" << strrecvbuffer << endl;

    return true;
}

bool biz002()   // 查询余额。
{
    strsendbuffer="<bizid>2</bizid><cardid>6262000000001</cardid>";
    if (tcpclient.write(strsendbuffer)==false)
    {
        printf("tcpclient.write() failed.\n"); return false;
    }
    cout << "发送：" << strsendbuffer << endl;

    if (tcpclient.read(strrecvbuffer)==false)
    {
        printf("tcpclient.read() failed.\n"); return false;
    }
    cout << "接收：" << strrecvbuffer << endl;

    return true;
}

bool biz003()   // 转帐。
{
    strsendbuffer="<bizid>3</bizid><cardid1>6262000000001</cardid1><cardid2>6262000000001</cardid2><je>100.8</je>";
    if (tcpclient.write(strsendbuffer)==false)
    {
        printf("tcpclient.write() failed.\n"); return false;
    }
    cout << "发送：" << strsendbuffer << endl;

    if (tcpclient.read(strrecvbuffer)==false)
    {
        printf("tcpclient.read() failed.\n"); return false;
    }
    cout << "接收：" << strrecvbuffer << endl;

    return true;
}