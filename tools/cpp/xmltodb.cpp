/*
    程序名：xmltodb.cpp 本程序是将某文件夹中的xml文件内容入库到oracle数据库表中
*/
# include "_tools.h"
using namespace idc;


// 程序运行参数的结构体。
struct st_arg
{
    char connstr[101];     // 数据库的连接参数。
    char charset[51];      // 数据库的字符集。
    char inifilename[256]; // 初始化解析表的文件存放xml的路径
    char xmlpath[256];     // 需要自动解析的存放xml文件夹的路径
    char xmlpathbak[256];  // 已经解析成功的文件夹路径
    char xmlpatherr[256];  // 解析失败的文件夹路径
    int timetvl;           // 已抽取数据的递增字段最大值存放的数据库的连接参数。
    int timeout;           // 进程心跳的超时时间。
    char pname[51];        // 进程名，建议用"dminingoracle_后缀"的方式。
} starg;
clogfile logfile;
connection conn;
ctcols tcols;

ctimer timer;              // 统计处理一个xml文件的时间
cpactive pactive;
int totalcount, insertcnt, updatecnt; // 统计一个文件更新和插入的数量
bool xmltobakerr(const string &fullfilename, const string &srcpath, const string &dstpath);

struct st_xmltotable
{
    char filename[256];  // xml文件匹配的规则，用逗号分隔
    char tname[31];      // 待入库的表名
    int  uptbz;           // 更新标志：1-更新; 2-不更新;
    char execsql[301];   // 处理xml文件之前，执行的SQL语句
} stxmltotable;
vector<struct st_xmltotable> vxmltotable;   // 数据入库的参数的vector
bool loadxmltotable();                      // 根据inifilename中xml的 neirong，加载到vxmltotable rongqi中

void EXIT(int sig);
void _help();
bool _xmltoarg(const char *strxmlbuffer); // 用来将xml内容翻译到参数文件中
bool _xmltodb();                          // 业务主函数
int _xmltodb(const string& ffilename,const string& filename);    // 业务主函数重载，是在业务主函数中用于解析某一个文件，并入库到相应的表中
bool findxmltotable(const string &filename);

string strinsertsql;
string strupdatesql;
void preparesql(); // 准备SQL语句

// <obtid>58015</obtid><ddatetime>20230508113000</ddatetime><t>141</t><p>10153</p><u>44</u><wd>67</wd><wf>106</wf><r>9</r><vis>102076</vis><keyid>6127135</keyid>
vector<string> vcolvalue;         // 存放从xml每一行中解析出来的字段的值，将用于插入和更新表的SQL语句绑定变量。
sqlstatement stmtins,stmtupt;     // 插入和更新表的sqlstatement语句。
void bindsql();    // 绑定SQL变量

bool execpresql();   //执行所有先前的需要的sql语句

void splittobuffer(const string &strbuffer); // 将buffer neirong解析到vcolvalue中

int main(int argc, char *argv[])
{
    if (argc != 3){
        _help(); return -1;
    }

    // 关闭全部的信号和输入输出。
    // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
    // 但请不要用 "kill -9 +进程号" 强行终止。
    // closeioandsignal(true);
    signal(SIGINT, EXIT); signal(SIGTERM, EXIT);

    // 打开日志文件
    if (!logfile.open(argv[1])){
        printf("logfile.open(%s) failed\n", argv[1]);
        return -1;
    }

    //解析xml参数的内容
    if(!_xmltoarg(argv[2])){
        logfile.write("解析参数失败！\n");
        return -1;
    }
    // printf("参数解析成功！\n");

    pactive.addpinfo(starg.timeout, starg.pname);

    if(!_xmltodb())
        logfile.write("xmltodb task failed\n");

    return 0;
}

void _help()
{
    printf("\n");
    printf("Using:./xmltodb logfilename xmlbuffer\n\n");
    printf("Sample:/home/lz6years/backend/myproject/tools/cpp/procctl 10 /home/lz6years/backend/myproject/tools/cpp/xmltodb /log/idc/xmltodb_vip.log "
           "\"<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset>"
           "<inifilename>/home/lz6years/backend/myproject/idc/ini/xmltodb.xml</inifilename>"
           "<xmlpath>/idcdata/xmltodb/vip</xmlpath><xmlpathbak>/idcdata/xmltodb/vipbak</xmlpathbak>"
           "<xmlpatherr>/idcdata/xmltodb/viperr</xmlpatherr>"
           "<timetvl>5</timetvl><timeout>50</timeout><pname>xmltodb_vip</pname>\"\n\n");

    printf("本程序是将某文件夹中的xml文件内容入库到oracle数据库表中.\n");
    printf("logfilename   本程序的运行的日志文件.\n");
    printf("xmlbuffer 本程序运行所需参数，用xml包括，详细内容如下：\n\n");

    printf("connstr     数据库的连接参数，格式：username/passwd@tnsname。\n");
    printf("charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n");
    printf("inifilename 数据入库的参数配置文件。\n");
    printf("xmlpath     待入库xml文件存放的目录。\n");
    printf("xmlpathbak  xml文件入库后的备份目录。\n");
    printf("xmlpatherr  入库失败的xml文件存放的目录。\n");
    printf("timetvl     扫描xmlpath目录的时间间隔（执行入库任务的时间间隔），单位：秒，视业务需求而定，2-30之间。\n");
    printf("timeout     本程序的超时时间，单位：秒，视xml文件大小而定，建议设置30以上。\n");
    printf("pname       进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。不填默认为程序名称\n\n");
}

bool _xmltodb()
{
    int icount = 50;
    // 遍历xmlpath中的文件下的文件
    cdir dir;

    while(true)   // 循环一次执行一次 ruku 的任务
    {
        if(icount > 40){
            if (!loadxmltotable()){
                logfile.write("loadxmltotable faild. \n");
                return false;
            }
        }
        else
            icount++;

        if (!dir.opendir(starg.xmlpath, "*.XML", 10000, false, true)){
            logfile.write("opendir(%s) failed\n", starg.xmlpath);
        }

        if(!conn.isopen()){
            // 连接数据库
            if (conn.connecttodb(starg.connstr, starg.charset)){
                logfile.write("connect database(%s) failed\n%s\n", starg.connstr, conn.message());
            }
            logfile.write("connect database(%s) ok.\n", starg.connstr);
        }

        while(true)
        {
            // 将 文件夹 下的匹配的文件 进行 ruku 操作
            if (!dir.readdir()) break;

            logfile.write("process file (%s) ...", dir.m_ffilename.c_str());
            
            // 处理这个xml文件
            int ret = _xmltodb(dir.m_ffilename, dir.m_filename);

            // 0-成功，没有错误。把已入库的xml文件移动到备份目录。
            if (ret == 0){
                logfile << "ok(" << stxmltotable.tname << ",总数=" << totalcount << ",插入=" << insertcnt
                        << ",更新=" << updatecnt << "，耗时=" << timer.elapsed() << ").\n";

                // 把xml文件移动到starg.xmlpathbak参数指定的目录中，一般不会发生错误，如果真发生了，程序将退出。
                if (xmltobakerr(dir.m_ffilename, starg.xmlpath, starg.xmlpathbak) == false)
                    return false;
            }

            // 1-入库参数不正确；3-待入库的表不存在；4-执行入库前的SQL语句失败。把xml文件移动到错误目录。
            if ((ret == 1) || (ret == 3) || (ret == 4)){
                if (ret == 1)
                    logfile << "failed，入库参数不正确。\n";
                if (ret == 3)
                    logfile << "failed，待入库的表（" << stxmltotable.tname << "）不存在。\n";
                if (ret == 4)
                    logfile << "failed，执行入库前的SQL语句失败。\n";

                // 把xml文件移动到starg.xmlpatherr参数指定的目录中，一般不会发生错误，如果真发生了，程序将退出。
                if (xmltobakerr(dir.m_ffilename, starg.xmlpath, starg.xmlpatherr) == false)
                    return false;
            }

            // 2-数据库错误，函数返回，程序将退出。
            if (ret == 2){
                logfile << "failed，数据库错误。\n";
                return false;
            }

            // 5- 打开xml文件失败，函数返回，程序将退出。
            if (ret == 5){
                logfile << "failed，打开文件失败。\n";
                return false;
            }
        }

        if (dir.size() == 0)
            sleep(starg.timetvl);
    }

    return true;
}


int _xmltodb(const string &ffilename, const string &filename)
{
    /* 返回代码详解
        1-入库参数配置不正确
        2-数据库系统有问题，或网络断开，或连接超时。
        3-待入库的表不存在。
        4-入库前，执行SQL语句失败
        5-打开XML文件失败。
    */

    insertcnt = 0, updatecnt = 0, totalcount = 0;
    timer.start();


    // 1) 从已有的容器vxmltotable中获取此文件的入库表名
    if (!findxmltotable(filename)){
        logfile.write("findxmltodbtabel(%s) failed.\n", filename.c_str());
        return 1; 
    }

    // 2) 在数据库中，根据数据字典查出此表需要的字段
    if (!tcols.allcols(conn, stxmltotable.tname)) 
        return 2;
    if (!tcols.pkcols(conn, stxmltotable.tname)) 
        return 2;

    // 3) 根据表中的字段拼接SQL语句
    // 准备好插入和更新的SQL语句
    if (tcols.m_allcols.size() == 0)
        return 3;

    preparesql();

    // 绑定SQL变量
    bindsql();

    // stxmltotable需要提前执行其中sql, 则先运行
    if(!execpresql())
        return 4;

    // 4) 打开XML文件
    cifile ifile;
    if (!ifile.open(ffilename)){
        conn.rollback();
        logfile.write("ofile.open(%s) failed\n", ffilename);
        return 5;
    }
    
    string strbuffer;
    
    while (true)
    {
        totalcount++;

        // 5) 读取其中的一条记录
        if (!ifile.readline(strbuffer, "</endl>"))
            break;

        // 6) 根据表中的字段名称，解析其中的记录
        splittobuffer(strbuffer);

        // 7) 执行插入其中文件的命令
        if (stmtins.execute() != 0) {
            if (stmtins.rc() == 1) { // 唯一键约束犯错
                // 看是否执行更新语句
                if(stxmltotable.uptbz != 1)
                    continue;
                
                // 执行更新语句
                if(stmtupt.execute() != 0){
                    // 如果update失败，记录出错的行和错误原因，函数不返回，继续处理数据，也就是说，不理这一行。
                    // 失败原因主要是数据本身有问题，例如时间的格式不正确、数值不合法、数值太大。
                    logfile.write("%s", strbuffer.c_str());
                    logfile.write("stmtupt.execute() failed.\n%s\n%s\n", stmtupt.sql(), stmtupt.message());
                }
                else
                    updatecnt++;
            }
            else
            {
                // 如果insert失败，记录出错的行和错误原因，函数不返回，继续处理数据，也就是说，不理这一行。
                // 失败原因主要是数据本身有问题，例如时间的格式不正确、数值不合法、数值太大。
                logfile.write("%s", strbuffer.c_str());
                logfile.write("stmtins.execute() failed.\n%s\n%s\n", stmtins.sql(), stmtins.message());
            }
        }
        else
        {
            insertcnt++;
            // 如果是数据库系统出了问题，常见的问题如下，还可能有更多的错误，如果出现了，再加进来。
            // ORA-03113: 通信通道的文件结尾；ORA-03114: 未连接到ORACLE；ORA-03135: 连接失去联系；ORA-16014：归档失败。
            if ((stmtins.rc() == 3113) || (stmtins.rc() == 3114) || (stmtins.rc() == 3135) || (stmtins.rc() == 16014))
                return 2;
        }
    }

    // 8) 执行 commit 操作
    conn.commit();

    pactive.uptatime();

    return 0;
}

void splittobuffer(const string &strbuffer)
{
    string strtmp;  //这里要用临时变量, 不能直接用vcolvalue[i],可能会使其中绑定后的地址更新

    for (int i = 0; i < tcols.m_vallcols.size(); ++i)
    {
        getxmlbuffer(strbuffer, tcols.m_vallcols[i].colname, strtmp, tcols.m_vallcols[i].collen);

        // 只需要提取其中的yyyymmddhh24miss
        if (strcmp(tcols.m_vallcols[i].datatype, "date") == 0){
            strtmp = picknumber(strtmp, false, false);
        }

        if (strcmp(tcols.m_vallcols[i].datatype, "number") == 0){
            strtmp = picknumber(strtmp, true, true);
        }

        vcolvalue[i] = strtmp.c_str();  // 这里不能是strtmp, 会调用移动构造导致地址修改
    }
}

void preparesql()
{
    // 1.准备插入的insert语句
    // insert into t_zhobtmind(obtid,ddatetime,t,p,u,wd,wf,r,vis,upttime,keyid)
    //        values(:1,to_date(:2, 'yyyymmddhh24miss'),:3,:4,:5,:6,:7,:8,:9,sysdate,SEQ_ZHOBTMIND1.nextval)

    string strinsertsqltp;
    int cnt = 1;
    // 拼接“:1,to_date(:2, 'yyyymmddhh24miss'),:3,:4,:5,:6,:7,:8,:9,sysdate,SEQ_ZHOBTMIND1.nextval”
    for (int i = 0; i < tcols.m_vallcols.size(); ++i)
    {
        // keyid特殊处理
        if(strcmp(tcols.m_vallcols[i].colname, "keyid") == 0){
            strinsertsqltp += sformat("SEQ_%s.nextval,", stxmltotable.tname + 2);
            continue;
        }

        // upttime特殊处理
        if (strcmp(tcols.m_vallcols[i].colname, "upttime") == 0){
            strinsertsqltp += "sysdate,";
            continue;
        }

        // date类型特殊处理
        if(strcmp(tcols.m_vallcols[i].datatype, "date") == 0){
            strinsertsqltp += sformat("to_date(:%d,'yyyymmddhh24miss'),", cnt);
            cnt++;   //要有
            continue;
        }

        // 其余正常处理
        strinsertsqltp += sformat(":%d,", cnt);
        cnt++;
    }

    deleterchr(strinsertsqltp, ',');
    // 拼接得到insert的内容
    strinsertsql = sformat("insert into %s(%s) values(%s)", stxmltotable.tname, tcols.m_allcols.c_str(), strinsertsqltp.c_str());

    // logfile.write("文件(%s), 执行插入 %s\n", stxmltotable.filename, strinsertsql.c_str());

    // 不要更新的话，直接返回
    if(stxmltotable.uptbz != 1)
        return;

    // 2.准备插入的update语句
    // update t_zhobtmind set t=:1,p=:2,u=:3,wd=:4,wf=:5,r=:6,vis=:7,upttime=sysdate
    //        where 1=1 and obtid=:8 and ddatetime=to_date(:9,'yyyymmddhh24miss');

    strupdatesql = sformat("update %s set ", stxmltotable.tname);

    cnt = 1;
    // 拼接“:1,to_date(:2, 'yyyymmddhh24miss'),:3,:4,:5,:6,:7,:8,:9,sysdate,SEQ_ZHOBTMIND1.nextval”
    for (int i = 0; i < tcols.m_vallcols.size(); ++i)
    {
        // keyid特殊处理 和 主键字段先跳过
        if (strcmp(tcols.m_vallcols[i].colname, "keyid") == 0 || tcols.m_vallcols[i].pkseq != 0){
            continue;
        }

        // upttime特殊处理
        if (strcmp(tcols.m_vallcols[i].colname, "upttime") == 0){
            strupdatesql += "upttime=sysdate,";
            continue;
        }

        // date类型特殊处理
        if (strcmp(tcols.m_vallcols[i].datatype, "date") == 0){
            strupdatesql += sformat("%s=to_date(:%d,'yyyymmddhh24miss'),", tcols.m_vallcols[i].colname,cnt);
            cnt++; // 要有
            continue;
        }

        // 其余正常处理
        strupdatesql += sformat("%s=:%d,", tcols.m_vallcols[i].colname, cnt);
        cnt++;
    }
    deleterchr(strupdatesql, ',');

    strupdatesql += " where 1=1";
    // 拼接剩下的部分
    for (int i = 0; i < tcols.m_vpkcols.size(); ++i)
    {
        // date类型特殊处理
        if (strcmp(tcols.m_vpkcols[i].datatype, "date") == 0)
        {
            strupdatesql += sformat(" and %s=to_date(:%d,'yyyymmddhh24miss')", tcols.m_vallcols[i].colname, cnt);
            cnt++; // 要有
            continue;
        }

        // 其余正常处理
        strupdatesql += sformat(" and %s=:%d", tcols.m_vallcols[i].colname, cnt);
        cnt++;
    }

    // logfile.write("文件(%s), 执行更新 %s\n", stxmltotable.filename, strupdatesql.c_str());
}

void bindsql()
{
    // 1.准备插入的insert语句
    // insert into t_zhobtmind(obtid,ddatetime,t,p,u,wd,wf,r,vis,upttime,keyid)
    //        values(:1,to_date(:2, 'yyyymmddhh24miss'),:3,:4,:5,:6,:7,:8,:9,sysdate,SEQ_ZHOBTMIND1.nextval)
    stmtins.connect(&conn);
    stmtins.prepare(strinsertsql);
    vcolvalue.resize(tcols.m_vallcols.size());

    // logfile.write("insert process: %d条记录\n", tcols.m_vallcols.size());

    int seq = 1;
    // 绑定到vcolvalue的mem中
    for (int i = 0; i < tcols.m_vallcols.size(); ++i){

        // 对于upttime和keyid不需要绑定
        if (strcmp(tcols.m_vallcols[i].colname, "upttime") == 0 ||
            strcmp(tcols.m_vallcols[i].colname, "keyid") == 0)
            continue;

        stmtins.bindin(seq, vcolvalue[i], tcols.m_vallcols[i].collen);
        // logfile.write("stmtins.bindin(%d,vcolvalue[%d],%d);\n", seq, i, tcols.m_vallcols[i].collen);

        seq++; // 输入参数的序号加1。
    }

    if (stxmltotable.uptbz != 1)
        return;
    // 2.准备插入的update语句
    // update t_zhobtmind set t=:1,p=:2,u=:3,wd=:4,wf=:5,r=:6,vis=:7,upttime=sysdate
    //        where 1=1 and obtid=:8 and ddatetime=to_date(:9,'yyyymmddhh24miss');

    stmtupt.connect(&conn);
    stmtupt.prepare(strupdatesql);

    // logfile.write("update process: %d条记录\n", tcols.m_vallcols.size());

    seq = 1;
    // 绑定到vcolvalue的mem中
    for (int i = 0; i < tcols.m_vallcols.size(); ++i){
        
        if(tcols.m_vallcols[i].pkseq != 0)
            continue;

        // 对于upttime和keyid不需要绑定
        if (strcmp(tcols.m_vallcols[i].colname, "upttime") == 0 ||
            strcmp(tcols.m_vallcols[i].colname, "keyid") == 0)
            continue;

        stmtupt.bindin(seq, vcolvalue[i], tcols.m_vallcols[i].collen);
        // logfile.write("stmtupt.bindin(%d,vcolvalue[%d],%d);\n", seq, i, tcols.m_vallcols[i].collen);

        seq++; // 输入参数的序号加1。
    }

    for (int i = 0; i < tcols.m_vpkcols.size(); ++i){

        stmtupt.bindin(seq, vcolvalue[i], tcols.m_vpkcols[i].collen);
        // logfile.write("stmtupt.bindin(%d,vcolvalue[%d],%d);\n", seq, i, tcols.m_vpkcols[i].collen);

        seq++; // 输入参数的序号加1。
    }

    return;
}

bool execpresql()
{
    if(strlen(stxmltotable.execsql) == 0)
        return true;

    sqlstatement stmttp;
    stmttp.connect(&conn);

    stmttp.prepare(stxmltotable.execsql);

    if(stmttp.execute() != 0){
        logfile.write("stmttp.execute() failed.\n%s\n%s\n", stmttp.sql(), stmttp.message());
        return false;
    }
    
    return true;
}

bool findxmltotable(const string &filename)
{
    for (const auto& aa : vxmltotable)
    {
        if (matchstr(filename, aa.filename))
        {
            stxmltotable = aa;
            return true;
        }
    }
    return false;
}

bool loadxmltotable()
{
    vxmltotable.clear();

    cifile ifile;
    //打开文件
    if(!ifile.open(starg.inifilename)){
        logfile.write("打开文件(%s) failed\n", starg.inifilename);
        return false;
    }

    string strbuffer;
    while (true)
    {
        // 读取其中的一行
        if(!ifile.readline(strbuffer, "</endl>")) break;

        memset(&stxmltotable, 0, sizeof stxmltotable);

        // 解析其中的neirong 到 临时的 st_xmltotable 对象中
        getxmlbuffer(strbuffer, "filename", stxmltotable.filename, 255);
        getxmlbuffer(strbuffer, "tname", stxmltotable.tname, 30);
        getxmlbuffer(strbuffer, "uptbz", stxmltotable.uptbz);
        getxmlbuffer(strbuffer, "execsql", stxmltotable.execsql, 300);

        // 保存到vxmltotable
        vxmltotable.push_back(stxmltotable);
    }

    // logfile.write("loadxmltotable(%s) ok.\n", starg.inifilename);
    return true;
}

// 用来将xml内容翻译到参数文件中
bool _xmltoarg(const char *strxmlbuffer)
{
    memset(&starg, 0, sizeof starg);

    getxmlbuffer(strxmlbuffer, "connstr", starg.connstr, 100); // 数据库的连接参数
    if (strlen(starg.connstr) == 0){ logfile.write("connstr is null\n"); return false; }

    getxmlbuffer(strxmlbuffer, "charset", starg.charset, 50); // 数据库的字符集。
    if (strlen(starg.charset) == 0) { logfile.write("charset is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer, "inifilename", starg.inifilename, 255); // 数据库的连接参数
    if (strlen(starg.inifilename) == 0){ logfile.write("inifilename is null\n"); return false; }

    getxmlbuffer(strxmlbuffer, "xmlpath", starg.xmlpath, 255); // 数据库的字符集。
    if (strlen(starg.xmlpath) == 0){ logfile.write("xmlpath is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer, "xmlpathbak", starg.xmlpathbak, 255); // 数据库的字符集。
    if (strlen(starg.xmlpathbak) == 0){ logfile.write("xmlpathbak is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer, "xmlpatherr", starg.xmlpatherr, 255); // 数据库的字符集。
    if (strlen(starg.xmlpatherr) == 0){ logfile.write("xmlpatherr is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer, "timetvl", starg.timetvl); // 进程心跳的超时时间。
    if (starg.timetvl < 2){ starg.timetvl = 2; }
    if (starg.timetvl > 30){ starg.timetvl = 30; }

    getxmlbuffer(strxmlbuffer, "timeout", starg.timeout); // 进程心跳的超时时间。
    if (starg.timeout == 0) { logfile.write("timeout is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer, "pname", starg.pname, 50); // 进程名。
    if (strlen(starg.pname) == 0){
        strncpy(starg.pname, "xmltodb", 50);
    }

    return true;
}

// 把xml文件移动到备份目录或错误目录。
bool xmltobakerr(const string &fullfilename, const string &srcpath, const string &dstpath)
{
    string dstfilename = fullfilename; // 目标文件名。

    replacestr(dstfilename, srcpath, dstpath, false); // 小心第四个参数，一定要填false。

    if (renamefile(fullfilename, dstfilename.c_str()) == false)
    {
        logfile.write("renamefile(%s,%s) failed.\n", fullfilename, dstfilename.c_str());
        return false;
    }

    return true;
}

void EXIT(int sig)
{
    printf("收到%d的信号, 进程%d已退出", sig, getpid());

    exit(0);
}