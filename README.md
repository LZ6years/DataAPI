# 一、数据处理与传送
## 1.1 数据处理小工具

**（1）gzipfiles** 

功能：压缩文件夹下指定的内容

启动方式：
```bash
./gzipfiles pathname matchstr timeout
./gzipfiles /log/idc "*.log.20*" 0.02
```

**（2）deletefiles** 

功能：删除文件夹下指定的内容

启动方式：
```
./deletefiles pathname matchstr timeout
./deletefiles /tmp/idc/surfdata "*.csv,*.xml,*.json" 0.02
```

## 1.2 数据传输工具

### 1.2.1 FPT工具

**（1）ftpgetfiles** 

功能：从FTP服务器获取文件

启动方式：
```bash
./ftpgetfiles /log/idc/ftpgetfiles_surfdata.log xmlbuffer
./ftpgetfiles /log/idc/ftpgetfiles_surfdata.log "<host>192.168.171.121:21</host><mode>1</mode><username>lz6years</username><password>root</password><localpath>/idcdata/surfdata</localpath><remotepath>/tmp/idc/surfdata</remotepath><matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname><listfilename>/idcdata/ftplist/ftpgetfiles_surfdata.list</listfilename><ptype>1</ptype><okfilename>/idcdata/ftplist/ftpgetfiles_surfdata.xml</okfilename><checkmtime>true</checkmtime><timeout>80</timeout><pname>ftpgetfiles_surfdata</pname>"
```

**（2）ftpputfiles** 

功能：从FTP服务器获取文件

启动方式：
```bash
./ftpputfiles /log/idc/ftpgetfiles_surfdata.log xmlbuffer
./ftpputfiles /log/idc/ftpputfiles_surfdata.log "<host>192.168.171.121:21</host><mode>1</mode><username>lz6years</username><password>root</password><localpath>/tmp/idc/surfdata</localpath><remotepath>/tmp/ftpputest</remotepath><matchname>SURF_ZH*.JSON</matchname><ptype>1</ptype><okfilename>/idcdata/ftplist/ftpputfiles_surfdata.xml</okfilename><timeout>80</timeout><pname>ftpputfiles_surfdata</pname>"
```

### 1.2.2 TCP工具

**（1）fileserver**

功能：tcp文件服务端(异步传送)

启动方式：
```bash
./fileserver port logfile
./fileserver 5005 /log/idc/fileserver.log
```

**（2）tcpputfiles** 

功能：向TCP服务器上传文件的客户端(异步传送)

启动方式：
```bash
./tcpputfiles logfile xmlbuffer
./tcpputfiles /log/idc/tcpputfiles_surfdata.log "<ip>192.168.171.121</ip><port>5005</port><ptype>1</ptype><clientpath>/tmp/ftpputest</clientpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><srvpath>/tmp/tcpputest</srvpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_surfdata</pname>"
```

**（3）tcpgetfiles** 

功能：从TCP服务器下载文件的客户端(异步传送)

启动方式：
```bash
./tcpgetfiles logfile xmlbuffer
./tcpgetfiles /log/idc/tcpgetfiles_surfdata.log "<ip>192.168.171.121</ip><port>5005</port><ptype>1</ptype><srvpath>/tmp/tcpputest</srvpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><clientpath>/tmp/tcpgetest</clientpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpgetfiles_surfdata</pname>"
```

# 二、数据与数据库模块

## 2.1 数据入库

文件：xmltodb

功能：将inifilename路径下的配置信息入库(xml  -> db)
```bash
./xmltodb logfile xmlbuffer
./xmltodb /log/idc/xmltodb_vip.log "<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset><inifilename>/home/lz6years/backend/myproject/idc/ini/xmltodb.xml</inifilename><xmlpath>/idcdata/xmltodb/vip</xmlpath><xmlpathbak>/idcdata/xmltodb/vipbak</xmlpathbak><xmlpatherr>/idcdata/xmltodb/viperr</xmlpatherr><timetvl>5</timetvl><timeout>50</timeout><pname>xmltodb_vip</pname>"
```

inifilename文件实例：
```xml
<?xml version='1.0' encoding='utf-8'?>

<!--该参数文件存放了共享平台的入库参数。-->
<!-- 
  filename: 匹配的xml文件格式
  tname: 匹配的表名
  uptbz: 更新标志：1-更新; 2-不更新;
  execsql: 处理xml文件之前，执行的SQL语句
-->
<xmltodb>
  <filename>ZHOBTCODE_*.XML</filename><tname>T_ZHOBTCODE1</tname><uptbz>2</uptbz><execsql>delete from T_ZHOBTCODE1</execsql></endl>
  <filename>ZHOBTMIND_*.XML</filename><tname>T_ZHOBTMIND1</tname><uptbz>1</uptbz></endl>
</xmltodb>

```

## 2.2 数据抽取

文件：dmingoracle

功能：将数据库中信息抽取到xml文件中(db -> xml)【支持增量抽取】
```bash
./xmltodb logfile xmlbuffer
./dminingoracle /log/idc/dminingoracle_ZHOBTMIND.log 
"<connstr>idc/root@snorcl11g_121</connstr><charset>Simplified Chinese_China.AL32UTF8</charset><selectsql>select obtid,to_char(ddatetime,'yyyymmddhh24miss'),t,p,u,wd,wf,r,vis,keyid from T_ZHOBTMIND where keyid>:1 and obtid like '5%%'</selectsql><fieldstr>obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid</fieldstr><fieldlen>5,19,8,8,8,8,8,8,8,15</fieldlen><bfilename>ZHOBTMIND</bfilename><efilename>togxpt</efilename><outpath>/idcdata/dmindata</outpath><starttime></starttime><incfield>keyid</incfield><incfilename>/idcdata/dmining/dminingoracle_ZHOBTMIND_togxpt.keyid</incfilename><timeout>30</timeout><pname>dminingoracle_ZHOBTMIND_togxpt</pname><maxcount>1000</maxcount><connstr1>scott/root@snorcl11g_121</connstr1>"
```

## 2.3 数据迁移

文件：migratetable

功能：将一个数据库中，表中的部分内容迁移到另一个表中(db -> db)
```bash
./migratetable logfile xmlbuffer
./migratetable /log/idc/migratetable_ZHOBTMIND1.log "<connstr>idc/root@snorcl11g_121</connstr><tname>T_ZHOBTMIND1</tname><totname>T_ZHOBTMIND1_HIS</totname><keycol>rowid</keycol><where>where ddatetime<sysdate-0.5</where><maxcount>100</maxcount><starttime></starttime><timeout>120</timeout><pname>migratetable_ZHOBTMIND1</pname>"
```
## 2.4 数据同步

**（1）syncref**

功能：采用刷新的方法同步Oracle数据库之间的表
```bash
./syncref logfile xmlbuffer
./syncref /log/idc/syncref_ZHOBTMIND2.log "<localconnstr>idc/root@snorcl11g_121</localconnstr><charset>Simplified Chinese_China.AL32UTF8</charset><linktname>T_ZHOBTMIND1@db128</linktname><localtname>T_ZHOBTMIND2</localtname><remotecols>obtid,ddatetime,t,p,u,wd,wf,r,vis,upttime,keyid</remotecols><localcols>stid,ddatetime,t,p,u,wd,wf,r,vis,upttime,recid</localcols><rwhere>where ddatetime>sysdate-10/1440</rwhere><synctype>2</synctype><remoteconnstr>idc/root@snorcl11g_121</remoteconnstr><remotetname>T_ZHOBTMIND1</remotetname><remotekeycol>keyid</remotekeycol><localkeycol>recid</localkeycol><keylen>15</keylen><maxcount>10</maxcount><timeout>50</timeout><pname>syncref_ZHOBTMIND2</pname>"
```

**（2）syncinc**

功能：采用增量的方法同步Oracle数据库之间的表
```bash
./syncinc logfile xmlbuffer
./syncinc /log/idc/syncinc_ZHOBTMIND3.log "<localconnstr>idc/root@snorcl11g_121</localconnstr><remoteconnstr>idc/root@snorcl11g_121</remoteconnstr><charset>Simplified Chinese_China.AL32UTF8</charset><remotetname>T_ZHOBTMIND1</remotetname><lnktname>T_ZHOBTMIND1@db128</lnktname><localtname>T_ZHOBTMIND3</localtname><remotecols>obtid,ddatetime,t,p,u,wd,wf,r,vis,upttime,keyid</remotecols><localcols>stid,ddatetime,t,p,u,wd,wf,r,vis,upttime,recid</localcols><where>and obtid like '54%%'</where><remotekeycol>keyid</remotekeycol><localkeycol>recid</localkeycol><maxcount>300</maxcount><timetvl>2</timetvl><timeout>30</timeout><pname>syncinc_ZHOBTMIND3</pname>"
```

# 三、网络部署模块

## 3.1 正向代理

文件：inetd

功能：正向代理程序

```bash
./inetd logfile inifile
./inetd /tmp/inetd.log /etc/inetd.conf
```

/etc/inetd.conf：
```conf
# 入端口 出IP 出端口 
5038 192.168.171.121 5008
5022 192.168.171.121 22
5536 192.168.171.121 1536
  22 192.168.171.121 5022
```

## 3.2 反向代理

（1）rinetd

功能：反向代理程序【外网端】
```bash
./inetd logfile inifile cmdport
./rinetd /log/rinetd/rinetd.log /etc/rinetd.conf 5001
```

/etc/rinetd.conf：
```conf
#入端口      进IP      进端口 
5122 192.168.171.121 22
5138 192.168.171.121 5008
5080 192.168.171.121 80
```

（2）rinetdin

功能：反向代理程序【内网端】
```bash
./rinetdin logfile ip port
./rinetdin /log/rinetd/rinetdin.log 192.168.171.121 5001
```

## 3.3 web接口

文件： webserver

功能：基于WEB的数据访问接口模块
```bash
./webserver logfile port
./webserver /log/idc/webserver.log 5088
```
