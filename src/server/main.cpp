#include"chatserver.hpp"
#include"chatservice.hpp"
#include<iostream>
#include<signal.h>
using namespace std;

//处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);  
}

int main(int argc,char **argv)
{
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    signal(SIGINT,resetHandler);//用于设置 SIGINT 信号（通常由 Ctrl+C 触发）的处理器。当接收到 SIGINT 信号时，会调用 resetHandler 函数

    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server( &loop,addr,"ChatServer");

    server.start();
    loop.loop();

    return 0;

}