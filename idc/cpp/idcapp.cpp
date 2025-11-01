/****************************************************************************************/
/*   程序名：idcapp.cpp，此程序是共享平台项目公用函数和类的定义文件。                    */     
/****************************************************************************************/
#include "idcapp.h"


//  把从文件读到的一行数据拆分到m_zhobtmind结构体中。
bool CZHOBTMIND::splitbuffer(const string &buffer,const bool bisxml)
{
    memset(&m_zhobtmind,0,sizeof m_zhobtmind);

    if(bisxml)  //解析xml文件
    {
        //将xml中的内容读取到结构体中
        getxmlbuffer(buffer,"obtid",m_zhobtmind.obtid,5);
        getxmlbuffer(buffer,"ddatetime",m_zhobtmind.ddatetime,14);
        //下面的写法是为了防止其中某一行中，有部分行不含有其内容
        char tmp[11];
        getxmlbuffer(buffer,"t",tmp,10);     if (strlen(tmp)>0) snprintf(m_zhobtmind.t,10,"%d",atoi(tmp));
        getxmlbuffer(buffer,"p",tmp,10);    if (strlen(tmp)>0) snprintf(m_zhobtmind.p,10,"%d",atoi(tmp));
        getxmlbuffer(buffer,"u",m_zhobtmind.u,10);
        getxmlbuffer(buffer,"wd",m_zhobtmind.wd,10);
        getxmlbuffer(buffer,"wf",tmp,10);  if (strlen(tmp)>0) snprintf(m_zhobtmind.wf,10,"%d",atoi(tmp));
        getxmlbuffer(buffer,"r",tmp,10);     if (strlen(tmp)>0) snprintf(m_zhobtmind.r,10,"%d",atoi(tmp));
        getxmlbuffer(buffer,"vis",tmp,10);  if (strlen(tmp)>0) snprintf(m_zhobtmind.vis,10,"%d",atoi(tmp));
    
    }
    else    //解析csv文件
    {
        ccmdstr cmdstr;
        cmdstr.splittocmd(buffer,",");
        cmdstr.getvalue(0,m_zhobtmind.obtid,5);
        cmdstr.getvalue(1,m_zhobtmind.ddatetime,14);
        char tmp[11];
        cmdstr.getvalue(2,tmp,10); if (strlen(tmp)>0) snprintf(m_zhobtmind.t,10,"%d",atoi(tmp));
        cmdstr.getvalue(3,tmp,10); if (strlen(tmp)>0) snprintf(m_zhobtmind.p,10,"%d",atoi(tmp));
        cmdstr.getvalue(4,m_zhobtmind.u,10);
        cmdstr.getvalue(5,m_zhobtmind.wd,10);
        cmdstr.getvalue(6,tmp,10); if (strlen(tmp)>0) snprintf(m_zhobtmind.wf,10,"%d",atoi(tmp));
        cmdstr.getvalue(7,tmp,10); if (strlen(tmp)>0) snprintf(m_zhobtmind.r,10,"%d",atoi(tmp));
        cmdstr.getvalue(8,tmp,10); if (strlen(tmp)>0) snprintf(m_zhobtmind.vis,10,"%d",atoi(tmp));
    }

    m_buffer = buffer;

    return true;
}

// 把m_zhobtmind结构体中的数据插入到T_ZHOBTMIND表中。
bool CZHOBTMIND::inserttable()
{
    //若没有与数据库进行交互连接
    if(!m_stmt.isopen())
    {
        m_stmt.connect(&m_conn);
        //将动态sql语句准备与地址绑定
        m_stmt.prepare("insert into T_ZHOBTMIND(obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid) "\
                                "values(:1,to_date(:2,'yyyymmddhh24miss'),:3*10,:4*10,:5,:6,:7*10,:8*10,:9*10,SEQ_ZHOBTMIND.nextval)");
            
        //将stmt动态操作的地址空间绑定
        m_stmt.bindin(1,m_zhobtmind.obtid,5);
        m_stmt.bindin(2,m_zhobtmind.ddatetime,20);
        m_stmt.bindin(3,m_zhobtmind.t,10);
        m_stmt.bindin(4,m_zhobtmind.p,10);
        m_stmt.bindin(5,m_zhobtmind.u,10);
        m_stmt.bindin(6,m_zhobtmind.wd,10);
        m_stmt.bindin(7,m_zhobtmind.wf,10);
        m_stmt.bindin(8,m_zhobtmind.r,10);
        m_stmt.bindin(9,m_zhobtmind.vis,10);
    }

    //执行sql操作，将数据插入  注意：其返回值为0时，表示插入成功
    if(m_stmt.execute() != 0)
    {
        //若错误代码为1，表示：之前有数据了。
        //其他情况则为出错
        if(m_stmt.rc() != 1)
        {
            m_logfile.write("m_buffer=%s\n",m_buffer.c_str());
            m_logfile.write("m_stmt.execute() failed.\n%s\n%s\n",m_stmt.sql(),m_stmt.message());
        }

        return false;
    }

    return true;
}