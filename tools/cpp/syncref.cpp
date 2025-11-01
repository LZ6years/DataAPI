/*
 *  程序名：syncref.cpp，本程序是共享平台的公共功能模块，采用刷新的方法同步Oracle数据库之间的表。
 *  作者：王泽权。
*/
#include "_tools.h"

struct st_arg
{
    char localconnstr[101];        // 本地数据库的连接参数。
    char charset[51];              // 数据库的字符集。
    char linktname[31];            // dblink指向的远程表名，如T_ZHOBTCODE1@db128。
    char localtname[31];           // 本地表名。
    char remotecols[1001];         // 远程表的字段列表。
    char localcols[1001];          // 本地表的字段列表。
    char rwhere[1001];             // 同步数据的条件(远程表)。
    char lwhere[1001];             // 同步数据的条件(本地表)。
    int    synctype;               // 同步方式：1-不分批刷新；2-分批刷新。
    char remoteconnstr[101];       // 远程数据库的连接参数。
    char remotetname[31];          // 远程表名。
    char remotekeycol[31];         // 远程表的键值字段名。
    char localkeycol[31];          // 本地表的键值字段名。
    int    keylen;                 // 键值字段的长度。
    int    maxcount;               // 每批执行一次同步操作的记录数。
    int    timeout;                // 本程序运行时的超时时间。
    char pname[51];                // 本程序运行时的程序名。
} starg;

// 显示程序的帮助
void _help();

// 把xml解析到参数starg结构中
bool _xmltoarg(const char *strxmlbuffer);

clogfile logfile;

connection connloc;       // 本地数据库连接。
connection connrem;       // 远程数据库连接。

// 业务处理主函数。
bool _syncref();
 
void EXIT(int sig);

cpactive pactive;

int main(int argc,char *argv[])
{
    if (argc!=3) { _help(); return -1; }

    // 关闭全部的信号和输入输出，处理程序退出的信号。
    //closeioandsignal(true); 
    signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

    if (logfile.open(argv[1])==false)
    {
        printf("打开日志文件失败（%s）。\n",argv[1]); return -1;
    }

    // 把xml解析到参数starg结构中
    if (_xmltoarg(argv[2])==false) return -1;

    // pactive.addpinfo(starg.timeout,starg.pname);
    // 注意，在调试程序的时候，可以启用类似以下的代码，防止超时。
    pactive.addpinfo(starg.timeout*10000,starg.pname);

    if (connloc.connecttodb(starg.localconnstr,starg.charset) != 0)
    {
        logfile.write("connect database(%s) failed.\n%s\n",starg.localconnstr,connloc.message()); EXIT(-1);
    }

    // 如果starg.remotecols或starg.localcols为空，就用starg.localtname表的全部列来填充。
    if ( (strlen(starg.remotecols)==0) || (strlen(starg.localcols)==0) )
    {
        ctcols tcols;

        // 获取starg.localtname表的全部列。
        if (tcols.allcols(connloc,starg.localtname)==false)
        {
            logfile.write("表%s不存在。\n",starg.localtname); EXIT(-1); 
        }

        if (strlen(starg.remotecols)==0)  strcpy(starg.remotecols,tcols.m_allcols.c_str());
        if (strlen(starg.localcols)==0)      strcpy(starg.localcols,tcols.m_allcols.c_str());
    }

    // 业务处理主函数。
    _syncref();
}

// 显示程序的帮助
void _help()
{
    printf("Using:/home/lz6years/backend/myproject/tools/bin/syncref logfilename xmlbuffer\n\n");

    // 不分批同步，把T_ZHOBTCODE1@db128同步到T_ZHOBTCODE2。
    printf("Sample:/home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/tools/bin/syncref /log/idc/syncref_ZHOBTCODE2.log "\
              "\"<localconnstr>idc/root@snorcl11g_121</localconnstr><charset>Simplified Chinese_China.AL32UTF8</charset>"\
              "<linktname>T_ZHOBTCODE1@db128</linktname><localtname>T_ZHOBTCODE2</localtname>"\
              "<remotecols>obtid,cityname,provname,lat,lon,height,upttime,keyid</remotecols>"\
              "<localcols>stid,cityname,provname,lat,lon,height,upttime,recid</localcols>"\
              "<rwhere>where obtid like '57%%%%'</rwhere><lwhere>where stid like '57%%%%'</lwhere>"
              "<synctype>1</synctype><timeout>50</timeout><pname>syncref_ZHOBTCODE2</pname>\"\n\n");

    // 分批同步，把T_ZHOBTCODE1@db128同步到T_ZHOBTCODE3。
    // 因为测试的需要，xmltodb程序每次会删除T_ZHOBTCODE1@db128中的数据，全部的记录重新入库，keyid会变。
    // 所以以下脚本不能用keyid，要用obtid，用keyid会出问题，可以试试。
    printf("       /home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/tools/bin/syncref /log/idc/syncref_ZHOBTCODE3.log "\
              "\"<localconnstr>idc/root@snorcl11g_121</localconnstr><charset>Simplified Chinese_China.AL32UTF8</charset>"\
              "<linktname>T_ZHOBTCODE1@db128</linktname><localtname>T_ZHOBTCODE3</localtname>"\
              "<remotecols>obtid,cityname,provname,lat,lon,height,upttime,keyid</remotecols>"\
              "<localcols>stid,cityname,provname,lat,lon,height,upttime,recid</localcols>"\
              "<rwhere>where obtid like '57%%%%'</rwhere><lwhere>where stid like '57%%%%'</lwhere>"\
              "<synctype>2</synctype><remoteconnstr>idc/root@snorcl11g_121</remoteconnstr>"\
              "<remotetname>T_ZHOBTCODE1</remotetname><remotekeycol>obtid</remotekeycol>"\
              "<localkeycol>stid</localkeycol><keylen>5</keylen>"\
              "<maxcount>10</maxcount><timeout>50</timeout><pname>syncref_ZHOBTCODE3</pname>\"\n\n");

    // 分批同步，把T_ZHOBTMIND1@db128同步到T_ZHOBTMIND2。
    printf("       /home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/tools/bin/syncref /log/idc/syncref_ZHOBTMIND2.log "\
              "\"<localconnstr>idc/root@snorcl11g_121</localconnstr><charset>Simplified Chinese_China.AL32UTF8</charset>"\
              "<linktname>T_ZHOBTMIND1@db128</linktname><localtname>T_ZHOBTMIND2</localtname>"\
              "<remotecols>obtid,ddatetime,t,p,u,wd,wf,r,vis,upttime,keyid</remotecols>"\
              "<localcols>stid,ddatetime,t,p,u,wd,wf,r,vis,upttime,recid</localcols>"\
              "<rwhere>where ddatetime>sysdate-10/1440</rwhere>"\
              "<synctype>2</synctype><remoteconnstr>idc/root@snorcl11g_121</remoteconnstr>"\
              "<remotetname>T_ZHOBTMIND1</remotetname><remotekeycol>keyid</remotekeycol>"\
              "<localkeycol>recid</localkeycol><keylen>15</keylen>"\
              "<maxcount>10</maxcount><timeout>50</timeout><pname>syncref_ZHOBTMIND2</pname>\"\n\n");

    printf("本程序是共享平台的公共功能模块，采用刷新的方法同步Oracle数据库之间的表。\n\n");

    printf("logfilename   本程序运行的日志文件。\n");
    printf("xmlbuffer     本程序运行的参数，用xml表示，具体如下：\n\n");

    printf("localconnstr  本地数据库的连接参数，格式：username/passwd@tnsname。\n");
    printf("charset       数据库的字符集，这个参数要与本地和远程数据库保持一致，否则会出现中文乱码的情况。\n");

    printf("linktname      dblink指向的远程表名，如T_ZHOBTCODE1@db128。\n");
    printf("localtname    本地表名，如T_ZHOBTCODE2。\n");

    printf("remotecols    远程表的字段列表，用于填充在select和from之间，所以，remotecols可以是真实的字段，"\
              "也可以是函数的返回值或者运算结果。如果本参数为空，就用localtname表的字段列表填充。\n");
    printf("localcols     本地表的字段列表，与remotecols不同，它必须是真实存在的字段。如果本参数为空，"\
              "就用localtname表的字段列表填充。\n");

    printf("rwhere        同步数据的条件，填充在远程表的查询语句之后，为空则表示同步全部的记录。\n");
    printf("lwhere        同步数据的条件，填充在本地表的删除语句之后，为空则表示同步全部的记录。\n");

    printf("synctype      同步方式：1-不分批刷新；2-分批刷新。\n");
    printf("remoteconnstr 远程数据库的连接参数，格式与localconnstr相同，当synctype==2时有效。\n");
    printf("remotetname   远程表名，当synctype==2时有效。\n");
    printf("remotekeycol  远程表的键值字段名，必须是唯一的，当synctype==2时有效。\n");
    printf("localkeycol   本地表的键值字段名，必须是唯一的，当synctype==2时有效。\n");
    printf("keylen        键值字段的长度，当synctype==2时有效。\n");

    printf("maxcount      每批执行一次同步操作的记录数，当synctype==2时有效。\n");

    printf("timeout       本程序的超时时间，单位：秒，视数据量的大小而定，建议设置30以上。\n");
    printf("pname         本程序运行时的进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
    printf("注意：\n"\
              "1）remotekeycol和localkeycol字段的选取很重要，如果是自增字段，那么在远程表中数据生成后自增字段的值不可改变，否则同步会失败；\n"\
              "2）当远程表中存在delete操作时，无法分批刷新，因为远程表的记录被delete后就找不到了，无法从本地表中执行delete操作。\n\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(const char *strxmlbuffer)
{
    memset(&starg,0,sizeof(struct st_arg));

    // 本地数据库的连接参数，格式：ip,username,password,dbname,port。
    getxmlbuffer(strxmlbuffer,"localconnstr",starg.localconnstr,100);
    if (strlen(starg.localconnstr)==0) { logfile.write("localconnstr is null.\n"); return false; }

    // 数据库的字符集，这个参数要与远程数据库保持一致，否则会出现中文乱码的情况。
    getxmlbuffer(strxmlbuffer,"charset",starg.charset,50);
    if (strlen(starg.charset)==0) { logfile.write("charset is null.\n"); return false; }

    // linktname表名。
    getxmlbuffer(strxmlbuffer,"linktname",starg.linktname,30);
    if (strlen(starg.linktname)==0) { logfile.write("linktname is null.\n"); return false; }

    // 本地表名。
    getxmlbuffer(strxmlbuffer,"localtname",starg.localtname,30);
    if (strlen(starg.localtname)==0) { logfile.write("localtname is null.\n"); return false; }

    // 远程表的字段列表，用于填充在select和from之间，所以，remotecols可以是真实的字段，也可以是函数
    // 的返回值或者运算结果。如果本参数为空，将用localtname表的字段列表填充。
    getxmlbuffer(strxmlbuffer,"remotecols",starg.remotecols,1000);

    // 本地表的字段列表，与remotecols不同，它必须是真实存在的字段。如果本参数为空，将用localtname表的字段列表填充。
    getxmlbuffer(strxmlbuffer,"localcols",starg.localcols,1000);

    // 同步数据的条件。
    getxmlbuffer(strxmlbuffer,"rwhere",starg.rwhere,1000);
    getxmlbuffer(strxmlbuffer,"lwhere",starg.lwhere,1000);

    // 同步方式：1-不分批刷新；2-分批刷新。
    getxmlbuffer(strxmlbuffer,"synctype",starg.synctype);
    if ( (starg.synctype!=1) && (starg.synctype!=2) ) { logfile.write("synctype is not in (1,2).\n"); return false; }

    if (starg.synctype==2)
    {
        // 远程数据库的连接参数，格式与localconnstr相同，当synctype==2时有效。
        getxmlbuffer(strxmlbuffer,"remoteconnstr",starg.remoteconnstr,100);
        if (strlen(starg.remoteconnstr)==0) { logfile.write("remoteconnstr is null.\n"); return false; }

        // 远程表名，当synctype==2时有效。
        getxmlbuffer(strxmlbuffer,"remotetname",starg.remotetname,30);
        if (strlen(starg.remotetname)==0) { logfile.write("remotetname is null.\n"); return false; }

        // 远程表的键值字段名，必须是唯一的，当synctype==2时有效。
        getxmlbuffer(strxmlbuffer,"remotekeycol",starg.remotekeycol,30);
        if (strlen(starg.remotekeycol)==0) { logfile.write("remotekeycol is null.\n"); return false; }

        // 本地表的键值字段名，必须是唯一的，当synctype==2时有效。
        getxmlbuffer(strxmlbuffer,"localkeycol",starg.localkeycol,30);
        if (strlen(starg.localkeycol)==0) { logfile.write("localkeycol is null.\n"); return false; }

        // 键值字段的大小。
        getxmlbuffer(strxmlbuffer,"keylen",starg.keylen);
        if (starg.keylen==0) { logfile.write("keylen is null.\n"); return false; }

        // 每批执行一次同步操作的记录数，当synctype==2时有效。
        getxmlbuffer(strxmlbuffer,"maxcount",starg.maxcount);
        if (starg.maxcount==0) { logfile.write("maxcount is null.\n"); return false; }
    }

    // 本程序的超时时间，单位：秒，视数据量的大小而定，建议设置30以上。
    getxmlbuffer(strxmlbuffer,"timeout",starg.timeout);
    if (starg.timeout==0) { logfile.write("timeout is null.\n"); return false; }

    // 本程序运行时的进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。
    getxmlbuffer(strxmlbuffer,"pname",starg.pname,50);
    if (strlen(starg.pname)==0) { logfile.write("pname is null.\n"); return false; }

    return true;
}

void EXIT(int sig)
{
    logfile.write("程序退出，sig=%d\n\n",sig);

    connloc.disconnect();

    exit(0);
}

// 业务处理主函数。
bool _syncref()
{
    ctimer timer;

    sqlstatement stmtdel(&connloc);    // 删除本地表中记录的SQL语句。
    sqlstatement stmtins(&connloc);    // 向本地表中插入数据的SQL语句。

    // 不分批刷新，适用于数据量不大（百万行以下）的场景，远程表中的增加、修改和删除操作都可以同步到本地表。
    // delete from T_ZHOBTCODE2 where stid like '57%';
    // insert into T_ZHOBTCODE2(stid,cityname,provname,lat,lon,height,upttime,recid)
    //        select obtid,cityname,provname,lat,lon,height,upttime,keyid from T_ZHOBTCODE1@db128 where obtid like '57%';
    if (starg.synctype==1)
    {
        logfile.write("sync %s to %s ...",starg.linktname,starg.localtname);

        // 先删除本地表starg.localtname中满足starg.lwhere条件的记录。
        stmtdel.prepare("delete from %s %s",starg.localtname,starg.lwhere);
        if (stmtdel.execute()!=0)
        {
            logfile.write("stmtdel.execute() failed.\n%s\n%s\n",stmtdel.sql(),stmtdel.message()); return false;
        }

        // 再把远程表starg.linktname中满足start.rwhere条件的记录插入到本地表starg.localtname。
        stmtins.prepare("insert into %s(%s) select %s from %s %s",starg.localtname,starg.localcols,starg.remotecols,starg.linktname,starg.rwhere);
        if (stmtins.execute()!=0)
        {
            logfile.write("stmtins.execute() failed.\n%s\n%s\n",stmtins.sql(),stmtins.message()); 
            connloc.rollback();     // 如果这里失败了，可以不用回滚事务，connection类的析构函数会回滚。
            return false;
        }

        logfile << " " << stmtins.rpc() << " rows in " << timer.elapsed() << "sec.\n";

        connloc.commit();

        return true;
    }

    // 分批次刷新，适用于数据量较大 ( 分批刷新 将 T_ZHOBTCODE1表中的内容同步到 T_ZHOBTCODE3)
    // select rowid from T_ZHOBTCODE1 where obtid like '54%';
    // delete from T_ZHOBTCODE3 where rowid in('AAAVV8AAFAAAAEUAB6', 'AAAVV8AAFAAAAEUAB7');
    // insert into T_ZHOBTCODE3(stid, cityname, provname, lat, lon, height, upttime, recid)
    //     select obtid, cityname, provname, lat, lon, height, upttime, keyid from T_ZHOBTCODE1@db12
    //            where rowid in( : 1, : 2, : 3, ..., : starg.maxcount);
    if(starg.synctype == 2)
    {
        // 连接到远程库
        if(connrem.connecttodb(starg.remoteconnstr, starg.charset) != 0){
            logfile.write("connrem.connecttodb(%s) failed.\n", starg.remoteconnstr);
            return false;
        }

        // 用于保存select返回的内容
        char tmpvalue[starg.keylen + 1];

        // 准备select远程数据库的内容
        sqlstatement stmtsel;
        stmtsel.connect(&connrem);
        stmtsel.prepare(sformat("select %s from %s %s", starg.remotekeycol, starg.remotetname, starg.rwhere));
        stmtsel.bindout(1, tmpvalue, starg.keylen);

        char keyvalues[starg.maxcount][starg.keylen + 1];
        // 准备bind的 : 1, : 2, : 3, ..., : starg.maxcount内容
        string strbind;
        for (int i = 1; i <= starg.maxcount; i++){
            strbind += sformat(":%d,", i);
        }
        deleterchr(strbind, ','); 


        // 准备删除语句的内容
        stmtdel.prepare(sformat("delete from %s where %s in(%s)", starg.localtname, starg.localkeycol, strbind.c_str()));
        for(int i = 1; i <= starg.maxcount; ++i)
            stmtdel.bindin(i, keyvalues[i - 1]);

        // 准备插入的语句
        stmtins.prepare(sformat("insert into %s(%s) select %s from %s where %s in(%s)",
                        starg.localtname, starg.localcols, starg.remotecols, starg.linktname, starg.remotekeycol, strbind.c_str()));
        for (int i = 1; i <= starg.maxcount; ++i)
            stmtins.bindin(i, keyvalues[i - 1]);

        // 执行sel语句
        if(stmtsel.execute() != 0){
            logfile.write("stmtsel.execute() failed.\n%s\n%s\n", stmtsel.sql(), stmtsel.message());
            return false;
        }

        int cnt = 0;
        memset(keyvalues, 0, sizeof keyvalues);

        // logfile.write("select: %s.\n delete: %s.\n insert: %s.\n", stmtsel.sql(), stmtdel.sql(), stmtins.sql());

        logfile.write("sync %s to %s ...", starg.linktname, starg.localtname);

        while(true)
        {
            memset(tmpvalue, 0, sizeof tmpvalue);
            
            if(stmtsel.next() != 0)
                break;

            // stmtsel.next()

            strcpy(keyvalues[cnt], tmpvalue);
            cnt++;

            if(cnt >= starg.maxcount)
            {
                // 执行删除后插入
                // 从本地表中删除记录。
                if (stmtdel.execute() != 0)
                {
                    // 执行从本地表中删除记录的操作一般不会出错。
                    // 如果报错，就肯定是数据库的问题或同步的参数配置不正确，流程不必继续。
                    logfile.write("stmtdel.execute() failed.\n%s\n%s\n", stmtdel.sql(), stmtdel.message());
                    return false;
                }

                // 向本地表中插入记录。
                if (stmtins.execute() != 0)
                {
                    // 执行向本地表中插入记录的操作一般不会出错。
                    // 如果报错，就肯定是数据库的问题或同步的参数配置不正确，流程不必继续。
                    logfile.write("stmtins.execute() failed.\n%s\n%s\n", stmtins.sql(), stmtins.message());
                    return false;
                }

                // logfile.write("sync %s to %s(%d rows) in %.2fsec.\n",starg.linktname,starg.localtname,cnt,timer.elapsed());

                connloc.commit();

                cnt = 0; // 记录从结果集中已获取记录的计数器。
                memset(keyvalues, 0, sizeof(keyvalues));

                pactive.uptatime();
            }
        }

        // 如果cnt>0，表示还有没同步的记录，再执行一次同步操作。
        if (cnt > 0)
        {
            // 从本地表中删除记录。
            if (stmtdel.execute() != 0)
            {
                logfile.write("stmtdel.execute() failed.\n%s\n%s\n", stmtdel.sql(), stmtdel.message());
                return false;
            }

            // 向本地表中插入记录。
            if (stmtins.execute() != 0)
            {
                logfile.write("stmtins.execute() failed.\n%s\n%s\n", stmtins.sql(), stmtins.message());
                return false;
            }

            // logfile.write("sync %s to %s(%d rows) in %.2fsec.\n",starg.linktname,starg.localtname,cnt,timer.elapsed());

            connloc.commit();
        }

        logfile << " " << stmtsel.rpc() << " rows in " << timer.elapsed() << "sec.\n";

    }

    return true;
}
