# 开发框架的头文件
PUBINCL = -I /home/lz6years/backend/myproject/public

# 开发框架的cpp文件名，之间和程序的源代码一起编译
PUBCPP = /home/lz6years/backend/myproject/public/_public.cpp

##################################################
# oracle头文件路径
ORAINCL = -I$(ORACLE_HOME)/rdbms/public -I/home/lz6years/backend/myproject/public/db/oracle

# oracle库文件路径
ORALIB =  -L$(ORACLE_HOME)/lib -L.

# oracle的oci库
ORALIBS = -lclntsh

# 开发框架oracle的cpp文件名，这里直接包含进来，没有采用链接库，是为了方便调试。
ORACPP = /home/lz6years/backend/myproject/public/db/oracle/_ooci.cpp
##################################################

#编译选项
CFLAGS = -g  #供调试时期使用
# CFLAGS = -o2 #供发行时期调用 

all: procctl checkproc deletefiles gzipfiles ftpgetfiles ftpputfiles fileserver tcpputfiles tcpgetfiles\
	dminingoracle xmltodb deletetable migratetable syncref syncinc inetd rinetd rinetdin webserver

procctl:procctl.cpp
	g++ $(CFLAGS) -o procctl procctl.cpp
	cp -f procctl ../bin/.

checkproc:checkproc.cpp
	g++ $(CFLAGS) -o checkproc checkproc.cpp $(PUBINCL) $(PUBCPP)
	cp -f checkproc ../bin/.

deletefiles:deletefiles.cpp
	g++ $(CFLAGS) -o deletefiles deletefiles.cpp $(PUBINCL) $(PUBCPP)
	cp -f deletefiles ../bin/.

gzipfiles:gzipfiles.cpp
	g++ $(CFLAGS) -o gzipfiles gzipfiles.cpp $(PUBINCL) $(PUBCPP)
	cp -f gzipfiles ../bin/.

ftpgetfiles:ftpgetfiles.cpp
	g++ $(CFLAGS) -o ftpgetfiles ftpgetfiles.cpp $(PUBINCL) $(PUBCPP) /home/lz6years/backend/myproject/lib/libftp.a /home/lz6years/backend/myproject/public/_ftp.cpp 
	cp -f ftpgetfiles ../bin/.

ftpputfiles:ftpputfiles.cpp
	g++ $(CFLAGS) -o ftpputfiles ftpputfiles.cpp $(PUBINCL) $(PUBCPP) /home/lz6years/backend/myproject/lib/libftp.a /home/lz6years/backend/myproject/public/_ftp.cpp 
	cp -f ftpputfiles ../bin/.

fileserver:fileserver.cpp
	g++ $(CFLAGS) -o fileserver fileserver.cpp $(PUBINCL) $(PUBCPP)
	cp -f fileserver ../bin/.

tcpputfiles:tcpputfiles.cpp
	g++ $(CFLAGS) -o tcpputfiles tcpputfiles.cpp $(PUBINCL) $(PUBCPP)
	cp -f tcpputfiles ../bin/.

tcpgetfiles:tcpgetfiles.cpp
	g++ $(CFLAGS) -o tcpgetfiles tcpgetfiles.cpp $(PUBINCL) $(PUBCPP)
	cp -f tcpgetfiles ../bin/.

dminingoracle:dminingoracle.cpp
	g++ $(CFLAGS) -o dminingoracle dminingoracle.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL)  $(ORALIB) $(ORALIBS) $(ORACPP)
	cp -f dminingoracle ../bin/.

xmltodb:xmltodb.cpp _tools.cpp
	g++ $(CFLAGS) -o xmltodb xmltodb.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL) $(ORALIB) $(ORALIBS) $(ORACPP) _tools.cpp
	cp -f xmltodb ../bin/.

deletetable:deletetable.cpp
	g++ $(CFLAGS) -o deletetable deletetable.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL)  $(ORALIB) $(ORALIBS) $(ORACPP)
	cp -f deletetable ../bin/.

migratetable:migratetable.cpp _tools.cpp
	g++ $(CFLAGS) -o migratetable migratetable.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL) $(ORALIB) $(ORALIBS) $(ORACPP) _tools.cpp
	cp -f migratetable ../bin/.

syncref:syncref.cpp _tools.cpp
	g++ $(CFLAGS) -o syncref syncref.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL) $(ORALIB) $(ORALIBS) $(ORACPP) _tools.cpp
	cp -f syncref ../bin/.

syncinc:syncinc.cpp _tools.cpp
	g++ $(CFLAGS) -o syncinc syncinc.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL) $(ORALIB) $(ORALIBS) $(ORACPP) _tools.cpp
	cp -f syncinc ../bin/.

inetd:inetd.cpp
	g++ $(CFLAGS) -o inetd inetd.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL)  $(ORALIB) $(ORALIBS) $(ORACPP)
	cp -f inetd ../bin/.

rinetd:rinetd.cpp
	g++ $(CFLAGS) -o rinetd rinetd.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL)  $(ORALIB) $(ORALIBS) $(ORACPP)
	cp -f rinetd ../bin/.

rinetdin:rinetdin.cpp
	g++ $(CFLAGS) -o rinetdin rinetdin.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL)  $(ORALIB) $(ORALIBS) $(ORACPP)
	cp -f rinetdin ../bin/.

webserver:webserver.cpp
	g++ $(CFLAGS) -o webserver webserver.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL)  $(ORALIB) $(ORALIBS) $(ORACPP)
	cp -f webserver ../bin/.

clean:
	rm -rf procctl checkproc deletefiles gzipfiles ftpgetfiles ftpputfiles fileserver tcpputfiles tcpgetfiles
	rm -rf dminingoracle xmltodb deletetable migratetable syncref syncinc inetd rinetd rinetdin webserver

