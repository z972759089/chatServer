#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include<mutex>
using namespace std;
using namespace muduo;
using namespace muduo::net;
#include "json.hpp"
using json = nlohmann::json;
#include "usermodel.hpp"
#include"offlinemessagemodel.hpp"
#include"friendmodel.hpp"
#include"groupmodel.hpp"
#include"redis.hpp"

// 表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

// 聊天服务器业务类
class ChatService
{
public:
    static ChatService *instance(); // 获取单例对象的接口函数
    // 处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 创建群组业务
    void creatGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 群聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    //服务器异常后，业务重置
    void reset();
    //从redis中获取订阅信息业务
    void handleRedisSubMsg(int id,string msg);
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);

private:
    ChatService();                                 // 私有化构造函数
    // 存储消息id和对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap; 

    //存储在线用户的通信连接
    unordered_map<int,TcpConnectionPtr> _userConnMap;

    //定义互斥锁，保证 _userConnMap的线程安全
    mutex _connMutex;

    //数据库操作对象
    UserModel _userModel;
    offlineMsgModel _offlineMsgModel;
    friendModel _friendModel;
    GroupModel _groupModel;

    //redis操作对象
    Redis _redis;
};

#endif