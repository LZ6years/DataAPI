/*
 *  程序名:filetoblob.cpp，此程序演示开发框架操作Oracle数据库（将文件以二进制存入数据库中）。
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
    
    //首先先插入一条数据
    stmt.prepare("\
        begin\
        delete from girls where id = 1;\
        insert into girls(id,name,pic) values(1,'冰冰',empty_blob());\
        end;");  // 注意：不可用null代替empty_blob()。
    if (stmt.execute()!=0)
    {
        printf("stmt.execute() failed.\n%s\n%s\n",stmt.sql(),stmt.message()); return -1;
    }
    printf("成功插入%ld条数据\n",stmt.rpc());
    
    //使用游标得到表格中的pic字段
    ////注意:对于blob,clob字段,都需要用游标来获取,由于其数据量相对较大,无法一次性读入内存,需要多次读取,所以用游标
    stmt.prepare("select pic from girls where id = 1 for update");
    stmt.bindblob();    //对blob字段进行绑定,这里就是将blob的内容绑定到一块内存区域

     // 执行SQL语句，一定要判断返回值，0-成功，其它-失败。
    if (stmt.execute() != 0)
    {
        printf("stmt.execute() failed.\n%s\n%s\n",stmt.sql(),stmt.message()); return -1;
    }

    // 获取一条记录，一定要判断返回值，0-成功，1403-无记录，其它-失败,从刚才绑定的内存中获得其select结果值,保存到statement结构体中
    if (stmt.next() != 0) return 0;

    // 把磁盘文件pic_in.jpeg的内容写入BLOB字段，一定要判断返回值，0-成功，其它-失败。
    if (stmt.filetolob("/home/lz6years/backend/myproject/public/db/oracle/pic_in.jpeg") != 0)
    {
        printf("stmt.filetolob() failed.\n%s\n",stmt.message()); return -1;
    }

    printf("二进制文件已存入数据库的BLOB字段中。\n");

    conn.commit();

}