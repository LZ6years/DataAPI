# 开发框架需要的头文件路径
PUBINCL = -I/home/lz6years/backend/myproject/public

# 开发框架cpp文件名,这里直接包含进来,没有采用链接库,未来方便测试
PUBCPP = /home/lz6years/backend/myproject/public/_public.cpp

################################
# oracle头文件路径
ORAINCL = -I$(ORACLE_HOME)/rdbms/public -I/home/lz6years/backend/myproject/public/db/oracle

# oracle库文件路径
ORALIB =  -L$(ORACLE_HOME)/lib -L.

# oracle的oci库
ORALIBS = -lclntsh

# 开发框架oracle的cpp文件名，这里直接包含进来，没有采用链接库，是为了方便调试。
ORACPP = /home/lz6years/backend/myproject/public/db/oracle/_ooci.cpp
################################

# 编译参数
CFLAGS = -g

all:crtsurfdata obtcodetodb obtmindtodb

# 写入需要编译的对象
crtsurfdata:crtsurfdata.cpp
	g++ $(CFLAGS) -o crtsurfdata crtsurfdata.cpp $(PUBINCL) $(PUBCPP) -lm -lc
	cp -f crtsurfdata ../bin/.

obtcodetodb:obtcodetodb.cpp
	g++ $(CFLAGS) -o obtcodetodb obtcodetodb.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL) $(ORALIB) $(ORALIBS) $(ORACPP) 
	cp obtcodetodb ../bin/.

obtmindtodb:obtmindtodb.cpp idcapp.cpp
	g++ $(CFLAGS) -o obtmindtodb obtmindtodb.cpp idcapp.cpp $(PUBINCL) $(PUBCPP) $(ORAINCL) $(ORALIB) $(ORALIBS) $(ORACPP) 
	cp obtmindtodb ../bin/.

clean:
	rm -f crtsurfdata obtcodetodb obtmindtodb
