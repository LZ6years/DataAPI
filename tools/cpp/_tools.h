#ifndef _TOOLS_H
#define _TOOLS_H

#include "_public.h"
#include "_ooci.h"
using namespace idc;

class ctcols
{
    struct st_columns
    {
        char colname[31];  // 列名
        char datatype[31]; // 列的数据类型, 分为char,number,data三大类
        int collen;        // 列长
        int pkseq;         // ruo列是主键的字段，存放主键字段的顺序，从1开始，不是主键：0
    };
    
public:
    ctcols();              // 默认构造函数

    vector<struct st_columns> m_vallcols; // 存放所有的字段
    vector<struct st_columns> m_vpkcols;  // 存放主键字段

    string m_allcols; // 存放所有字段name的连接，中间用 “,” 连接
    string m_pkcols;  // 存放主键字段name的连接，中间用 “,” 连接

    void initdata(); // 成员变量初始化。

    // 获取指定表的全部字段信息。
    bool allcols(connection &conn, char *tablename);

    // 获取指定表的主键字段信息。
    bool pkcols(connection &conn, char *tablename);
};

#endif