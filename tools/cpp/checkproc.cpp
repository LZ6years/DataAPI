// 守护程序：检查共享内存中进程的心跳，如果超时，则终止进程。
#include "_public.h"
using namespace idc;

int main(int argc,char *argv[])
{
    //程序的帮助
    if(argc != 2){
        cout << "\n";
        cout << "Using:./checkproc logfilename\n";
        cout << "Example:/home/test/myproject/tools/bin/procctl 10 /home/test/myproject/tools/bin/checkproc /home/test/tmp/log/checkproc.log\n\n";

        cout << "本程序用于检查后台服务程序是否超时，若已超时，则会终止它";
        cout << "注意：\n";
        cout << "1) 本程序由procctl启动，运行周期建议为10秒 \n";
        cout << "2) 为了避免被普通用户误杀，本程序应该用root用户启动 \n";
        cout << "3) 若要停止本程序，只能用killall -9 来终止 \n";
        return 0;
    }

    // 忽略全部的信号和IO，不处理程序的退出信号。
    closeioandsignal(true);

    // 打开日志文件。
    clogfile logfile;  // 程序运行的日志。
    if (logfile.open(argv[1])==false) 
    { 
        printf("logfile.open(%s) failed.\n",argv[1]);   return -1; 
    }

    // 创建/获取共享内存，键值为SHMKEYP，大小为MAXNUMP个st_procinfo结构体的大小。
    int shmid=0;
	if ( (shmid = shmget((key_t)SHMKEYP, MAXNUMP*sizeof(struct st_procinfo), 0666|IPC_CREAT)) == -1)
    {
        logfile.write("创建/获取共享内存(%x)失败。\n",SHMKEYP); return false;
    }

    // 将共享内存连接到当前进程的地址空间。
    struct st_procinfo *shm=(struct st_procinfo *)shmat(shmid, 0, 0);

    // 遍历共享内存中全部的记录，如果进程已超时，终止它。
    for (int ii=0;ii<MAXNUMP;ii++)
    {
        // 如果记录的pid==0，表示空记录，continue;
        if (shm[ii].pid==0) continue;

        // 如果记录的pid!=0，表示是服务程序的心跳记录。

        // 显示进程信息，程序稳定运行后，以下两行代码可以注释掉。
        //logfile.write("ii=%d,pid=%d,pname=%s,timeout=%d,atime=%d\n",\
        //               ii,shm[ii].pid,shm[ii].pname,shm[ii].timeout,shm[ii].atime);
        
        // 如果进程已经不存在了，共享内存中是残留的心跳信息。
        // 向进程发送信号0，判断它是否还存在，如果不存在，从共享内存中删除该记录，continue;
        int iret=kill(shm[ii].pid,0);
        if (iret==-1)
        {
            logfile.write("进程pid=%d(%s)已经不存在。\n",shm[ii].pid,shm[ii].pname);
            memset(&shm[ii],0,sizeof(struct st_procinfo));   // 从共享内存中删除该记录。
            continue;
        } 

        // 判断进程的心跳是否超时，如果超时了，就终止它。

        time_t now=time(0);   // 获取当前时间。

        // 如果进程未超时，continue;
        if (now-shm[ii].atime<shm[ii].timeout) continue;

        // 一定要把进程的结构体备份出来，不能直接用共享内存中的值。
        struct st_procinfo tmp=shm[ii];
		if (tmp.pid==0) continue;  

        // 如果已超时。
        logfile.write("进程pid=%d(%s)已经超时。\n",tmp.pid,tmp.pname);

        // 发送信号15，尝试正常终止已超时的进程。
        kill(tmp.pid,15);     

        // 每隔1秒判断一次进程是否存在，累计5秒，一般来说，5秒的时间足够让进程退出。
        for (int jj=0;jj<5;jj++)
        {
            sleep(1);
            iret=kill(tmp.pid,0);     // 向进程发送信号0，判断它是否还存在。
            if (iret==-1) break;         // 进程已退出。
        }

        if (iret==-1)
            logfile.write("进程pid=%d(%s)已经正常终止。\n",tmp.pid,tmp.pname);
        else
        {   
            // 如果进程仍存在，就发送信号9，强制终止它。
            kill(tmp.pid,9);  
            logfile.write("进程pid=%d(%s)已经强制终止。\n",tmp.pid,tmp.pname);

            // 从共享内存中删除已超时进程的心跳记录。
            memset(shm+ii,0,sizeof(struct st_procinfo));
        }
    }

    // 把共享内存从当前进程中分离。
    shmdt(shm);

    return 0;
}