/*
    程序名：deletetable.cpp 用于将指定 表 和 条件的数据库内容删除
*/

#include "_public.h"
#include "_ooci.h"
using namespace idc;

// 程序运行的参数结构体
struct st_arg
{
    char connstr[101];  // 数据库的连接参数。
    char tname[31];     // 待清理的表名。
    char keycol[31];    // 待清理的表的唯一键字段名。
    char where[1001];   // 待清理的数据需要满足的条件。
    int maxcount;       // 执行一次SQL删除的记录数。
    char starttime[31]; // 程序运行的时间区间。
    int timeout;        // 本程序运行时的超时时间。
    char pname[51];     // 本程序运行时的程序名。
} starg;

clogfile logfile;
connection conn;
ctimer timer;
cpactive pactive;

void _help();
bool _xmltoarg(const char* strxmlbuffer);
void EXIT(int sig);

bool instarttime();     // 判断是否在starttime的时间内
bool _deletetable();    // 业务主函数

int main(int argc, char *argv[])
{
    if (argc != 3){ _help(); return -1; }

    // 关闭全部的信号和输入输出
    // 处理程序退出的信号
    closeioandsignal();
    signal(SIGINT, EXIT); signal(SIGTERM, EXIT);

    if (logfile.open(argv[1]) == false){
        printf("打开日志文件失败（%s）。\n", argv[1]);
        return -1;
    }

    // 把xml解析到参数starg结构中
    if (_xmltoarg(argv[2]) == false)
        return -1;

    if(!instarttime())
        return 0;

    pactive.addpinfo(starg.timeout, starg.pname);

    _deletetable();
    return 0;
}

// 显示程序的帮助
void _help()
{
    printf("Using:/home/lz6years/backend/myproject/tools/bin/deletetable logfilename xmlbuffer\n\n");

    printf("Sample:/home/lz6years/backend/myproject/tools/bin/procctl 3600 /home/lz6years/backend/myproject/tools/bin/deletetable /log/idc/deletetable_ZHOBTMIND1.log "
           "\"<connstr>idc/root@snorcl11g_121</connstr><tname>T_ZHOBTMIND1</tname>"
           "<keycol>rowid</keycol><where>where ddatetime<sysdate-0.03</where>"
           "<maxcount>10</maxcount><starttime>22,23,00,01,02,03,04,05,06,13</starttime>"
           "<timeout>120</timeout><pname>deletetable_ZHOBTMIND1</pname>\"\n\n");

    printf("本程序是共享平台的公共功能模块，用于清理表中的数据。\n");

    printf("logfilename 本程序运行的日志文件。\n");
    printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");

    printf("connstr     数据库的连接参数，格式：username/passwd@tnsname。\n");
    printf("tname       待清理数据表的表名。\n");
    printf("keycol      待清理数据表的唯一键字段名，可以用记录编号，如keyid，建议用rowid，效率最高。\n");
    printf("where       待清理的数据需要满足的条件，即SQL语句中的where部分。\n");
    printf("maxcount    执行一次SQL语句删除的记录数，建议在100-500之间。\n");
    printf("starttime   程序运行的时间区间，例如02,13表示：如果程序运行时，踏中02时和13时则运行，其它时间不运行。"
           "如果starttime为空，本参数将失效，只要本程序启动就会执行数据清理，"
           "为了减少对数据库的压力，数据清理一般在业务最闲的时候时进行。\n");
    printf("timeout     本程序的超时时间，单位：秒，建议设置120以上。默认为：150\n");
    printf("pname       进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。默认为：deletetable_{tname}\n\n");
}

// 业务主函数
bool _deletetable()
{
    // 1) 连接数据库
    if(conn.connecttodb(starg.connstr, idc::SIM_AL32UTF8, true) != 0) {    // 启用自动commit
        logfile.write("connecttodb(%s) failed.\n", starg.connstr);
        return false;
    }

    timer.start();

    char seltmp[21]; // 用来存放select返回的内容
    // 2) 准备查询rowid的字段
    // select rowid from T_ZHOBTMIND1 WHERE ....;
    sqlstatement stmtsel;
    stmtsel.connect(&conn); 
    string strsel = sformat("select %s from %s %s", starg.keycol, starg.tname, starg.where);
    stmtsel.prepare(strsel);
    stmtsel.bindout(1, seltmp);

    // 3) 准备删除的sql语句
    // delete from T_ZHOBTMIND1 where rowid in(:1,:2,:3,:4,:5,:6,:7,:8,:9,:10);
    sqlstatement stmtdel;
    stmtdel.connect(&conn);
    string strdel = sformat("delete from %s where %s in(", starg.tname, starg.keycol);
    for (int i = 1; i <= starg.maxcount; ++i)
        strdel += sformat(":%d,", i);
    deleterchr(strdel, ','); strdel += ")";
    stmtdel.prepare(strdel);

    char tmpvalue[starg.maxcount][21];
    // 绑定删除的内容
    for (int i = 0; i < starg.maxcount; ++i){
        stmtdel.bindin(i + 1, tmpvalue[i], 20);
    }

    if (stmtsel.execute() != 0){  // 执行提取数据的SQL语句。
        logfile.write("stmtsel.execute() failed.\n%s\n%s\n", stmtsel.sql(), stmtsel.message());
        return false;
    }

    int cnt = 0; // keyvalues数组中有效元素的个数。
    memset(tmpvalue, 0, sizeof(tmpvalue));

    while(true)
    {
        memset(seltmp, 0, sizeof(seltmp));
        if(stmtsel.next() != 0)
            break;
        strcpy(tmpvalue[cnt], seltmp);
        cnt++;

        if(cnt >= starg.maxcount)
        {
            if (stmtdel.execute() != 0) // 执行从表中删除数据的SQL语句。
            {
                logfile.write("stmtdel.execute() failed.\n%s\n%s\n", stmtdel.sql(), stmtdel.message());
                return false;
            }

            cnt = 0;
            memset(tmpvalue, 0, sizeof(tmpvalue));

            pactive.uptatime();
        }
    }

    // 4）如果临时数组中还有记录，再执行一次删除数据的SQL语句。
    if (cnt > 0){
        if (stmtdel.execute() != 0){
            logfile.write("stmtdel.execute() failed.\n%s\n%s\n", stmtdel.sql(), stmtdel.message());
            return false;
        }
    }

    if (stmtsel.rpc() > 0)
        logfile.write("delete from %s %d rows in %.02fsec.\n", starg.tname, stmtsel.rpc(), timer.elapsed());

    return true;
}

// 判断是否在starttime的时间内
bool instarttime()
{
    if(strlen(starg.starttime) == 0)
        return true;

    // 若有值
    if(strstr(starg.starttime, ltime1("hh24").c_str()) == 0){
        return false;
    }

    return true;
}

// 把xml解析到参数starg结构中
bool _xmltoarg(const char *strxmlbuffer)
{
    memset(&starg, 0, sizeof(struct st_arg));

    getxmlbuffer(strxmlbuffer, "connstr", starg.connstr, 100);
    if (strlen(starg.connstr) == 0){
        logfile.write("connstr is null.\n");
        return false;
    }

    getxmlbuffer(strxmlbuffer, "tname", starg.tname, 30);
    if (strlen(starg.tname) == 0){
        logfile.write("tname is null.\n");
        return false;
    }

    getxmlbuffer(strxmlbuffer, "keycol", starg.keycol, 30);
    if (strlen(starg.keycol) == 0){
        logfile.write("keycol is null.\n");
        return false;
    }

    getxmlbuffer(strxmlbuffer, "where", starg.where, 1000);
    if (strlen(starg.where) == 0){
        logfile.write("where is null.\n");
        return false;
    }

    getxmlbuffer(strxmlbuffer, "starttime", starg.starttime, 30);

    getxmlbuffer(strxmlbuffer, "maxcount", starg.maxcount);
    if (starg.maxcount == 0){
        logfile.write("maxcount is null.\n");
        return false;
    }

    getxmlbuffer(strxmlbuffer, "timeout", starg.timeout);
    if (starg.timeout == 0){
        starg.timeout = 150;
    }

    getxmlbuffer(strxmlbuffer, "pname", starg.pname, 50);
    if (strlen(starg.pname) == 0){
        strncpy(starg.pname, sformat("deletetable_%s", starg.tname).c_str(),50);
    }

    return true;
}

void EXIT(int sig)
{
    logfile.write("程序退出，sig=%d\n\n", sig);

    conn.disconnect();

    exit(0);
}
