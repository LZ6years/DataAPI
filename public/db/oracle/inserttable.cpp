/*
 *  程序名:inserttable.cpp，此程序演示开发框架操作Oracle数据库（向表中插入数据）。
*/
#include "_ooci.h"
using namespace idc;

int main(int argc,char *argv[])
{
    connection conn;

    //与远程数据库进行
    if(conn.connecttodb("scott/root@snorcl11g_121",idc::SIM_AL32UTF8) != 0)
    {
        printf("connect database failed\n %s\n",conn.message()); return -1;
    }
    printf("connect database ok.\n");

    sqlstatement stmt(&conn);  //与连接上的数据库进行连接

    /*
    //静态SQL语句
    stmt.prepare("insert into girls(id,name,weight,btime,memo) \
                    values('0001','妞妞',120,to_date('2000-09-12 21:23:21','yyyy-mm-dd hh24:mi:ss'),'haha')");

    if(stmt.execute() != 0)
    {
        printf("%s插入操作执行失败\n返回错误：%s\n",stmt.sql(),stmt.message());
    }

    printf("成功插入%ld行记录\n",stmt.rpc());
    */

    //动态SQL语句

    //准备写入的结构体
    struct st_girl
    {
        long id;
        char name[31];      //实际数据库中为30,这里为31表示，表达为字符串时，需要将最后一个字符设为'\0'
        double weight;
        char btime[20];
        char memo[301];
    }stgirl;
    
    //准备sql模板语句
    stmt.prepare("insert into girls(id,name,weight,btime,memo) \
                values(:1,:2,:3,to_date(:4,'yyyy-mm-dd hh24:mi:ss'),:5)");
    
    //对结构体变量进行绑定
    stmt.bindin(1,stgirl.id);
    stmt.bindin(2,stgirl.name,30);
    stmt.bindin(3,stgirl.weight);
    stmt.bindin(4,stgirl.btime,19);
    stmt.bindin(5,stgirl.memo,300);


    //对结构题中的值进行复制狗插入数据
    for(int i = 10; i < 19; ++ i)
    {
        memset(&stgirl,0,sizeof stgirl);

        //给每一个变量赋值
        stgirl.id = i;
        sprintf(stgirl.name,"超女编号%d",i);
        stgirl.weight  = 45.22 + i*2 ;
        sprintf(stgirl.btime,"2000-09-12 21:%d:21",i);
        sprintf(stgirl.memo,"这是'第%05d个超级女生的备注。",i);         // 备注。

        if(stmt.execute() != 0)
        {
            printf("%s插入操作执行失败\n返回错误：%s\n",stmt.sql(),stmt.message());
        }
        
        printf("成功插入%ld行记录\n",stmt.rpc());
    }

    conn.commit(); //提交事务
    return 0;
}