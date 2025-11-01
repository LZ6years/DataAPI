/*
    程序名：server.cpp  此程序用来模拟进程的心跳信息（用共享内存来记录不同进程间的心跳信息）
*/
#include "_public.h"
using namespace idc;

//进程心跳的结构体
struct stprocinfo
{
    int m_pid = 0;          //进程的编号
    char m_pname[51] = {0}; //进程的名字 {0}表示将其中的内存信息赋值为0
    int m_timeout = 0;      //超时时间
    time_t m_atime = 0;     //最后一次心跳时间

    stprocinfo() = default; //代开默认构造函数
    //拷贝构造函数
    stprocinfo(const int pid,const string& pname,const int timeout,const time_t atime):m_pid(pid),m_timeout(timeout),m_atime(atime)
    {
        memcpy(m_pname,pname.c_str(),50);
    }
};

int m_shmid = -1;  //表示共享内存的编号信息
int m_pos = -1;    //表示此进程在共享内存中的位置
stprocinfo *m_shm = nullptr;  //用来指向共享内存的指针

//进程退出的处理函数
void EXIT(int sig);

int main()
{
    csemp semlock;  //定义信号量，给共享内存加锁

    if (semlock.init(0x5095) == false)  // 初始化信号量。
    {
        printf("创建/获取信号量(%x)失败。\n",0x5095); EXIT(-1);
    }

    //1.处理2，15号的信息，给出退出函数
    signal(SIGINT,EXIT);signal(SIGTERM,EXIT);

    //2.获取共享内存
    if((m_shmid = shmget((key_t)0x5095,1000*sizeof(struct stprocinfo),0640|IPC_CREAT)) == -1){
        printf("创建/获取共享内存(%x)失败",0x5095);return -1;
    }

    //3.将获取的共享内存连接到本进程中
    m_shm = (struct stprocinfo*)shmat(m_shmid,0,0);
    

    //4.得到本进程信息的心跳结构体
    struct stprocinfo procinfo(getpid(),"server1",30,time(0));

    semlock.wait();     //操作与寻找内存，需要加锁

    //5.在共享内存中找到位置，并写入内存中
    //先找到是否有与此进程相同的编号(原因可能为：遇到kill -9 异常退出,由于没有捕获9的信号，不能处理，这里若进程编号重复，则之前的进程已经异常退出了)
    for(int i = 0; i < 1000; ++i){
        if(m_shm[i].m_pid == procinfo.m_pid){
            m_pos = i;  //记录此时共享内存所用的位置
            printf("找到旧位置ii=%d\n",i);
            break;
        }
    }

    //寻找新位置
    if(m_pos == -1){
        //在找到是否有空的位置
        for(int i = 0; i < 1000; ++i){
         if(m_shm[i].m_pid == 0){
             m_pos = i;
             printf("找到新位置ii=%d\n",i);
             break;
            } 
        }
    }
    
    //没有位置的情况
    if(m_pos == -1){
        semlock.post(); printf("共享内存空间已用完。\n");  EXIT(-1); 
    }

    //写入内存
    memcpy(m_shm + m_pos,&procinfo,sizeof(struct stprocinfo));

    semlock.post();   //解锁

    //主要服务的运行程序
    while(true){

        cout << "服务程序正在运行中。。。"<<"\n";

        sleep(10); //表示程序运行所需的时间
        
        //更新其中最后一次的心跳信息  (只操作自己的内存，不需要信号量加锁)
        m_shm[m_pos].m_atime = time(0);  //注意：程序运行的时间要 小于 这里的超过时间timeout
    }


    return 0;
}

//进程退出的处理函数
void EXIT(int sig)
{
    //将进程退出的信息得到
    cout << "进程：" << getpid() << " 收到" << sig << "信号，已退出\n";

    //将自己进程所占的共享内存信息删去 (只操作自己的内存，不需要信号量加锁)
    if(m_pos != -1) memset(m_shm + m_pos,0,sizeof(struct stprocinfo));

    //将共享内存分离此进程          (只操作自己的内存，不需要信号量加锁)
    if(m_shm != nullptr) shmdt(m_shm);  //这里填入的是共享内存的指针

    exit(0);
}