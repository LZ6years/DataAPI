#include "_public.h"
using namespace idc;

cpactive pactive;      // 进程心跳，用全局对象（保证析构函数会被调用）。

void EXIT(int sig)     // 程序退出和信号2、15的处理函数。
{
    printf("sig=%d\n",sig);

    exit(0);
}

int main(int argc,char *argv[])
{
    // 处理程序的退出信号。
    signal(SIGINT,EXIT); signal(SIGTERM,EXIT);
    // closeioandsignal(true);
    
    pactive.addpinfo(atoi(argv[1]),"demo02");  // 把当前进程的信息加入共享内存进程组中。

    while (1)
    {
        sleep(atoi(argv[2]));
        pactive.uptatime(); // 更新进程的心跳。
    }

    return 0;
}
