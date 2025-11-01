# 用于停止所有服务

# 关闭调度模块
killall -9 procctl

## 关闭其他服务程序
# 关闭生成测试文件的进程
killall crtsurfdata deletefiles gzipfiles ftpgetfiles ftpputfiles fileserver
killall tcpputfiles tcpgetfiles obtcodetodb obtmindtodb dminingoracle xmltodb deletetable migratetable

#睡眠5秒，方便进程的退出工作
sleep 5

# 强制杀死服务程序
killall -9 crtsurfdata deletefiles gzipfiles ftpgetfiles ftpputfiles fileserver
killall -9 tcpputfiles tcpgetfiles obtcodetodb obtmindtodb dminingoracle xmltodb deletetable migratetable
