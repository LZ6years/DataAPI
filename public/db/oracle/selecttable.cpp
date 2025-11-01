/*
 *  程序名:selecttable.cpp，此程序演示开发框架操作Oracle数据库（查询表中数据）。
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

    //动态SQL语句
    
    long minid = 1;long maxid = 18;
    //准备sql模板语句
    stmt.prepare("select id,name,weight,to_char(btime,'yyyy-mm-dd hh24:mi:ss'),memo from girls \
                  where id >= :1 and id <= :2");
    
    //对结构体变量进行绑定
    stmt.bindin(1,minid);
    stmt.bindin(2,maxid);

    //这是查询语句，需要将查询的结果集保存下来到分配的内存中
    struct st_girl
    {
        long id;                 // 超女编号，用long数据类型对应Oracle无小数的number(10)。
        char name[31];     // 超女姓名，用char[31]对应Oracle的varchar2(30)。
        double weight;     // 超女体重，用double数据类型对应Oracle有小数的number(8,2)。
        char btime[20];     // 报名时间，用char对应Oracle的date，格式：'yyyy-mm-dd hh24:mi:ssi'。
        char memo[301];  // 备注，用char[301]对应Oracle的varchar2(300)。
    }stgirl;

    // 把查询语句的结果集与变量的地址绑定，bindout()方法不需要判断返回值。
    stmt.bindout(1,stgirl.id);
    stmt.bindout(2,stgirl.name,30);
    stmt.bindout(3,stgirl.weight);
    stmt.bindout(4,stgirl.btime,19);
    stmt.bindout(5,stgirl.memo,300);

    //执行查询操作
    if(stmt.execute() != 0)
    { 
        printf("%s查询操作执行失败\n返回错误：%s\n",stmt.sql(),stmt.message());
    }

    //将查询出来的结果显示出来
    while (true)
    {
        memset(&stgirl,0,sizeof stgirl);

        if(stmt.next() != 0) break;  //获取内存的一个值

        // 把获取到的记录的值打印出来。
        printf("id=%ld,name=%s,weight=%.02f,btime=%s,memo=%s\n",stgirl.id,stgirl.name,stgirl.weight,stgirl.btime,stgirl.memo);
    }
        
    printf("成功查询%ld行记录\n",stmt.rpc());


    conn.commit(); //提交事务
    return 0;
}