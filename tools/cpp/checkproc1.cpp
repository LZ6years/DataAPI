/*
    程序名：checkproc.cpp 检查共享内存中进程的心跳，如果超时，则终止进程
*/
#include "_public.h"
using namespace idc;

int main(int argc,char* argv[])
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

    //关闭所有的信号与IO
    closeioandsignal(true);

    //将日志文件打开
    clogfile logfile;
    if(!logfile.open(argv[1])){
        printf("logfile.open(%s) failed \n",argv[1]); return -1;
    }

    //先获取相同的共享内存
    int shmid = 0;
    st_procinfo *shm = nullptr;
    if((shmid = shmget(key_t(SHMKEYP),MAXNUMP*sizeof(struct st_procinfo),0666|IPC_CREAT)) == -1){
        logfile.write("创建/获取共享内存(%x)失败！\n",SHMKEYP);return -1;
    }

    //将共享内存连接入此进程中
    shm = (struct st_procinfo *)shmat(shmid,0,0);
    //遍历内存中的每一个值，查看是否为超时进程
    //超时进程1：已经死掉，但是其信息留在共享内存中(被kill -9 杀死的进程)
    //超时进程2：还没有死，但是其 (现在的时间 - 最后一次修改时间 > 超越时间),因将其杀死，并处理其共享内存的部分
    for(int i = 0; i < MAXNUMP; ++i){

        //若此时进程编号为0，则跳过
        if(shm[i].pid == 0) continue;

        // //显示进程信息，程序稳定运行后，以下两行代码可以注释掉。 
        // logfile.write("ii=%d,pid=%d,pname=%s,timeout=%d,atime=%d\n",\
        //               i,shm[i].pid,shm[i].pname,shm[i].timeout,shm[i].atime);
        
        //1.对于已经死掉的进程(不管有没有心跳，都要去掉)
        //向其发送信号，看其是否已经死掉
        int iret = kill(shm[i].pid,0);  //返回值为-1时，表示其已不在，返回0表示还存在
        if(iret == -1){
            //将其信息清空
            logfile.write("进程pid=%d(%s) 已经不存在 \n",shm[i].pid,shm[i].pname);
            memset(&shm[i],0,sizeof(struct st_procinfo));
            continue;
        }

        //2.对于还有心跳的进程
        time_t now = time(0);  //获取当前时间
        if(now - shm[i].atime <= shm[i].timeout) continue;

        // 一定要把进程的结构体备份出来(后面会删去共享内存的值，再使用时，已经为空了)
        struct st_procinfo tmp=shm[i];
		if (tmp.pid == 0) continue; 

        //3.对于没有心跳的进程,并且活着的进程
        logfile.write("进程pid=%d(%s) 已经超时 \n",tmp.pid,tmp.pname);

        //先发送信号15,尝试终止进程 (此进程可能忽略15信号)
        kill(tmp.pid,15);

        //进程的退出需要时间，这里我们等待进程的退出(每个一秒看进程是否存在)
        for(int j = 0; j < 5; ++j){
            sleep(1);
            iret = kill(tmp.pid,0);
            if(iret == -1) break;
        }
        //查看进程是否退出，(这里还没有退出表示，15信号杀不死它，用-9强行杀死)

        if(iret == -1){  //进程已经退出
            //将其所占的空间，删去
            logfile.write("进程pid=%d(%s) 已经正常终止 \n",tmp.pid,tmp.pname);

            //这里不写资源释放：资源正常退出，资源释放的事情应该交给终止进程自己处理
        }
        else{   //进程还是没有退出
            //用9信号强行杀死
            kill(tmp.pid,9);
            logfile.write("进程pid=%d(%s) 已经强行杀死 \n",tmp.pid,tmp.pname);

            //将其资源释放调
            memset(&shm[i],0,sizeof(struct st_procinfo));
        }
    }

    //把共享内存从当前进程分离
    shmdt(shm);

    return 0;
}