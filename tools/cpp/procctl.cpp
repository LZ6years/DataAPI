// 本程序不需要包含_public.h，没必要依赖那么多头文件。
#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc,char *argv[])
{
     if(argc < 3){
       
        printf("Using:./procctl timetvl program argv ...\n");
        printf("Example:/project/tools/bin/procctl 10 /usr/bin/tar zcvf /tmp/tmp.tgz /usr/include\n");
  	    printf("Example:/home/test/myproject/tools/bin/procctl 60 /home/test/myproject/idc/bin/crtsurfdata /home/test/myproject/idc/ini/stcode.ini /home/test/tmp/idc/surfdata /home/test/log/idc/crtsurfdata.log csv,xml,json\n");

        printf("本程序是服务程序的调度程序，周期性启动服务程序或shell脚本。\n");
        printf("timetvl 运行周期，单位：秒。\n");
        printf("        被调度的程序运行结束后，在timetvl秒后会被procctl重新启动。\n");
        printf("        如果被调度的程序是周期性的任务，timetvl设置为运行周期。\n");
        printf("        如果被调度的程序是常驻内存的服务程序，timetvl设置小于5秒。\n");
        printf("program 被调度的程序名，必须使用全路径。\n");
        printf("...     被调度的程序的参数。\n");
        printf("注意，本程序不会被kill杀死，但可以用kill -9强行杀死。\n\n\n");

    return -1;
    }

    // 关闭信号和I/O，本程序不希望被打扰。
    // 注意：1）为了防调度程序被误杀，不处理退出信号；
    //       2）如果忽略和信号和关闭了I/O，将影响被调度的程序（也会忽略和信号和关闭了I/O）。 why？因为被调度的程序取代了子进程，子进程会继承父进程的信号处理方式和I/O。
    
    for (int ii=0;ii<64;ii++)
    {
        signal(ii,SIG_IGN);  close(ii);
    }

    // 生成子进程，父进程退出，让程序运行在后台，由系统1号进程托管，不受启动它的shell的控制。
    if (fork()!=0) exit(0);

    // 把子进程退出的信号SIGCHLD恢复为默认行为，让父进程可以调用wait()函数等待子进程退出。
    // 如果 继续忽略 SIGCHLD，那么子进程退出时会被内核自动回收（不会留下僵尸），
    // 但这也会导致父进程无法用 wait() 拿到子进程的结束状态（wait() 会返回 - 1 / 没有可等待的子进程）
    signal(SIGCHLD,SIG_DFL);

    // 定义一个和argv一样大的指针数组，存放被调度程序名及其参数。
    char *pargv[argc];
    for (int ii=2;ii<argc;ii++)
        pargv[ii-2]=argv[ii];

    pargv[argc-2]=nullptr; // 空表示参数已结束。

    while (true)
    {
        if (fork()==0)
        {
            // 子进程运行被调度的程序。
            execv(argv[2],pargv);
            exit(0);  // 如果被调度的程序运行失败，才会执行这行代码。
        }
        else
        {
            // 父进程等待子进程终止（被调度的程序运行结束）。
            //int status;
            //wait(&status);           // wait()函数会阻塞，直到被调度的程序终止。
            wait(nullptr);           // wait()函数会阻塞，直到被调度的程序终止。
            sleep(atoi(argv[1]));  // 休眠timetvl秒，然后回到循环。
        }
    }
}