/*
 *  程序名：createtable.cpp，此程序演示开发框架操作Oracle数据库（创建表）。
*/
#include "_ooci.h"
using namespace idc;

int main(int argc,char *argv[])
{
    connection conn;  //创建于数据库的连接对象

    //1.登录数据库
    if(conn.connecttodb("scott/root@snorcl11g_121",idc::SIM_AL32UTF8) != 0)
    {
        printf("connect database failed\n %s\n",conn.message());
    }
    printf("connect database ok.\n");

    sqlstatement stmt; // 操作数据库的对象
    
    //2.连接数据库
    stmt.connect(&conn);  // 指定stmt对象使用的数据库连接。

    //(1)准备要操作的sql语句
    string sql = "\
        create table girls(id    number(10),\
                                    name  varchar2(30),\
                                    weight   number(8,2),\
                                    btime date,\
                                    memo  varchar2(300),\
                                    pic   blob,\
                                    primary key (id))";
    
    stmt.prepare(sql);      //(2)准备执行sql语句

    //(3)执行语句
    if(stmt.execute() != 0)
    {
        printf("stmt.execute() failed.\n%s\n%s\n",stmt.sql(),stmt.message()); return -1;
    }
    
    printf("create table girls ok.\n");

    // conn.disconnect();              // 在connection类的析构函数中会自动调用disconnect()方法。


    return 0;
}