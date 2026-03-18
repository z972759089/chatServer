#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include "usermodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include <vector>
#include <ctime>
using namespace std;
using namespace muduo;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}
// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 登录业务
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    // 注册业务
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    // 注销业务
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    // 一对一聊天业务
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    // 添加好友业务
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    // 创建群聊业务
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::creatGroup, this, _1, _2, _3)});
    // 加入群聊业务
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    // 群聊业务
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubMsg,this,_1,_2));
    }
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个临时 lambda 表达式作为默认处理器,空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务 id pwd
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];
    User user = _userModel.query(id);
    if (user.getId() == id && user.getPassword() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2; // 2表示重复
            response["errmsg"] = "该账号已经登录，请重新输入新账号";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息 加互斥锁保证线程安全
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登录成功后，向redis订阅channel（id）
            _redis.subscribe(id);

            // 登录成功，向客户端返回登录成功的响应信息，更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0; // 0表示没有错误
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询该用户是否有离线信息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlineMessage"] = vec;
                // 读取该用户的离线信息后，把该用户的离线信息删除掉
                _offlineMsgModel.remove(id);
            }
            // 查询用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec1;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec1.push_back(js.dump());
                }
                response["friends"] = vec1;
            }

            // 查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (Groupuser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    groupjs["users"] = userV;
                    groupV.push_back(groupjs.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 登录失败，向客户端返回登录失败的响应信息
        // 用户不存在
        if (user.getId() == -1)
        {
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 3; // 3表示用户不存在
            response["errmsg"] = "用户不存在";
            conn->send(response.dump());
        }
        // 用户存在，用户名或密码错误
        else
        {
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 1; // 1表示有错误
            response["errmsg"] = "用户名或密码错误";
            conn->send(response.dump());
        }
    }
}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功，向客户端返回注册成功的响应信息
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0; // 0表示没有错误
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败，向客户端返回注册失败的响应信息
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1; // 1表示有错误
        conn->send(response.dump());
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    // lock_guard<mutex> lock(_connMutex);
    // for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
    // {
    //     if (it->second == conn)
    //     {
    //         // 更新用户状态信息
    //         User user = _userModel.query(it->first);
    //         user.setState("offline");
    //         _userModel.updateState(user);
    //         // 从map表中删除用户的连接信息
    //         _userConnMap.erase(it);
    //         break;
    //     }
    // }

    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                // 从map表中删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 用户下线，在redis中取消订阅
    _redis.unsubscribe(user.getId());

    // 更新用户状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"];

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线，转发消息  服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线,如果在线则在另一台服务器上
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);

        if (it != _userConnMap.end())
        {
            // 从map表中删除用户的连接信息
            _userConnMap.erase(it);
        }
    }
    // 下线后，在redis中取消订阅
    _redis.unsubscribe(userid);

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}
// 服务器异常后，业务重置
void ChatService::reset()
{
    // 把online用户，设置成offline
    _userModel.resetState();
}

// 添加好友业务 msgid  id  friend
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    int friendid = js["friendid"];

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}

// 创建群组业务
void ChatService::creatGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];

    Group group;
    group.setName(groupname);
    group.setDesc(groupdesc);

    bool state = _groupModel.creatGroup(group);
    if (state)
    {
        // 创建成功，存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}
// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int groupid = js["groupid"];
    int userid = js["userid"];

    // 添加群组信息
    _groupModel.addGroup(userid, groupid, "normal");
}
// 群聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["userid"];
    int groupid = js["groupid"];

    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (auto id : useridVec)
    {

        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询id是否在线,如果在线则在另一台服务器上
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 从redis中获取订阅信息业务
void ChatService::handleRedisSubMsg(int id, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(id);
    if(it!= _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    //存储用户离线信息
    _offlineMsgModel.insert(id,msg);

}