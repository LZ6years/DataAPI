# 此shell文件用于启动所有的程序

# 启动守护进程模块
# /home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/tools/bin/checkproc /home/lz6years/backend/tmp/log/checkproc.log

# 启动其他进程
# 启动生成测试数据的进程
/home/lz6years/backend/myproject/tools/bin/procctl 60 /home/lz6years/backend/myproject/idc/bin/crtsurfdata /home/lz6years/backend/myproject/idc/ini/stcode.ini /tmp/idc/surfdata /log/idc/crtsurfdata.log csv,xml,json

# 启动删除文件（/tmp/idc/surfdata）的进程
/home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/deletefiles /tmp/idc/surfdata "*.csv,*.xml,*.json" 0.02

# 压缩日志文件（/log/idc）
/home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/gzipfiles /log/idc "*.log.20*" 0.02

# 从/tmp/idc/surfdata目录下载原始的气象观测数据(.csv .xml)文件，存放在/idcdata/surfdata目录。
/home/lz6years/backend/myproject/tools/bin/procctl 30 /home/lz6years/backend/myproject/tools/bin/ftpgetfiles /log/idc/ftpgetfiles_surfdata.log "<host>192.168.171.121:21</host><mode>1</mode><username>lz6years</username><password>root</password><localpath>/idcdata/surfdata</localpath><remotepath>/tmp/idc/surfdata</remotepath><matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname><listfilename>/idcdata/ftplist/ftpgetfiles_surfdata.list</listfilename><ptype>1</ptype><okfilename>/idcdata/ftplist/ftpgetfiles_surfdata.xml</okfilename><checkmtime>true</checkmtime><timeout>80</timeout><pname>ftpgetfiles_surfdata</pname>"

# 清理/idcdata/surfdata目录中0.04天之前的文件。
/home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/deletefiles /idcdata/surfdata "*" 0.04

# 把/tmp/idc/surfdata目录的原始气象观测数据文件上传到/tmp/ftpputest目录。
# 注意，先创建好服务端的目录：mkdir /tmp/ftpputest 
/home/lz6years/backend/myproject/tools/bin/procctl 30 /home/lz6years/backend/myproject/tools/bin/ftpputfiles /log/idc/ftpputfiles_surfdata.log "<host>192.168.171.121:21</host><mode>1</mode><username>lz6years</username><password>root</password><localpath>/tmp/idc/surfdata</localpath><remotepath>/tmp/ftpputest</remotepath><matchname>SURF_ZH*.JSON</matchname><ptype>1</ptype><okfilename>/idcdata/ftplist/ftpputfiles_surfdata.xml</okfilename><timeout>80</timeout><pname>ftpputfiles_surfdata</pname>"

# 清理/tmp/ftpputest目录中0.04天之前的文件。
/home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/deletefiles /tmp/ftpputest "*" 0.04

# 文件传输的服务端程序。
/home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/tools/bin/fileserver 5005 /log/idc/fileserver.log

# 把目录/tmp/ftpputest中的文件上传到/tmp/tcpputest目录中。
/home/lz6years/backend/myproject/tools/bin/procctl 20 /home/lz6years/backend/myproject/tools/bin/tcpputfiles /log/idc/tcpputfiles_surfdata.log "<ip>192.168.171.121</ip><port>5005</port><ptype>1</ptype><clientpath>/tmp/ftpputest</clientpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><srvpath>/tmp/tcpputest</srvpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_surfdata</pname>"

# 把目录/tmp/tcpputest中的文件下载到/tmp/tcpgetest目录中。
/home/lz6years/backend/myproject/tools/bin/procctl 20 /home/lz6years/backend/myproject/tools/bin/tcpgetfiles /log/idc/tcpgetfiles_surfdata.log "<ip>192.168.171.121</ip><port>5005</port><ptype>1</ptype><srvpath>/tmp/tcpputest</srvpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><clientpath>/tmp/tcpgetest</clientpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpgetfiles_surfdata</pname>"

# 清理/tmp/tcpgetest目录中的历史数据文件。
/home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/deletefiles /tmp/tcpgetest "*" 0.02

# 把站点参数数据入库到ZHOBTCODE表中，如果站点不存在则插入，站点已存在则更新。
/home/lz6years/backend/myproject/tools/bin/procctl 120 /home/lz6years/backend/myproject/idc/bin/obtcodetodb /home/lz6years/backend/myproject/idc/ini/stcode.ini "idc/root@snorcl11g_121" "Simplified Chinese_China.AL32UTF8" /log/idc/obtcodetodb.log

# 把/idcdata/surfdata目录中的气象观测数据文件入库到T_ZHOBTMIND表中。
/home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/idc/bin/obtmindtodb /idcdata/surfdata "idc/root@snorcl11g_121" "Simplified Chinese_China.AL32UTF8" /log/idc/obtmindtodb.log

# 执行/home/lz6years/backend/myproject/idc/sql/deletetable.sql脚本，删除T_ZHOBTMIND表两小时之前的数据，如果启用了数据清理程序deletetable，就不必启用这行脚本了。
# /home/lz6years/backend/myproject/tools/bin/procctl 120 /oracle/home/bin/sqlplus idc/root@snorcl11g_121 @/home/lz6years/backend/myproject/idc/sql/deletetable.sql

# 每隔1小时把T_ZHOBTCODE表中全部的数据抽取出来。
/home/lz6years/backend/myproject/tools/bin/procctl 3600 /home/lz6years/backend/myproject/tools/bin/dminingoracle /log/idc/dminingoracle_ZHOBTCODE.log "<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset><selectsql>select obtid,cityname,provname,lat,lon,height from T_ZHOBTCODE</selectsql><fieldstr>obtid,cityname,provname,lat,lon,height</fieldstr><fieldlen>10,30,30,10,10,10</fieldlen><bfilename>ZHOBTCODE</bfilename><efilename>toidc</efilename><outpath>/idcdata/dmindata</outpath><timeout>30</timeout><pname>dminingoracle_ZHOBTCODE</pname>"

# 每30秒从T_ZHOBTMIND表中增量抽取数据。
/home/lz6years/backend/myproject/tools/bin/procctl 30 /home/lz6years/backend/myproject/tools/bin/dminingoracle /log/idc/dminingoracle_ZHOBTMIND.log "<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset><selectsql>select obtid,to_char(ddatetime,'yyyymmddhh24miss'),t,p,u,wd,wf,r,vis,keyid from T_ZHOBTMIND where keyid>:1 and obtid like '5%%'</selectsql><fieldstr>obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid</fieldstr><fieldlen>5,19,8,8,8,8,8,8,8,15</fieldlen><bfilename>ZHOBTMIND</bfilename><efilename>togxpt</efilename><outpath>/idcdata/dmindata</outpath><starttime></starttime><incfield>keyid</incfield><incfilename>/idcdata/dmining/dminingoracle_ZHOBTMIND_togxpt.keyid</incfilename><timeout>30</timeout><pname>dminingoracle_ZHOBTMIND_togxpt</pname><maxcount>1000</maxcount><connstr1>scott/root@snorcl11g_121</connstr1>"

# 清理/idcdata/dmindata目录中文件，防止把空间撑满。
# /home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/deletefiles /idcdata/dmindata "*" 0.02

# 把/idcdata/dmindata目录中的xml文件发送到/idcdata/xmltodb/vip，交给入库程序。
# /home/lz6years/backend/myproject/tools/bin/procctl 20 /home/lz6years/backend/myproject/tools/bin/tcpputfiles /log/idc/tcpputfiles_togxpt.log "<ip>127.0.0.1</ip><port>5005</port><ptype>1</ptype><clientpath>/idcdata/dmindata</clientpath><srvpath>/idcdata/xmltodb/vip</srvpath><andchild>true</andchild><matchname>*.XML</matchname><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_togxpt</pname>"

# 把/idcdata/xmltodb/vip目录中的xml文件入库到T_ZHOBTCODE1和T_ZHOBTMIND1。
/home/lz6years/backend/myproject/tools/bin/procctl 10 /home/lz6years/backend/myproject/tools/bin/xmltodb /log/idc/xmltodb_vip.log "<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset><inifilename>/home/lz6years/backend/myproject/idc/ini/xmltodb.xml</inifilename><xmlpath>/idcdata/xmltodb/vip</xmlpath><xmlpathbak>/idcdata/xmltodb/vipbak</xmlpathbak><xmlpatherr>/idcdata/xmltodb/viperr</xmlpatherr><timetvl>5</timetvl><timeout>50</timeout><pname>xmltodb_vip</pname>"
# 注意，观测数据源源不断的入库到T_ZHOBTMIND1中，为了防止表空间被撑满，在/home/lz6years/backend/myproject/idc/sql/deletetable.sql中要配置清理T_ZHOBTMIND1表中历史数据的脚本。

# 清理/idcdata/xmltodb/vipbak和/idcdata/xmltodb/viperr目录中文件。
/home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/deletefiles /idcdata/xmltodb/vipbak "*" 0.02
/home/lz6years/backend/myproject/tools/bin/procctl 300 /home/lz6years/backend/myproject/tools/bin/deletefiles /idcdata/xmltodb/viperr  "*" 0.02

# 清理T_ZHOBTMIND表中1天之前的数据。
/home/lz6years/backend/myproject/tools/bin/procctl 3600 /home/lz6years/backend/myproject/tools/bin/deletetable /log/idc/deletetable_ZHOBTMIND.log "<connstr>idc/root@snorcl11g_121</connstr><tname>T_ZHOBTMIND</tname><keycol>rowid</keycol><where>where ddatetime<sysdate-1</where><maxcount>100</maxcount><timeout>120</timeout><pname>deletetable_ZHOBTMIND</pname>"

# 把T_ZHOBTMIND1表中0.5天之前的数据迁移到T_ZHOBTMIND1_HIS表
/home/lz6years/backend/myproject/tools/bin/procctl 3600 /home/lz6years/backend/myproject/tools/bin/migratetable /log/idc/migratetable_ZHOBTMIND1.log "<connstr>idc/root@snorcl11g_121</connstr><tname>T_ZHOBTMIND1</tname><totname>T_ZHOBTMIND1_HIS</totname><keycol>rowid</keycol><where>where ddatetime<sysdate-0.5</where><maxcount>100</maxcount></starttime><timeout>120</timeout><pname>migratetable_ZHOBTMIND1</pname>"

# 清理T_ZHOBTMIND1_HIS表中1天之前的数据。
/home/lz6years/backend/myproject/tools/bin/procctl 3600 /home/lz6years/backend/myproject/tools/bin/deletetable /log/idc/deletetable_ZHOBTMIND1_HIS.log "<connstr>idc/root@snorcl11g_121</connstr><tname>T_ZHOBTMIND1_HIS</tname><keycol>rowid</keycol><where>where ddatetime<sysdate-1</where><maxcount>100</maxcount><timeout>120</timeout><pname>deletetable_ZHOBTMIND1_HIS</pname>"
