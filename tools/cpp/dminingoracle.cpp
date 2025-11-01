/*
    程序名:dminingoracle.cpp 本程序是数据中心的公共功能模块，用于从Oracle数据库源表抽取数据，生成xml文件。
*/

#include "_ooci.h"
#include "_public.h"
using namespace idc;

// 程序运行参数的结构体。
struct st_arg
{
    char connstr[101];        // 数据库的连接参数。
    char charset[51];         // 数据库的字符集。
    char selectsql[1024];     // 从数据源数据库抽取数据的SQL语句。
    char fieldstr[501];        // 抽取数据的SQL语句输出结果集字段名，字段名之间用逗号分隔。
    char fieldlen[501];       // 抽取数据的SQL语句输出结果集字段的长度，用逗号分隔。
    char bfilename[31];     // 输出xml文件的前缀。
    char efilename[31];     // 输出xml文件的后缀。
    char outpath[256];      // 输出xml文件存放的目录。
    int  maxcount;           // 输出xml文件最大记录数，0表示无限制。
    char starttime[52];      // 程序运行的时间区间
    char incfield[31];         // 递增字段名。
    char incfilename[256]; // 已抽取数据的递增字段最大值存放的文件。
    char connstr1[101];     // 已抽取数据的递增字段最大值存放的数据库的连接参数。
    int  timeout;              // 进程心跳的超时时间。
    char pname[51];          // 进程名，建议用"dminingoracle_后缀"的方式。
} starg;

void EXIT(int sig);
void _help();
bool _xmltoarg(const char* strxmlbuffer);  //用来将xml内容翻译到参数文件中
bool instarttime();     //判断是否在当前时间段内
bool _dminingoracle();  //将数据主内文件传到xml文件的主函数

clogfile logfile;
connection conn; //数据库连接对象
ccmdstr fieldname;  //用来存结果集的数组
ccmdstr fieldlen;   //用来存取每个对应结果集长度的数组

// 1）从文件或数据库中获取上次已抽取数据的增量字段的最大值；（如果是第一次执行抽取任务，增量字段的最大值为0）
// 2）绑定输入变量（已抽取数据的增量字段的最大值）；
// 3）获取结果集的时候，要把增量字段的最大值保存在全局变量imaxincvalue中；
// 4）抽取完数据之后，把i全局变量imaxincvalue中的增量字段的最大值保存在文件或数据库中。
long imaxincvalue;         // 递增字段的最大值。
int  incfieldpos = -1;        // 递增字段在结果集数组中的位置。
bool readincfield();        // 从数据库表中或starg.incfilename文件中加载上次已抽取数据的最大值。
bool writeincfield();       // 把已抽取数据的最大值写入数据库表或starg.incfilename文件。

int main(int argc,char* argv[])
{
    if(argc != 3){_help();return -1;}

    // 关闭全部的信号和输入输出。
    // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
    // 但请不要用 "kill -9 +进程号" 强行终止。
    //closeioandsignal(true); 
    signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

    //打开日志文件
    if(!logfile.open(argv[1]))
    {
        printf("logfile.open(%s) failed\n",argv[1]);
    }

    //解析第三个参数中的xml内容
    if(!_xmltoarg(argv[2])){ logfile.write("参数解析失败!\n"); EXIT(-1);}

    //判断当前时间是否为程序运行的时间区间内
    if(!instarttime()){logfile.write("instarttime() failed\n");}

    //打开数据库
    if(conn.connecttodb(starg.connstr,starg.charset) != 0)
    {
        logfile.write("connect database(%s) failed\n%s\n",starg.connstr,conn.message());
    }
    logfile.write("connect database(%s) ok.\n",starg.connstr);

    if(!readincfield()) EXIT(-1);

    //将数据库中的内容，存放到xml文件中
    _dminingoracle();

    return 0;
}


void EXIT(int sig)
{
    logfile.write("收到%d的信号,dminingoracle退出\n");

    exit(0);
}

//以下代码注意：为什么like后面为：5%%%%，正常sql查询只需要一个就可以，
//原因：%%%%由于转义，代表%% stmt.prepare(const char *fmt)，内部也为printf,也需要转移，所以%%变为%
void _help()
{
    printf("\n");
    printf("Using:./dminingoracle logfilename xmlbuffer\n\n");

    printf("Sample:/home/lz6years/backend/myproject/tools/bin/procctl 3600 /home/lz6years/backend/myproject/tools/bin/dminingoracle /log/idc/dminingoracle_ZHOBTCODE.log "
              "\"<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset>"\
              "<selectsql>select obtid,cityname,provname,lat,lon,height from T_ZHOBTCODE where obtid like '5%%%%'</selectsql>"\
              "<fieldstr>obtid,cityname,provname,lat,lon,height</fieldstr><fieldlen>5,30,30,10,10,10</fieldlen>"\
              "<bfilename>ZHOBTCODE</bfilename><efilename>togxpt</efilename><outpath>/idcdata/dmindata</outpath>"\
              "<timeout>30</timeout><pname>dminingoracle_ZHOBTCODE</pname>\"\n\n");
    printf("       /home/lz6years/backend/myproject/tools/bin/procctl 30 /home/lz6years/backend/myproject/tools/bin/dminingoracle /log/idc/dminingoracle_ZHOBTMIND.log "\
              "\"<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset>"\
              "<selectsql>select obtid,to_char(ddatetime,'yyyymmddhh24miss'),t,p,u,wd,wf,r,vis,keyid from T_ZHOBTMIND where keyid>:1 and obtid like '5%%%%'</selectsql>"\
              "<fieldstr>obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid</fieldstr><fieldlen>5,19,8,8,8,8,8,8,8,15</fieldlen>"\
              "<bfilename>ZHOBTMIND</bfilename><efilename>togxpt</efilename><outpath>/idcdata/dmindata</outpath>"\
              "<starttime></starttime><incfield>keyid</incfield>"\
              "<incfilename>/idcdata/dmining/dminingoracle_ZHOBTMIND_togxpt.keyid</incfilename>"\
              "<timeout>30</timeout><pname>dminingoracle_ZHOBTMIND_togxpt</pname>"\
              "<maxcount>1000</maxcount><connstr1>scott/root@snorcl11g_121</connstr1>\"\n\n");

    printf("本程序是共享平台的公共功能模块，用于从Oracle数据库源表抽取数据，生成xml文件。\n");
    printf("logfilename 本程序运行的日志文件。\n");
    printf("xmlbuffer   本程序运行的参数，用xml表示，具体如下：\n\n");

    printf("connstr     数据源数据库的连接参数，格式：username/passwd@tnsname。\n");
    printf("charset     数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的情况。\n");
    printf("selectsql   从数据源数据库抽取数据的SQL语句，如果是增量抽取，一定要用递增字段作为查询条件，如where keyid>:1。\n");
    printf("fieldstr    抽取数据的SQL语句输出结果集的字段名列表，中间用逗号分隔，将作为xml文件的字段名。\n");
    printf("fieldlen    抽取数据的SQL语句输出结果集字段的长度列表，中间用逗号分隔。fieldstr与fieldlen的字段必须一一对应。\n");
    printf("outpath     输出xml文件存放的目录。\n");
    printf("bfilename   输出xml文件的前缀。\n");
    printf("efilename   输出xml文件的后缀。\n"); 
    printf("maxcount    输出xml文件的最大记录数，缺省是0，表示无限制，如果本参数取值为0，注意适当加大timeout的取值，防止程序超时。\n");
    printf("starttime   程序运行的时间区间，例如02,13表示：如果程序启动时，踏中02时和13时则运行，其它时间不运行。"\
         "如果starttime为空，表示不启用，只要本程序启动，就会执行数据抽取任务，为了减少数据源数据库压力"\
         "抽取数据的时候，如果对时效性没有要求，一般在数据源数据库空闲的时候时进行。\n");
    printf("incfield    递增字段名，它必须是fieldstr中的字段名，并且只能是整型，一般为自增字段。"\
          "如果incfield为空，表示不采用增量抽取的方案。");
    printf("incfilename 已抽取数据的递增字段最大值存放的文件，如果该文件丢失，将重新抽取全部的数据。\n");
    printf("connstr1    已抽取数据的递增字段最大值存放的数据库的连接参数。connstr1和incfilename二选一，connstr1优先。");
    printf("timeout     本程序的超时时间，单位：秒。\n");
    printf("pname       进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n\n");
    
}
bool _xmltoarg(const char* strxmlbuffer)  //用来将xml内容翻译到参数文件中
{
    memset(&starg,0,sizeof starg);

    getxmlbuffer(strxmlbuffer,"connstr",starg.connstr,100);        // 数据库的连接参数
    if(strlen(starg.connstr) == 0) {logfile.write("connstr is null\n"); return false;}

    getxmlbuffer(strxmlbuffer,"charset",starg.charset,50);         // 数据库的字符集。
    if (strlen(starg.charset)==0) { logfile.write("charset is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer,"selectsql",starg.selectsql,1000);  // 从数据源数据库抽取数据的SQL语句。
    if (strlen(starg.selectsql)==0) { logfile.write("selectsql is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer,"fieldstr",starg.fieldstr,500);          // 结果集字段名列表。
    if (strlen(starg.fieldstr)==0) { logfile.write("fieldstr is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer,"fieldlen",starg.fieldlen,500);         // 结果集字段长度列表。
    if (strlen(starg.fieldlen)==0) { logfile.write("fieldlen is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer,"bfilename",starg.bfilename,30);   // 输出xml文件的前缀。
    if (strlen(starg.bfilename)==0) { logfile.write("bfilename is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer,"efilename",starg.efilename,30);    // 输出xml文件的后缀。
    if (strlen(starg.efilename)==0) { logfile.write("efilename is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer,"outpath",starg.outpath,255);        // 输出xml文件的目录。
    if (strlen(starg.outpath)==0) { logfile.write("outpath is null.\n"); return false; }

    getxmlbuffer(strxmlbuffer,"maxcount",starg.maxcount);       // 输出xml文件的最大记录数，可选参数。

    getxmlbuffer(strxmlbuffer,"starttime",starg.starttime,50);     // 程序运行的时间区间，可选参数。

    getxmlbuffer(strxmlbuffer,"incfield",starg.incfield,30);          // 递增字段名，可选参数。

    getxmlbuffer(strxmlbuffer,"incfilename",starg.incfilename,255);  // 已抽取数据的递增字段最大值存放的文件，可选参数。

    getxmlbuffer(strxmlbuffer,"connstr1",starg.connstr1,100);          // 已抽取数据的递增字段最大值存放的数据库的连接参数，可选参数。

    getxmlbuffer(strxmlbuffer,"timeout",starg.timeout);       // 进程心跳的超时时间。
    if (starg.timeout==0) { logfile.write("timeout is null.\n");  return false; }

    getxmlbuffer(strxmlbuffer,"pname",starg.pname,50);     // 进程名。
    if (strlen(starg.pname)==0) { strncpy(starg.pname,"dminingoracle",50); }

    // 拆分starg.fieldstr到fieldname中。
    fieldname.splittocmd(starg.fieldstr,",");

    // 拆分starg.fieldlen到fieldlen中。
    fieldlen.splittocmd(starg.fieldlen,",");

    // 判断fieldname和fieldlen两个数组的大小是否相同。
    if (fieldlen.size() != fieldname.size())
    {
        logfile.write("fieldstr和fieldlen的元素个数不一致。\n"); return false;
    }

    // 如果是增量抽取，incfilename和connstr1必二选一。
    if (strlen(starg.incfield) > 0)
    {
        if ( (strlen(starg.incfilename)==0) && (strlen(starg.connstr1)==0) )
        {
            logfile.write("如果是增量抽取，incfilename和connstr1必二选一，不能都为空。\n"); return false;
        }
    }

    return true;
}

bool instarttime() //判断是否在当前时间段内
{
    //当starttime没有填值的话
    if(strlen(starg.starttime) != 0)
    {
        //当有starttime规定时
        string now = ltime1("hh24");     //获取当前时间的小时
        if(strstr(starg.starttime,now.c_str())) return false;

    }

    return true;
}

bool _dminingoracle()  //将数据主内文件传到xml文件的主函数
{
    sqlstatement stmt(&conn);
    //准备sql语句
    stmt.prepare(starg.selectsql);

    //将sql语句绑定输出的内存区域
    string strfieldvalue[fieldname.size()];  //定义一个string数组，用来绑定输出的内存
    for(int i = 1; i <= fieldname.size(); ++i)
    {
        stmt.bindout(i,strfieldvalue[i - 1],stoi(fieldlen[i - 1]));
    }

     //若为增量抽取，需要绑定一个变量
    if(strlen(starg.incfield) != 0 && incfieldpos != -1) stmt.bindin(1,imaxincvalue);

    //执行sql语句
    if(stmt.execute() != 0)
    {
        logfile.write("stmt.execute(%s) failed\n",starg.selectsql);
    }

    //拼接要打开的文件
    string strxmlfilename;
    int iseq = 1;       //输出数据库文件的编号
    cofile ofile;

    //打开文件，将sql语句写入其中
    while (true)
    {
        //获取这个记录的值
        if(stmt.next() != 0) break;

        if(!ofile.isopen())
        {
            strxmlfilename = sformat("%s/%s_%s_%s_%d.xml",starg.outpath,starg.bfilename,ltime1("yyyymmddhh24miss").c_str(),starg.efilename,iseq++);
            if(!ofile.open(strxmlfilename))
            {
                logfile.write("ofile.open(%s) failed\n",strxmlfilename.c_str());
            }
            ofile.writeline("<data>\n");  //写入内容的开始标志
        }

        //将记录的值输出到文件中
        for(int i = 1; i <= fieldname.size(); ++i)
        {
            ofile.writeline("<%s>%s</%s>",fieldname[i-1].c_str(),strfieldvalue[i-1].c_str(),fieldname[i-1].c_str());
        }
        ofile.writeline("</endl>\n");  //将行尾结束标志加入

        //判断是否插入记录达到maxcount上限  stmt.rpc()中存的，已经操作了多少的记录
        if(starg.maxcount > 0 && (stmt.rpc() % starg.maxcount) == 0)
        {
            ofile.writeline("</data>\n");

            //达到上限，关闭文件，循环回去打开另一个文件
            if (!ofile.closeandrename())
            {
                logfile.write("ofile.closeandrename(%s) failed.\n",strxmlfilename.c_str()); return false;
            }

            logfile.write("生成文件%s(%d)。\n",strxmlfilename.c_str(),starg.maxcount);
        }

        // 更新递增字段的最大值。
        // if ( (strlen(starg.incfield)!=0) && (imaxincvalue<stol(strfieldvalue[incfieldpos])) )
        //    imaxincvalue=stol(strfieldvalue[incfieldpos]);
        //将此次记录的值imaxincvalue更新
        if((strlen(starg.incfield) != 0) && (imaxincvalue < stol(strfieldvalue[incfieldpos]))) imaxincvalue = stol(strfieldvalue[incfieldpos]);
    }

    // 5）如果maxcount==0或者向xml文件中写入的记录数不足maxcount，关闭文件。
    if (ofile.isopen())
    {
        ofile.writeline("</data>\n");        // 写入数据集结束的标签。
        if (ofile.closeandrename()==false)
        {
            logfile.write("ofile.closeandrename(%s) failed.\n",strxmlfilename.c_str()); return false;
        }

        if (starg.maxcount==0)
            logfile.write("生成文件%s(%d)。\n",strxmlfilename.c_str(),stmt.rpc());
        else
            logfile.write("生成文件%s(%d)。\n",strxmlfilename.c_str(),stmt.rpc() % starg.maxcount);
    }

    //将记录的值更新到数据库或文件中
    if(stmt.rpc() > 0) writeincfield();

    return true;
}


bool readincfield()        // 从数据库表中或starg.incfilename文件中加载上次已抽取数据的最大值。
{
    imaxincvalue = 0;

    if(strlen(starg.incfield) == 0) return true;  //若不采用增量抽取方式，直接返回

    //查找，增量字段名在fieldname数组中的位置
    for(int i = 0; i < fieldname.size(); ++i){
        if(fieldname[i] == starg.incfield) {incfieldpos = i; break;}
    }

    //若没有找到，则返回false
    if(incfieldpos == -1){
        logfile.write("递增字段名%s不在列表%s中。\n",starg.incfield,starg.fieldstr); return false;
    }

    //若采用数据库的的方式
    if(strlen(starg.connstr1) != 0)
    {
        connection conn1;
        //首先连接数据库
        if(conn1.connecttodb(starg.connstr1,starg.charset) != 0)
        {
            logfile.write("connect database(%s) failed\n%s\n",starg.connstr1,conn1.message());return false;
        }

        sqlstatement stmt(&conn1);
        stmt.prepare("select maxincvalue from T_MAXINCVALUE where pname = :1");
        stmt.bindin(1,starg.pname);
        stmt.bindout(1,imaxincvalue);
        stmt.execute();  //可能，这是第一次执行，所以这里不需要报错与返回
        stmt.next();        
    }
    else //采用文件存储的方式
    {
        cifile ifile;

        if(!ifile.open(starg.incfilename))
        {
            logfile.write("ifile.open(%s) failed\n",starg.incfilename);
        }

        //将文件内内容读出，并保存到变量中
        string buffer;
        ifile.readline(buffer);

        imaxincvalue = stol(buffer);
    }
    
    logfile.write("上次已抽取数据的位置（%s=%ld）。\n",starg.incfield,imaxincvalue);
    
    return true;
}

bool writeincfield()       // 把已抽取数据的最大值写入数据库表或starg.incfilename文件。
{
    if(strlen(starg.incfield) == 0) return true;

    //写入数据库
    if(strlen(starg.connstr1) != 0)
    {
        connection conn1;
        //连接数据库
        if(conn1.connecttodb(starg.connstr1,starg.charset) != 0)
        {
            logfile.write("connect database(%s) failed\n%s\n",starg.connstr1,conn1.message());return false;
        }

        //准备写入的sql语句
        sqlstatement stmt(&conn1);
        stmt.prepare("update T_MAXINCVALUE set maxincvalue = :1 where pname = :2");
        stmt.bindin(1,imaxincvalue);
        stmt.bindin(2,starg.pname,50);
        if(stmt.execute() != 0)
        {
            //更新不成功，可能为表不存在，直接创建表，并插入数据即可
            if(stmt.rc() == 942)
            {
                // 如果表不存在，就创建表，然后插入记录。
                conn1.execute("create table T_MAXINCVALUE(pname varchar2(50),maxincvalue number(15),primary key(pname))");
                conn1.execute("insert into T_MAXINCVALUE values('%s',%ld)",starg.pname,imaxincvalue);
                conn1.commit();
                return true;
            }
            else
            {
                logfile.write("stmt.execute() failed.\n%s\n%s\n",stmt.sql(),stmt.message()); return false;
            }
        }
        else
        {
            //即使执行成功，也有可能并没有影响到行(原因：表存在，但是其中没有相应的pname数据)
            if(stmt.rpc() == 0)
            {
                conn1.execute("insert into T_MAXINCVALUE values('%s',%ld)",starg.pname,imaxincvalue);
            }   
            conn1.commit();
        }
    }
    else
    {
        //文件方式
        cofile ofile;

        if(!ofile.open(starg.incfilename))
        {
            logfile.write("ofile.open(%s) failed\n",starg.incfilename);
        }

        //将文件内内容写出，并保存到文件中
        ofile.writeline("%ld",imaxincvalue);
    }

    return true;
}
