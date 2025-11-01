/*
 *  程序名:deletetable.cpp，此程序演示开发框架操作Oracle数据库（删除表中数据）。
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
    stmt.prepare("delete from girls where id = '10'");

    if(stmt.execute() != 0)
    {
        printf("%s更新操作执行失败\n返回错误：%s\n",stmt.sql(),stmt.message());
    }

    printf("成功更新%ld行记录\n",stmt.rpc());
    */

  
    //动态SQL语句
    
    long minid = 12;long maxid = 15;
    //准备sql模板语句
    stmt.prepare("delete from girls where id >= :1 and id <= :2");
    
    //对结构体变量进行绑定
    stmt.bindin(1,minid);
    stmt.bindin(2,maxid);


    if(stmt.execute() != 0)
    { 
        printf("%s删除操作执行失败\n返回错误：%s\n",stmt.sql(),stmt.message());
    }
        
    printf("成功删除%ld行记录\n",stmt.rpc());


    conn.commit(); //提交事务
    return 0;
}