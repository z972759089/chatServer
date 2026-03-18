# chatServer
基于muduo网络库实现的集群聊天服务器和客户端源码，nginx tcp负载均衡，redis

编译方法：
执行命令 ./autobuild.sh 自动编译脚本

需要配置nginx tcp负载均衡：
命令vim nginx.conf 进入配置页面，添加如下配置
events {
    worker_connections  1024;
}
stream{

        upstream MyServer{
                server 192.168.150.128:6100 weight=1 max_fails=3 fail_timeout=30s;
                server 192.168.150.128:6002 weight=1 max_fails=3 fail_timeout=30s;
        }
        server{
                proxy_connect_timeout 1s;
               # proxy_timeout 3s;
                listen 8000;
                proxy_pass MyServer;
                tcp_nodelay on;
        }
}
<img width="1161" height="795" alt="image" src="https://github.com/user-attachments/assets/e6279079-ee76-4804-bc31-9caa83b1a348" />


