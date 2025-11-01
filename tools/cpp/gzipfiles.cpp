/*
    程序名：gzipfiles.cpp 用于清除程序的历史数据文件
*/
#include "_public.h"
using namespace idc;

void EXIT(int sig);  //退出函数的定义

cpactive pactive;    //给此进程加入心跳

int main(int argc,char *argv[])
{
    //给出帮助文档
     if (argc != 4)
    {
        printf("\n");
        printf("Using:/home/test/myproject/tools/bin/gzipfiles pathname matchstr timeout\n\n");

        printf("Example:/home/test/myproject/tools/bin/gzipfiles /home/test/tmp/idc/surfdata \"*.csv,*.xml,*.json\" 0.01\n");
        cout << R"(        /home/test/myproject/tools/bin/gzipfiles /home/test/log/idc "*.log.20*" 0.02)" << "\n";
        printf("        /home/test/myproject/tools/bin/procctl 300 /home/test/myproject/tools/bin/gzipfiles/log/idc \"*.log.20*\" 0.02\n");
        printf("        /home/test/myproject/tools/bin/procctl 300 /home/test/myproject/tools/bin/gzipfiles /home/test/tmp/idc/surfdata \"*.xml,*.json\" 0.01\n\n");

        printf("这是一个工具程序，用于压缩历史的数据文件或日志文件。\n");
        printf("本程序把pathname目录及子目录中timeout天之前的匹配matchstr文件全部删除，timeout可以是小数。\n");
        printf("本程序不写日志文件，也不会在控制台输出任何信息。\n\n\n");
        printf("本程序调用/usr/bin/gzip命令压缩文件\n\n\n");
        return -1;
	}
    //忽略所有的信号和关闭I/O，设置信号处理函数
    closeioandsignal(true);
    signal(SIGINT,EXIT);signal(SIGTERM,EXIT);

    //加入进程的心跳信息
    pactive.addpinfo(120,"gzipfiles");

    //得到需要压缩的时间点
    string strtimeout = ltime1("yyyymmddhh24miss",0 - int(atof(argv[3])*24*60*60));

    //打开目标目标目录
    cdir dir;
    if(!dir.opendir(argv[1],argv[2],10000,true)){
        printf("dir.opendir(%s) failed",argv[1]);return -1;
    }

    //对于满足的地方进行压缩处理(1.时间点要在压缩时间点前 2.需要不是已压缩文件)
    while(dir.readdir())
    {
        //满足条件的文件
        if(dir.m_atime <= strtimeout && !matchstr(dir.m_filename,"*.gz")){
            //压缩文件,调用系统的gzip命令(后面1> 2>表示向屏幕输出正常与错误与信息)
            string strcmd = "/usr/bin/gzip -f " + dir.m_ffilename + " 1>/dev/null 2>/dev/null";
            if(system(strcmd.c_str()) == 0){
                cout << "gzip " << dir.m_ffilename << " ok.\n";
            }else{
                cout << "gzip " << dir.m_ffilename << " failed.\n";
            }
        }

        // 压缩文件所用的时间较长，需要及时更新心跳的信息
        pactive.uptatime();
    }
    
    return 0;
}

void EXIT(int sig)
{
    printf("程序gzipfiles收到%d的信号，已退出",sig);

    exit(0);
}