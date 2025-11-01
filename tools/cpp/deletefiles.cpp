/*
    程序名：deletefiles.cpp 用于清除程序的历史数据文件
*/
#include "_public.h"
using namespace idc;

void EXIT(int sig);  //退出函数的定义

cpactive pactive;    //给此进程加入心跳

int main(int argc,char* argv[])
{
    // 程序的帮助。
    if (argc != 4)
    {
        printf("\n");
        printf("Using:/home/test/myproject/tools/bin/deletefiles pathname matchstr timeout\n\n");

        printf("Example:/home/test/myproject/tools/bin/deletefiles /home/test/tmp/idc/surfdata \"*.csv,*.xml,*.json\" 0.01\n");
        cout << R"(        /home/test/myproject/tools/bin/deletefiles /home/test/log/idc "*.log.20*" 0.02)" << "\n";
        printf("        /home/test/myproject/tools/bin/procctl 300 /home/test/myproject/tools/bin/deletefiles /log/idc \"*.log.20*\" 0.02\n");
        printf("        /home/test/myproject/tools/bin/procctl 300 /home/test/myproject/tools/bin/deletefiles /home/test/tmp/idc/surfdata \"*.xml,*.json\" 0.01\n\n");

        printf("这是一个工具程序，用于删除历史的数据文件或日志文件。\n");
        printf("本程序把pathname目录及子目录中timeout天之前的匹配matchstr文件全部删除，timeout可以是小数。\n");
        printf("本程序不写日志文件，也不会在控制台输出任何信息。\n\n\n");
        return -1;
	}

    //关掉所有的IO
    closeioandsignal(true);
    signal(SIGINT,EXIT); signal(SIGTERM,EXIT);  //注册15，2的信号

    pactive.addpinfo(30,"deletefiles");  //加入到共享内存中，加入信息

    //获取需要删除文件的限定时间点
    string strtimeout = ltime1("yyyymmddhh24miss",0 - (int)(atof(argv[3])*24*60*60));

    //打开文件所在的目录
    cdir dir;
    if(!dir.opendir(argv[1],argv[2],10000,true)){
        printf("dir.opendir(%s) failed\n",argv[1]);return -1;
    }

    int count = 0; //用来记录删除了多少文件

    //将符合matchstr 与 在限定时间点之前的文件删除
    while (dir.readdir())  //读取获得目录所有匹配文件中的一个文件
    {
        //若此文件的生成时间更早，就要删除
        if(dir.m_mtime < strtimeout){
            if(remove(dir.m_ffilename.c_str()) == 0){
                printf("remove(%s) ok",dir.m_ffilename.c_str());
                count ++;
            }
            else
            {
                printf("remove(%s) failed",dir.m_ffilename.c_str());
            }
        }   
    }

    printf("已成功删除%d文件",count);

    return 0;
}


//退出函数
void EXIT(int sig)
{
    printf("deletefiles程序收到(%d)信号，已退出", sig);

    exit(0);
}