/*
    obtcodetodb.cpp,本程序将全国气象参数文件的值存入到数据库中
*/
#include "_public.h"
#include "_ooci.h"    // 操作Oracle的头文件。
using namespace idc;

clogfile logfile;
connection conn;
cpactive pactive;

struct st_stcode
{
    char provname[31];  //省份
    char obtid[6];     //站号
    char cityname[31];   //站名:同城市名
    char lat[11];         //纬度:度
    char lon[11];         //经度:度
    char height[11];      //海拔高度:米
}stcode;
list<struct st_stcode> stlist; //用来保存每一个数据的容器
//功能:用来加载数据的函数
//inifile:表示传入的文件名
bool loadstcode(const string &inifile);

void EXIT(int sig);
void _help();


int main(int argc,char * argv[])
{
    //给出帮助文档
    if(argc != 5) {_help(); return -1;}

    // 关闭全部的信号和输入输出。
    // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
    // 但请不要用 "kill -9 +进程号" 强行终止。
    // closeioandsignal(true);
    signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

    //(1)打开日志文件
    if(!logfile.open(argv[4]))
    {
         printf("打开日志文件失败(%s)。\n",argv[4]); return -1;
    }

    pactive.addpinfo(10,"obtcodetodb");  //增加进程心跳

    //(2)将数据准备到容器中
    if (!loadstcode(argv[1])) EXIT(-1);

    //(3)连接数据库
    if(conn.connecttodb(argv[2],argv[3]) != 0)
    {
        logfile.write("database connect %s,failed\n%s\n",argv[2],conn.message());EXIT(-1);
    }
    logfile.write("database connect %s,success\n",argv[2]);

    //(4)准备好插入与更新的sql语句
    sqlstatement stmtins(&conn);     //进行插入的stmt语句
    sqlstatement stmtupd(&conn);    //进行更新的stmt语句


    //对于插入进行绑定
    stmtins.prepare("insert into t_zhobtcode(obtid,cityname,provname,lat,lon,height,keyid) \
                                values(:1,:2,:3,:4*100,:5*100,:6*10,seq_zhobtcode.nextval)");
    stmtins.bindin(1,stcode.obtid,5);
    stmtins.bindin(2,stcode.cityname,30);
    stmtins.bindin(3,stcode.provname,30);
    stmtins.bindin(4,stcode.lat,10);
    stmtins.bindin(5,stcode.lon,10);
    stmtins.bindin(6,stcode.height,10);

    //对于更新的绑定
    stmtupd.prepare("update t_zhobtcode \
                     set cityname=:1,provname=:2,lat=:3*100,lon=:4*100,height=:5*10,upttime=sysdate where obtid=:6");
    stmtupd.bindin(1,stcode.cityname,30);
    stmtupd.bindin(2,stcode.provname,30);
    stmtupd.bindin(3,stcode.lat,10);
    stmtupd.bindin(4,stcode.lon,10);
    stmtupd.bindin(5,stcode.height,10);
    stmtupd.bindin(6,stcode.obtid,5);

    long long ins_count = 0;
    long long upd_count = 0;

    ctimer timer;  //定义计时器，看总执行时间
    //(5)进行插入操作
    for (auto &tmp_stcode : stlist)
    {
        //初始化这次需要更新的对象
        stcode = tmp_stcode;

        if(stmtins.execute() != 0)
        {
            //插入失败
            if(stmtins.rc() == 1) // 错误代码为ORA-0001违返唯一约束，表示该站点的记录在表中已存在。
            {
                //更新操作
                if(stmtupd.execute() != 0)
                {
                    logfile.write("stmtupt.execute() failed.\n%s\n%s\n",stmtupd.sql(),stmtupd.message()); EXIT(-1);
                }
                else
                    upd_count++;
            }
            else
            {
                //其他错误原因，返回插入失败
                logfile.write("stmtins.execute() failed.\n%s\n%s\n",stmtins.sql(),stmtins.message()); EXIT(-1);
            }
        }
        else
            ins_count++;
  
    }

    // 把总记录数、插入记录数、更新记录数、消耗时长写日志。
    logfile.write("总记录数=%d，插入=%d，更新=%d，耗时=%.2f秒。\n",stlist.size(),ins_count,upd_count,timer.elapsed());

    //(6)执行，commit操作
    conn.commit();

    return 0;
}

bool loadstcode(const string &inifile)
{

    cifile ifile;  //定义打开文件的对象
    //先打开文件
    if(!ifile.open(inifile)){
        logfile.write("ifile.open(%s) failed\n",inifile.c_str()); //这里传入c_str需要是c分格的字符串
        return false;
    }

    string buffer; //用来截取文件的一行
    ifile.readline(buffer); //读入第一行,并不需要(注意,每一次读取时,会自动清空buffer的内容)

    ccmdstr cmdstr; //用来拆分字符串的对象
    struct st_stcode stcode;  //用来保存每一行的数据信息

    //开始读取其中的每一行
    while(ifile.readline(buffer)){
        //将结构体的数据先清空为0
        memset(&stcode,0,sizeof stcode);

        //按照 ","拆分
        cmdstr.splittocmd(buffer,",");

        //保存到零时结构体中
        cmdstr.getvalue(0,stcode.provname,30);
        cmdstr.getvalue(1,stcode.obtid,5);
        cmdstr.getvalue(2,stcode.cityname,30);
        cmdstr.getvalue(3,stcode.lat,10);
        cmdstr.getvalue(4,stcode.lon,10);
        cmdstr.getvalue(5,stcode.height,10);

        //保存到列表中
        stlist.push_back(stcode);
    }

    return true;
}

void _help()
{
    printf("\n");
    printf("Using:./obtcodetodb inifile connstr charset logfile\n");

    printf("Example:/home/lz6years/backend/myproject/tools/bin/procctl 120 /home/lz6years/backend/myproject/idc/bin/obtcodetodb /home/lz6years/backend/myproject/idc/ini/stcode.ini "\
                  "\"idc/root@snorcl11g_121\" \"Simplified Chinese_China.AL32UTF8\" /log/idc/obtcodetodb.log\n\n");

    printf("本程序用于把全国气象站点参数数据保存到数据库的T_ZHOBTCODE表中，如果站点不存在则插入，站点已存在则更新。\n");
    printf("inifile 全国气象站点参数文件名（全路径）。\n");
    printf("connstr 数据库连接参数：username/password@tnsname\n");
    printf("charset 数据库的字符集。\n");
    printf("logfile 本程序运行的日志文件名。\n");
    printf("程序每120秒运行一次，由procctl调度。\n\n\n");
}

void EXIT(int sig)
{
    logfile.write("程序退出，sig=%d\n\n",sig);

    // 可以不写，在析构函数中会回滚事务和断开与数据库的连接。
    conn.rollback();
    conn.disconnect();   

    exit(0);
}