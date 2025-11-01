/*
 *  程序名:blobtofile.cpp，此程序演示开发框架操作Oracle数据库（将数据库中的BLOB数据,保存到磁盘文件中）。
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
    
    //得到表格中的pic字段
    stmt.prepare("select pic from girls where id = 1");
    stmt.bindblob();    //对blob字段进行绑定,这里就是将blob的内容绑定到一块内存区域

     // 执行SQL语句，一定要判断返回值，0-成功，其它-失败。
    if (stmt.execute() != 0)
    {
        printf("stmt.execute() failed.\n%s\n%s\n",stmt.sql(),stmt.message()); return -1;
    }

    // 获取一条记录，一定要判断返回值，0-成功，1403-无记录，其它-失败,从刚才绑定的内存中获得其select结果值,保存到statement结构体中
    if (stmt.next() != 0) return 0;

    // 把磁盘文件pic_in.jpeg的内容写入BLOB字段，一定要判断返回值，0-成功，其它-失败。
    if (stmt.lobtofile("/home/lz6years/backend/myproject/public/db/oracle/pic_out.jpeg") != 0)
    {
        printf("stmt.filetolob() failed.\n%s\n",stmt.message()); return -1;
    }

    printf("数据库的BLOB字段二进制文件已存入。\n");

    conn.commit();

}