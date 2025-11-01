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

all:demo02 demo03 demo04 pactiveserver client server

demo02:demo02.cpp
	g++ -g -o demo02 demo02.cpp $(PUBINCL) $(PUBCPP)

demo03:demo03.cpp
	g++ -g -o demo03 demo03.cpp $(PUBINCL) $(PUBCPP)

demo04:demo04.cpp
	g++ -g -o demo04 demo04.cpp $(PUBINCL) $(PUBCPP)

pactiveserver:pactiveserver.cpp
	g++ -g -o pactiveserver pactiveserver.cpp $(PUBINCL) $(PUBCPP)

server:server.cpp
	g++ $(CFLAGS) -o server server.cpp $(PUBINCL) $(PUBCPP)

client:client.cpp
	g++ $(CFLAGS) -o client client.cpp $(PUBINCL) $(PUBCPP)

clean:
	rm -f demo02 demo03 demo04 pactiveserver server client

