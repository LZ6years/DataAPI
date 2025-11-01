#include "_tools.h"

// 获取表全部的列和主键列信息的类。
ctcols::ctcols()
{
    initdata(); // 调用成员变量初始化函数。
}

void ctcols::initdata()
{
    m_allcols.clear();
    m_vallcols.clear();
    m_pkcols.clear();
    m_vpkcols.clear();
}

// 获得指定 oracle 库中的某个表的所有字段信息
bool ctcols::allcols(connection &conn, char *tablename)
{
    m_allcols.clear();
    m_vallcols.clear();

    struct st_columns stcolumns;

    sqlstatement stmt;
    stmt.connect(&conn);
    /*从USER_TAB_COLUNMS字典中获取表的全部字段,并注意：
        1) 把结果集转化成小写;
        2) 数据字段中的表命是大写;
    */
    stmt.prepare("select lower(column_name),lower(data_type),lower(data_length)\
            from user_tab_columns\
            where table_name = upper(:1) order by column_id");
    stmt.bindin(1, tablename, 30);
    stmt.bindout(1, stcolumns.colname, 30);
    stmt.bindout(2, stcolumns.datatype, 30);
    stmt.bindout(3, stcolumns.collen);

    if (stmt.execute() != 0)
        return false;

    while (true)
    {
        memset(&stcolumns, 0, sizeof stcolumns);

        if (stmt.next() != 0)
            break;
        // 列的数据类型, 分为char, number, data三大类

        // 字符串类型
        if (strcmp(stcolumns.datatype, "char") == 0)
            strcpy(stcolumns.datatype, "char");
        if (strcmp(stcolumns.datatype, "nchar") == 0)
            strcpy(stcolumns.datatype, "char");
        if (strcmp(stcolumns.datatype, "varchar2") == 0)
            strcpy(stcolumns.datatype, "char");
        if (strcmp(stcolumns.datatype, "nvarchar2") == 0)
            strcpy(stcolumns.datatype, "char");
        if (strcmp(stcolumns.datatype, "rowid") == 0)
        {
            strcpy(stcolumns.datatype, "char");
            stcolumns.collen = 18;
        }

        // 时间类型
        if (strcmp(stcolumns.datatype, "date") == 0)
            stcolumns.collen = 14;

        // 数据类型
        if (strcmp(stcolumns.datatype, "number") == 0)
            strcpy(stcolumns.datatype, "number");
        if (strcmp(stcolumns.datatype, "integer") == 0)
            strcpy(stcolumns.datatype, "number");
        if (strcmp(stcolumns.datatype, "float") == 0)
            strcpy(stcolumns.datatype, "number");

        if (strcmp(stcolumns.datatype, "char") != 0 &&
            strcmp(stcolumns.datatype, "number") != 0 &&
            strcmp(stcolumns.datatype, "date") != 0)
            continue;

        if (strcmp(stcolumns.datatype, "number") == 0)
            stcolumns.collen = 22;

        m_allcols = m_allcols + stcolumns.colname + ",";

        m_vallcols.push_back(stcolumns);
    }

    if (stmt.rc() > 0)
        deleterchr(m_allcols, ',');

    return true;
}

// 获得指定 oracle 中某个表的所有主键信息
bool ctcols::pkcols(connection &conn, char *tablename)
{
    m_pkcols.clear();
    m_vpkcols.clear();
    struct st_columns stcolumns;

    sqlstatement stmt;
    stmt.connect(&conn);
    // 从USER_CONS_COLUMNS和USER_CONSTRAINTS字典中获取表的主键字段，注意：1）把结果集转换成小写；2）数据字典中的表名是大写。
    stmt.prepare("select lower(column_name),position from USER_CONS_COLUMNS\
         where table_name=upper(:1)\
           and constraint_name=(select constraint_name from USER_CONSTRAINTS\
                               where table_name=upper(:2) and constraint_type='P'\
                                 and generated='USER NAME')\
         order by position");
    stmt.bindin(1, tablename, 30);
    stmt.bindin(2, tablename, 30);
    stmt.bindout(1, stcolumns.colname, 30);
    stmt.bindout(2, stcolumns.pkseq);

    if (stmt.execute() != 0)
        return false;

    while (true)
    {
        memset(&stcolumns, 0, sizeof stcolumns);

        if (stmt.next() != 0)
            break;

        m_pkcols = m_pkcols + stcolumns.colname + ",";

        m_vpkcols.push_back(stcolumns);
    }

    if (stmt.rc() > 0)
        deleterchr(m_pkcols, ',');

    // 将在全部字段中的内容更新
    for (auto &aa : m_vallcols){
        for (auto &bb : m_vpkcols){
            if (strcmp(aa.colname, bb.colname) == 0){
                aa.pkseq = bb.pkseq; // 更新m_vallcols中的部分
                // 更新在vpkcols的其余部分
                bb.collen = aa.collen;
                strncpy(bb.datatype, aa.datatype, 30);
                break;
            }
        }
    }

    return true;
}