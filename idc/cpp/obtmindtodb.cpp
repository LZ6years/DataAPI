/*
    obtcodetodb.cpp,本程序将全国气象参数文件的值存入到数据库中
*/
#include "idcapp.h"
using namespace idc;

clogfile logfile;
connection conn;
cpactive pactive;

void EXIT(int sig);
void _help(); //给出帮助文档
bool _obtmindtodb(const char* pathname,const char* connstr,const char* charset);

int main(int argc,char *argv[])
{

    if(argc != 5){_help();return -1;}
    //注册退出时需要的信号
    //closeioandsignal(true);
    signal(SIGTERM,EXIT);signal(SIGINT,EXIT);

    //打开日志文件
    if (logfile.open(argv[4])==false)
    {
        printf("打开日志文件失败(%s)。\n",argv[4]); return -1;
    }

    pactive.addpinfo(30,"obtmindtodb");

    _obtmindtodb(argv[1],argv[2],argv[3]);

    return 0;
}
void EXIT(int sig)
{
    logfile.write("收到%d的信号,已退出。\n",sig);

    exit(0);
}
void _help() //给出帮助文档
{
    printf("\n");
    printf("Using:./obtmindtodb pathname connstr charset logfile\n");

    printf("Example:/home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/idc/bin/obtmindtodb /idcdata/surfdata "\
              "\"idc/root@snorcl11g_121\" \"Simplified Chinese_China.AL32UTF8\" /log/idc/obtmindtodb.log\n\n");

    printf("本程序用于把全国气象观测数据文件入库到T_ZHOBTMIND表中，支持xml和csv两种文件格式，数据只插入，不更新。\n");
    printf("pathname 全国气象观测数据文件存放的目录。\n");
    printf("connstr  数据库连接参数：username/password@tnsname\n");
    printf("charset  数据库的字符集。\n");
    printf("logfile  本程序运行的日志文件名。\n");
    printf("程序每10秒运行一次，由procctl调度。\n\n\n");
}

bool _obtmindtodb(const char* pathname,const char* connstr,const char* charset)
{
    cdir dir;
    //打开文件夹
    if(!dir.opendir(pathname,"*.xml,*.csv"))
    {
        logfile.write("dir.opendir(%s) failed\n",pathname);EXIT(-1);
    }

    CZHOBTMIND ZHOBTMIND(conn,logfile);  // 操作气象观测数据表的对象。
    
    //读取其中的一个文件名
    while (dir.readdir())
    {
        //查看数据库连接是否联上
        if(!conn.isopen())
        {
            //没连上，则连接数据库  注意：返回0表示连接上了
            if(conn.connecttodb(connstr,charset) != 0)
            {
                logfile.write("connect to dababase failed\n");EXIT(-1);
            }
            logfile.write("connect to database ok.\n");
        }

        cifile ifile;
        //打开文件
        if(!ifile.open(dir.m_ffilename))
        {
            logfile.write("ifile.open(%s) failed\n",dir.m_filename.c_str());EXIT(-1);
        }

        int inscount = 0;        //成功插入的总数         
        int totalcount = 0;      //文件总记录数
        ctimer timer;            //记录插入的时间

        string buffer;  //用来存取得到的每一行内容

        bool bisxml = matchstr(dir.m_filename,"*.xml");
        if(!bisxml) ifile.readline(buffer);  //若为csv文件，则先读取一行跳过标题

        //将文件中的内容读取出来，然后存到结构体中，保存到数据库中
        while (true)
        {
            if(bisxml)
            {
                if(!ifile.readline(buffer,"<endl/>")) break;
            }
            else
            {
                if(!ifile.readline(buffer)) break;
            }

            totalcount++;

            ZHOBTMIND.splitbuffer(buffer,bisxml); //将buffer中的内容，拆分到其中

            //执行sql操作，将数据插入  注意：其返回值为0时，表示插入成功
            if(ZHOBTMIND.inserttable()) inscount++;
        }

        //关闭文件和提交事务
        ifile.closeandremove();
        conn.commit();
        logfile.write("已处理文件%s（totalcount=%d,insertcount=%d），耗时%.2f秒。\n",\
                        dir.m_ffilename.c_str(),totalcount,inscount,timer.elapsed());
        pactive.uptatime();
        
    }
    
    return true;
}

