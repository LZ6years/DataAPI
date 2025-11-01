/*
    程序名:crtsurfdata.cpp 用于生成气象站点观测的分钟数据
*/
#include "_public.h"
using namespace idc;

clogfile logfile;     // 定义生成日志文件的对象

//定义站点数据的结构体
/*data:
省   站号  站名 纬度   经度  海拔高度
安徽,58015,砀山,34.27,116.2,44.2
*/
struct st_stcode
{
    char provname[31];  //省份
    char obtid[11];     //站号
    char obtname[31];   //站名:同城市名
    double lat;         //纬度:度
    double lon;         //经度:度
    double height;      //海拔高度:米
};
list<struct st_stcode> stlist; //用来保存每一个数据的容器
//功能:用来加载数据的函数
//inifile:表示传入的文件名
bool loadstcode(const string &inifile);

//气象台观测结构体
struct st_surfdata
{
    char obtid[11];        //站号
    char ddatetime[15];    //数据时间,格式yyyymmddhh24miss
    int t;                 //温度:0.1摄氏度
    int p;                 //气压:0.1百帕
    int u;                 //相对湿度:0-100以内
    int wd;                //风向:0-360之间的值
    int wf;                //风速:单位0.1m/s
    int r;                 //降雨量:0.1mm
    int vis;               //能见度:0.1米
};
list<struct st_surfdata> datalist;      //保存观测数据的容器
void ctrsurfdata();                     //生成测试数据的代码
char strdatetime[15];                   //用来记录现在的时间

//outpath:输出文件所在的文件夹
//datafmt:输出文件的格式
bool crtsurffile(const string& outpath,const string &datafmt);                     //生成输出文件内容的函数

void EXIT(int sig);          //定义遇到信号后退出的函数

//给进程增加心跳
cpactive pactive;

int main(int argc,char* argv[])
{
    //三个参数的含义: 1.站点的参数文件 2.生成测试文件存放的目录 3.本程序运行日志 4.输出文件的格式
    if(argc != 5){
        //若参数非法,给出测试数据存放的目录
        cout << "Using:./crtsurfdata inifile outpath logfile datafmt\n";
        cout << "Examples:/home/lz6years/backend/myproject/tools/bin/procctl 60 /home/lz6years/backend/myproject/idc/bin/crtsurfdata /home/lz6years/backend/myproject/idc/ini/stcode.ini /tmp/idc/surfdata /log/idc/crtsurfdata.log csv,xml,json\n\n";

        cout << "此程序应该由调度程序启动,间隔建议为60秒";
        cout << "inifile 气象站点参数文件名\n";
        cout << "outpath 气象站点数据文件存放目录\n";
        cout << "logfile 本程序运行的日志文件名\n";
        cout << "datafmt 生成数据文件的格式csv,xml,json 若要生成多个,用\",\"分隔 ";

        return -1;
    }

    //将所有信号与IO都关掉
    closeioandsignal(true);
    //只接受2与15的信号
    signal(SIGINT,EXIT);signal(SIGTERM,EXIT);

    //给心跳信息初始化
    pactive.addpinfo(10,"crusurfdata");

    // 打开日志文件
    if(!logfile.open(argv[3])){
        cout << "logfile.open(" << argv[3] << ") failed\n";return -1;
    }

    logfile.write("crtsurfdata 开始运行\n");

    memset(strdatetime,0,sizeof strdatetime);
    //得到现在的时间值
    ltime(strdatetime,"yyyymmddhh24miss");
    //将最后的秒变为00
    strncpy(strdatetime + 12,"00",2);

    //这里编写处理业务的代码
    //1.从站点参数文件中加载站点参数,存放于容器中
    if(!loadstcode(argv[1])) EXIT(-1);

    //2.根据站点参数,生成站点观测数据(随机数)
    ctrsurfdata();

    //3.把站点观测数据保存到文件中
    if(strstr(argv[4],"csv")) crtsurffile(argv[2],"csv");
    if(strstr(argv[4],"xml")) crtsurffile(argv[2],"xml");
    if(strstr(argv[4],"json")) crtsurffile(argv[2],"json");


    logfile.write("crtsurfdata 运行结束!\n");
    return 0;
}



void EXIT(int sig)          //定义遇到信号后退出的函数
{
    logfile.write("程序退出,sig=%d\n\n",sig);

    exit(0);
}

bool loadstcode(const string &inifile)  //用来加载数据的函数
{
    logfile.write("开始加载数据......\n");

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
        cmdstr.getvalue(1,stcode.obtid,10);
        cmdstr.getvalue(2,stcode.obtname,30);
        cmdstr.getvalue(3,stcode.lat);
        cmdstr.getvalue(4,stcode.lon);
        cmdstr.getvalue(5,stcode.height);

        // //将其内容显示到日志中
        // logfile.write("省份为:%s,站号为%s,站名为:%s,经度为:%lf,维度为:%lf,高度为:%lf\n",
        //     stcode.provname,stcode.obtid,stcode.obtname,stcode.lat,stcode.lon,stcode.height);

        //保存到列表中
        stlist.push_back(stcode);
    }

    //这里不需要手动的关闭文件,ifile析构函数会帮我们自动的关闭文件

    //返回成功
    return true;    
}

//生成测试数据的代码
void ctrsurfdata()
{
    //播随机数种子
    srand(time(0));

    //用来保存surfdata结构体临时对象
    st_surfdata surfdata;

    //遍历stlist的每一行内容
    for(const auto &data:stlist){
        
        //给变量清零
        memset(&surfdata,0,sizeof surfdata);

        //给临时结构体赋值
        strcpy(surfdata.obtid,data.obtid);
        strcpy(surfdata.ddatetime,strdatetime);
        surfdata.t = rand() % 360;
        surfdata.p = rand() % 265 + 10000;
        surfdata.u = rand() % 101;
        surfdata.wd = rand() % 360;
        surfdata.wf = rand() % 150;
        surfdata.r = rand() % 16;
        surfdata.vis = rand() % 5001 + 1000000;

        datalist.push_back(surfdata);
    }

    // for (const auto &data:datalist){
    //     logfile.write("%s %s %.1f %.1f %d %d %.1f %.1f %.1f\n",
    //         data.obtid,data.ddatetime,data.t / 10.0,data.p / 10.0,data.u,data.wd,data.wf /10.0,
    //         data.r/10.0,data.vis/10.0);
    // }
}                  



//outpath:输出文件所在的文件夹
//datafmt:输出文件的格式
bool crtsurffile(const string& outpath,const string &datafmt)                     //生成输出文件内容的函数
{
    //先得到输出文件的文件名  eg:/tmp/idc/surfdata/SURF_ZH_20210629092200_22883.csv
    string filename = outpath + "/" + "SURF_ZH_" + strdatetime + "_" + to_string(getpid()) + "." + datafmt;

    //得到打开文件的对象
    cofile ofile;

    if(!ofile.open(filename)){
        logfile.write("ofile.open(%s) failed",filename.c_str());
    }

    //先写入头
    if(datafmt == "csv") ofile.writeline("站点代码,数据时间,气温,气压,相对湿度,风向,风速,降雨量,能见度\n");
    if(datafmt == "xml") ofile.writeline("<data>\n");
    if(datafmt == "json") ofile.writeline("{\"data\":[\n");

    int count = 0;
    //写入中间部分
    for (const auto &data:datalist){
        if(datafmt == "csv"){
            ofile.writeline("%s,%s,%.1f,%.1f,%d,%d,%.1f,%.1f,%.1f\n",
             data.obtid,data.ddatetime,data.t/10.0,data.p/10.0,data.u,data.wd
             ,data.wf/10.0,data.r/10.0,data.vis/10.0);
        }

        if(datafmt == "xml"){
            ofile.writeline("<obtid>%s</obtid><ddatetime>%s</ddatetime><t>%.1f</t><p>%.1f</p><u>%d</u>"\
                                   "<wd>%d</wd><wf>%.1f</wf><r>%.1f</r><vis>%.1f</vis><endl/>\n",\
                                    data.obtid,data.ddatetime,data.t/10.0,data.p/10.0,
                                    data.u,data.wd,data.wf/10.0,data.r/10.0,data.vis/10.0);

        }

        if(datafmt == "json"){
             ofile.writeline("{\"obtid\":\"%s\",\"ddatetime\":\"%s\",\"t\":\"%.1f\",\"p\":\"%.1f\","\
                                  "\"u\":\"%d\",\"wd\":\"%d\",\"wf\":\"%.1f\",\"r\":\"%.1f\",\"vis\":\"%.1f\"}",\
                                   data.obtid,data.ddatetime,data.t/10.0,data.p/10.0,
                                   data.u,data.wd,data.wf/10.0,data.r/10.0,data.vis/10.0);
            
            // 注意，json文件的最后一条记录不需要逗号，用以下代码特殊处理。
            if (count < datalist.size()-1)
            {   // 如果不是最后一行。
                ofile.writeline(",\n");  count++;
            }
            else
                ofile.writeline("\n");  
        }
    }

    //写入尾部
    if(datafmt == "xml") ofile.writeline("</data>\n");
    if(datafmt == "json") ofile.writeline("]}\n");

    //将写入的信息记录到日志中
    logfile.write("时间:%s 文件%s: 已经写入:%d\n",strdatetime,filename.c_str(),datalist.size());
    
    //注意:ofile的内部用临时文件来操作,需要将其改为正式的文件名
    ofile.closeandrename();
    return true;
}