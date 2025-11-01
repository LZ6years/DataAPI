/*
    程序名：ftpgetfiles.cpp 用于ftp连接后，下载服务器上的文件
*/
#include "_public.h"
#include "_ftp.h"
using namespace idc;

//对象内容
clogfile logfile;  //日志文件对象
cftpclient ftp;    //ftp对象
cpactive pactive;  //心跳对象

//程序的退出函数，处理2，15的信号
void EXIT(int sig);

//程序的帮助文档
void _help();  

/*
st_arg中的参数内容：
(1)主机名 (2)ftp连接模式[1被动，2主动] (3)用户名 (4)密码 (5)服务器的文件目录 (6)主机从放得到的文件目录 (7)服务器下载文件的名字规则
*/
//xml解析的参数结构体
struct st_arg
{
    char host[31];
    int  mode;
    char username[31];
    char password[31];
    char remotepath[256];
    char localpath[256];
    char matchname[256];
    int  ptype;
    char remotepathbak[256];
    char okfilename[256];
    bool checkmtime;
    int  timeout;
    char pname[51];
    //用来初始化结构体中的内容
    bool init(const string& xmlstr);

    //显示其中所有的内容(用于调试)
    void show();
} starg;

struct st_fileinfo
{
    string filename;  //文件名
    string mtime;     //最后一次修改时间

    //定义拷贝构造函数等
    st_fileinfo() = default;
    st_fileinfo(const string &in_filename,const string & in_mtime):filename(in_filename),mtime(in_mtime) {}

    void clear(){filename.clear();mtime.clear();}
};

map<string,string> mfromok;             // 容器一：存放已下载成功文件，从starg.okfilename参数指定的文件中加载。
list<struct st_fileinfo> vfromnlist;    // 容器二：下载前列出服务端文件名的容器，从nlist文件中加载。
list<struct st_fileinfo> vtook;         // 容器三：本次不需要下载的文件的容器。
list<struct st_fileinfo> vdownload;     // 容器四：本次需要下载的文件的容器。

bool loadokfile();    //将okfilename文件中的内容加入到容器1中
bool compmap();       //将容器1-2对比，得到的信息加入到容器3-4中
bool loadlistfile();  //用来将文件中的数据加载到vfromnlist容器中
bool writetookfile();//将容器3-4的内容写入到okfilename文件中
bool appendtookfile(const st_fileinfo& fileinfo); //下载成功，将其内容加入到okfilename文件中

int main(int argc,char* argv[])
{
    // closeioandsignal(true);  //关闭所有的I/O
    signal(SIGINT,EXIT); signal(SIGTERM,EXIT);  //设置2，15的信号处理

    //1.写出帮助文档
    if(argc != 3){ _help(); return -1;}

    //2.打开日志文件
    if(!logfile.open(argv[1])){
        printf("打开日志文件失败 (%s)。\n",argv[1]); return -1;
    }

    //3.首先解析出xmlbuffer中的内容
    if(!starg.init(argv[2])){
        logfile.write("读取参数失败！");
    }

    // starg.show();  //显示其中所有内容

    pactive.addpinfo(starg.timeout,starg.pname);  //在登录ftp前加入，ftp为网络通讯，可能卡死

    //4.登录ftp客户端
    if(!ftp.login(starg.host,starg.username,starg.password,starg.mode)){
        logfile.write("ftp.login(%s,%s,%s) failed.\n%s\n",starg.host,starg.username,starg.password,ftp.response()); return -1;
    }
    //5.去到ftp相应的文件夹下
    if(!ftp.chdir(starg.remotepath)){
        logfile.write("ftp.chdir(%s) failed.\n%s\n",starg.remotepath,ftp.response()); return -1;
    }

    /*先去到文件夹，在读取的好处：
    1、磁盘I/O读取速度快
    2、这样得到的是文件名称，而直接用nlist读取是全路径名称，这里用文件名就可以*/
    //6.得到所有的文件,并保存在本地文件夹中
    if(!ftp.nlist(".",sformat("/tmp/nlist/ftpgetfiles_%d.nlist",getpid()))){
        logfile.write("ftp.nlist(%s) failed.\n%s\n",starg.remotepath,ftp.response()); return -1;
    }

    pactive.uptatime();

    //7.读取刚刚保存的文件内的名称，并将文件名保存到文件名的容器中
    if(!loadlistfile()){
        logfile.write("loadlistfile() failed.\n\n"); return -1;
    }

    if(starg.ptype == 1){
        
        //将okfilename文件中的内容加入到容器1中
        loadokfile();

        //将容器1-2对比，得到的信息加入到容器3-4中
        compmap();

        //将容器3的内容写入到okfilename文件中
        writetookfile();
    }
    else
        vfromnlist.swap(vdownload);     //为了使下面代码不变

    pactive.uptatime();

    //8.遍历文件容器，读取到local相应的文件夹中
    for(const auto& file : vdownload){
        
        //拼接服务端与客户端的文件全路径
        string strremoteffile = sformat("%s/%s",starg.remotepath,file.filename.c_str());
        string strlocalffile = sformat("%s/%s",starg.localpath,file.filename.c_str());
        
        logfile.write("get：%s ... ",file.filename.c_str());

        if(!ftp.get(strremoteffile,strlocalffile,true)){
            logfile.write("ftp.get(%s) failed\n",strremoteffile.c_str());
        }

        logfile << ". ok.\n";
        
        pactive.uptatime();

        // ptype=1,表示什么也不处理，增量备份
        if(starg.ptype == 1) appendtookfile(file); //下载成功，将其内容加入到okfilename文件中
        //ptype=2,表示删除
        if(starg.ptype == 2){
            if(!ftp.ftpdelete(strremoteffile)){
                logfile.write("ftp.ftpdelete(%s) failed\n",strremoteffile.c_str());
            }
            else{
                logfile.write("ftp.ftpdelete(%s) success\n",strremoteffile.c_str());
            }
        }

        //ptype=3,表示备份拷贝的数据
        if(starg.ptype == 3){

            //拼接备份到的文件夹
            string strremoteffilebak = sformat("%s/%s",starg.remotepathbak,file.filename.c_str());

            if(!ftp.ftprename(strremoteffile,strremoteffilebak)){
                logfile.write("ftp.ftprename(%s) failed reason(%s)\n",strremoteffilebak.c_str(),ftp.response());
            }
            else{
                logfile.write("ftp.ftprename(%s) success\n",strremoteffilebak.c_str());
            }
        }
    }

    return 0;
}

void _help()
{
    printf("\n");
    printf("Using:/home/lz6years/backend/myproject/tools/bin/ftpgetfiles logfilename xmlbuffer\n\n");

    // printf("Sample:/home/lz6years/backend/myproject/tools/bin/procctl 30 /home/lz6years/backend/myproject/tools/bin/ftpgetfiles /log/idc/ftpgetfiles_surfdata.log " \
    //          "\"<host>192.168.171.128:21</host><mode>1</mode>"\
    //          "<username>test01</username><password>root</password>"\
    //          "<remotepath>/home/lz6years/backend/tmp/idc/surfdata</remotepath><localpath>/idcdata/surfdata</localpath>"\
    //          "<matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname>"\
    //          "<ptype>3</ptype><remotepathbak>/home/lz6years/backend/tmp/idc/surfdatabak</remotepathbak>\"\n\n");

    printf("Sample:/home/lz6years/backend/myproject/tools/bin/procctl 30 /home/lz6years/backend/myproject/tools/bin/ftpgetfiles /log/idc/ftpgetfiles_test.log "
           "\"<host>192.168.171.121:21</host><mode>1</mode>"
           "<username>lz6years</username><password>root</password>"
           "<remotepath>/tmp/idc/surfdata</remotepath><localpath>/idcdata/surfdata</localpath>"
           "<matchname>*.xml</matchname>"
           "<ptype>1</ptype><okfilename>/idcdata/ftplist/ftpgetfiles_test.xml</okfilename>"
           "<checkmtime>true</checkmtime>"
           "<timeout>30</timeout><pname>ftpgetfiles_test</pname>\"\n\n\n");

    printf("本程序是通用的功能模块，用于把远程ftp服务端的文件下载到本地目录。\n");
    printf("logfilename是本程序运行的日志文件。\n");
    printf("xmlbuffer为文件下载的参数，如下：\n");
    printf("<host>192.168.171.121:21</host> 远程服务端的IP和端口。\n");
    printf("<mode>1</mode> 传输模式，1-被动模式，2-主动模式，缺省采用被动模式。\n");
    printf("<username>lz6years</username> 远程服务端ftp的用户名。\n");
    printf("<password>root</password> 远程服务端ftp的密码。\n");
    printf("<remotepath>/tmp/idc/surfdata</remotepath> 远程服务端存放文件的目录。\n");
    printf("<localpath>/idcdata/surfdata</localpath> 本地文件存放的目录。\n");
    printf("<matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname> 待下载文件匹配的规则。"\
               "不匹配的文件不会被下载，本字段尽可能设置精确，不建议用*匹配全部的文件。\n");
    printf("<ptype>1</ptype> 文件下载成功后，远程服务端文件的处理方式："\
              "1-什么也不做；2-删除；3-备份，如果为3，还要指定备份的目录。\n");
    printf("<remotepathbak>/home/lz6years/backend/tmp/idc/surfdatabak</remotepathbak> 文件下载成功后，服务端文件的备份目录，"\
              "此参数只有当ptype=3时才有效。\n");
    printf("<okfilename>/idcdata/ftplist/ftpgetfiles_test.xml</okfilename> 已下载成功文件名清单，"\
              "此参数只有当ptype=1时才有效。\n");
    printf("<checkmtime>true</checkmtime> 是否需要检查服务端文件的时间，true-需要，false-不需要，"\
              "此参数只有当ptype=1时才有效，缺省为false。\n");
    printf("<timeout>30</timeout> 下载文件超时时间，单位：秒，视文件大小和网络带宽而定。\n");
    printf("<pname>ftpgetfiles_test</pname> 进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n\n");

}

bool st_arg::init(const string& xmlstr)
{
    //先将结构体中的内容清空
    memset(&starg,0,sizeof starg);

    //解析主机名称与端口
    getxmlbuffer(xmlstr,"host",host,31);
    if(strlen(host) == 0){
        logfile.write("host is null.\n"); return false;
    }

    //解析打开模式，若填错则默认为被动模式
    getxmlbuffer(xmlstr,"mode",mode);
    if(mode != 2){ mode = 1;}

    //解析用户名
    getxmlbuffer(xmlstr,"username",username,31);
    if(strlen(username) == 0){
        logfile.write("username is null.\n"); return false;
    }

    //解析密码
    getxmlbuffer(xmlstr,"password",password,31);
    if(strlen(password) == 0){
        logfile.write("password is null.\n"); return false;
    }

    //解析服务器上的打开目录
    getxmlbuffer(xmlstr,"remotepath",remotepath,255);
    if(strlen(remotepath) == 0){
        logfile.write("remotepath is null.\n"); return false;
    }

    //解析本机上的打开目录
    getxmlbuffer(xmlstr,"localpath",localpath,255);
    if(strlen(localpath) == 0){
        logfile.write("localpath is null.\n"); return false;
    }

    //解析文件匹配的规则
    getxmlbuffer(xmlstr,"matchname",matchname,255);
    if(strlen(matchname) == 0){
        logfile.write("matchname is null.\n"); return false;
    }

    //解析ptype的值
    getxmlbuffer(xmlstr,"ptype",ptype);
    if(ptype != 1 && ptype != 2 && ptype != 3){
        logfile.write("ptype value is error(only 1,2,3).\n"); return false;
    }

    //当ptype=3时，解析器备份的路径
    if(ptype == 3){
        getxmlbuffer(xmlstr,"remotepathbak",remotepathbak,255);
        if(strlen(remotepathbak) == 0){
            logfile.write("remotepathbak is null.\n"); return false;
        }
    }

    if(ptype == 1){
        getxmlbuffer(xmlstr,"okfilename",okfilename,255);
        if(strlen(okfilename) == 0){
            logfile.write("okfilename is null.\n"); return false;
        }
        if(!getxmlbuffer(xmlstr,"checkmtime",checkmtime)){
            checkmtime = true;
        }
    }

    //解析timeout的值
    getxmlbuffer(xmlstr,"timeout",timeout);
    if(timeout == 0){
        logfile.write("timeout value is error.\n"); return false;
    }

    //解析pname的值
    getxmlbuffer(xmlstr,"pname",pname,50);
    if(strlen(pname) == 0){
        strncpy(pname,to_string(getpid()).c_str(),50);
    }

    return true;
}

void st_arg::show()
{
    printf("host=%s, mode=%d, username=%s, password=%s,\nremotepath=%s, localpath=%s,matchname=%s\n, okfilename=%s, checkmtime=%d",
            host,mode,username,password,remotepath,localpath,matchname,okfilename,checkmtime);
}

bool loadlistfile()  //用来将文件中的数据加载到vfromnlist容器中
{
    //1.先将容器的内容清空
    vfromnlist.clear();

    //2.得到文件的对象，并打开文件夹
    cifile ifile;

    if(!ifile.open(sformat("/tmp/nlist/ftpgetfiles_%d.nlist",getpid()))){
        logfile.write("ifile.open(%s) is failed\n",sformat("/tmp/nlist/ftpgetfiles_%d.nlist",getpid()));
        return false;
    }

    string strfilename; //用来保存读取到的文件

    //3.将符合内容的文件保存到容器中
    while (ifile.readline(strfilename))
    {
        if(!matchstr(strfilename,starg.matchname)) continue; //与匹配模式串不同，则跳过

        if(starg.ptype == 1 && starg.checkmtime){  //获取文件的时间
            if(!ftp.mtime(strfilename)){
                logfile.write("ftp.mtime(%s)",strfilename.c_str());return false;
            }
        }

        //将得到的文件加入到容器中
        vfromnlist.emplace_back(strfilename,ftp.m_mtime);  //利用原地构造的方式来
    }

    ifile.closeandremove();

    // for (auto &aa:vfromnlist)
    //    logfile.write("filename=%s,mtime=%s\n",aa.filename.c_str(),aa.mtime.c_str());


    return true;
}

bool loadokfile()    //将okfilename文件中的内容加入到容器1中
{
    if (starg.ptype!=1) return true;

    mfromok.clear();

    cifile ifile;

    // 注意：如果程序是第一次运行，starg.okfilename是不存在的，并不是错误，所以也返回true。
    if (!ifile.open(starg.okfilename)) return true;

    string strbuffer;
    struct st_fileinfo stfileinfo;

    while(ifile.readline(strbuffer))
    {
        stfileinfo.clear();

        getxmlbuffer(strbuffer,"filename",stfileinfo.filename);
        getxmlbuffer(strbuffer,"mtime",stfileinfo.mtime);

        mfromok[stfileinfo.filename]=stfileinfo.mtime;
    }

    // for (auto &aa:mfromok)
    //    logfile.write("filename=%s,mtime=%s\n",aa.first.c_str(),aa.second.c_str());

    return true;
}

bool compmap()       //将容器1-2对比，得到的信息加入到容器3-4中
{
    vtook.clear();
    vdownload.clear();

    // 遍历vfromnlist。
    for (auto &aa:vfromnlist)
    {
        auto it=mfromok.find(aa.filename);           // 在容器一中用文件名查找。
        if (it !=mfromok.end())
        {   // 如果找到了，再判断文件时间。
            if (starg.checkmtime==true)
			{
				// 如果时间也相同，不需要下载，否则需要重新下载。
				if (it->second==aa.mtime) vtook.push_back(aa);    // 文件时间没有变化，不需要下载。
				else vdownload.push_back(aa);     // 需要重新下载。
			}
			else
			{
				vtook.push_back(aa);   // 不需要重新下载。
			}
        }
        else
        {   // 如果没有找到，把记录放入vdownload容器。
            vdownload.push_back(aa);
        }
    }

    // //将两个文件内容输出
    // for(auto bb : vtook){
    //     cout << "vtook: filename = " << bb.filename << "  vtook: mtime = " << bb.mtime << "\n";
    // }

    // for(auto bb : vdownload){
    //     cout << "vdownload: filename = " << bb.filename << "  vdownload: mtime = " << bb.mtime << "\n";
    // }

    return true;
}

bool writetookfile()//将容器3-4的内容写入到okfilename文件中
{
    cofile ofile;
    //以覆盖的方式打开文件
    if(!ofile.open(starg.okfilename)){
        logfile.write("ofile(%s) is failed",starg.okfilename); return false;
    }

    //将两个容器的内容写入到okfilename文件中
    for(auto &aa:vtook) ofile.writeline("<filename>%s</filename><mtime>%s</mtime>\n",aa.filename.c_str(),aa.mtime.c_str());

    ofile.closeandrename();

    return true;
}


bool appendtookfile(const st_fileinfo& fileinfo) //下载成功，将其内容加入到okfilename文件中
{
    cofile ofile;
    //以追加的方式打开文件，注意：对于已经存在的文件不可以用临时文件的方式打开
    if(!ofile.open(starg.okfilename,false,ios::app)){
        logfile.write("ofile(%s) is failed",starg.okfilename); return false;
    }

    //将两个容器的内容写入到okfilename文件中
    ofile.writeline("<filename>%s</filename><mtime>%s</mtime>\n",fileinfo.filename.c_str(),fileinfo.mtime.c_str());    
    
    //因为ofile会自动关闭文件，这里不用写
    return true;
}

void EXIT(int sig)
{
    printf("程序退出，退出信号sig = %d\n\n",sig);

    exit(0);
}

