#include "json.hpp"
#include <iostream>
#include <thread>
#include <ctime>
#include <vector>
#include <chrono>
#include <limits>  // 用于清理错误输入流
using namespace std;
using json = nlohmann::json;
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登陆的用户信息
User g_currentUser;
// 记录当前登录用户的好友信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 控制主聊天页面
bool isMainMenuRunning = false;

// 显示当前登录成功的用户信息
void showCurrentUserData();
// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接受线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example:./Chatclient 192.168.150.128 6100" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket creat error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接受用户输入，负责发送数据
    while (1)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "==========================" << endl;
        cout << "1.login" << endl;
        cout << "2.register" << endl;
        cout << "3.quit" << endl;
        cout << "==========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        if (cin.fail())
        {
            // 非数字输入（例如字母）会导致cin进入失败状态，必须清理流并重新显示菜单
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "invalid input, please enter a number (1/2/3)" << endl;
            continue;
        }
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login 业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get();
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }
            else
            {
                char buffer[4096] = {0}; // 增加buffer大小以处理更大的JSON响应
                len = recv(clientfd, buffer, 4096, 0);
                if (-1 == len)
                {
                    cerr << "recv login response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"]) // 登录失败
                    {
                        cerr << responsejs["errmsg"] << endl;
                    }
                    else // 登录成功
                    {
                        // 记录当前用户的id和name
                        g_currentUser.setId(responsejs["id"]);
                        g_currentUser.setName(responsejs["name"]);

                        // 记录当前用户的好友列表信息
                        if (responsejs.contains("friends"))
                        {
                            g_currentUserFriendList.clear(); // 初始化
                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"]);
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        // 记录当前用户的群组列表
                        if (responsejs.contains("groups"))
                        {
                            g_currentUserGroupList.clear(); // 初始化
                            vector<string> vec1 = responsejs["groups"];
                            for (string &groupstr : vec1)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"]);
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);
                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    Groupuser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"]);
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }
                        // 展示登录用户的基本信息
                        showCurrentUserData();

                        // 显示当前用户的离线信息  个人聊天信息或群组消息
                        if (responsejs.contains("offlineMessage"))
                        {
                            vector<string> vec = responsejs["offlineMessage"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                if (ONE_CHAT_MSG == js["msgid"])
                                {
                                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>() << " said:" << js["msg"].get<string>() << endl;
                                }
                                else
                                {
                                    cout << "群消息" << " [group " << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["userid"] << "]" << js["name"].get<string>() << " said:" << js["msg"].get<string>() << endl;
                                }
                            }
                        }

                        // 登录成功，启动接收线程负责接收数据，该线程只启动一次
                        static int readthread = 0;
                        if (readthread == 0)
                        {
                            std::thread readTask(readTaskHandler, clientfd); // pthread_creat
                            readTask.detach();                               // pthread_detach
                            readthread++;
                        }

                        isMainMenuRunning = true;
                        // 进入聊天主菜单界面
                        mainMenu(clientfd);
                    }
                }
            }
        }

        break;
        case 2: // register 业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv reg response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"]) // 注册失败
                    {
                        cerr << name << " is already exit ,register error!" << endl;
                    }
                    else // 注册成功
                    {
                        cout << name << " register success, userid is " << responsejs["id"] << ",do not forget it!" << endl;
                    }
                }
            }

            break;
        }

        case 3: // quit 业务
            close(clientfd);
            exit(0);
            break;

        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }
    return 0;
}

// help command handler
void help(int clientfd = 0, string command = "");
// chat command handler
void chat(int clientfd, string command);
// addfriend command handler
void addfriend(int clientfd, string command);
// creategroup command handler
void creategroup(int clientfd, string command);
// addgroup command handler
void addgroup(int clientfd, string command);
// groupchat command handler
void groupchat(int clientfd, string command);
// loginout command handler
void loginout(int clientfd, string command);

// 系统支持的客户端命令处理列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群组聊天,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}

};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid command!" << endl;
            continue;
        }
        else
        {
            // 调用命令处理函数，处理用户输入的命令
            it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
        }
    }
}

void help(int clientfd, string command)
{
    cout << "show command list:" << endl;
    for (auto &command : commandMap)
    {
        cout << command.first << " : " << command.second << endl;
    }
    cout << endl;
}

void addfriend(int clientfd, string command)
{
    int friendid = atoi(command.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send addfriend msg error:" << request << endl;
    }
}

void chat(int clientfd, string command)
{
    int idx = command.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "invalid command format!" << endl;
        return;
    }
    int friendid = atoi(command.substr(0, idx).c_str());
    string message = command.substr(idx + 1, command.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["to"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send chat msg error:" << request << endl;
    }
}

void groupchat(int clientfd, string command)
{
    int idx = command.find(":"); // groupid:message
    if (-1 == idx)
    {
        cerr << "invalid command format!" << endl;
        return;
    }
    int groupid = atoi(command.substr(0, idx).c_str());
    string message = command.substr(idx + 1, command.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["userid"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send groupchat msg error:" << request << endl;
    }
}

void creategroup(int clientfd, string command)
{
    int idx = command.find(":");
    if (-1 == idx)
    {
        cerr << "invalid command format!" << endl;
        return;
    }
    string groupname = command.substr(0, idx);
    string groupdesc = command.substr(idx + 1, command.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send creategroup msg error:" << request << endl;
    }
}

void addgroup(int clientfd, string command)
{
    int groupid = atoi(command.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["userid"] = g_currentUser.getId();
    js["groupid"] = groupid;

    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send addgroup msg error:" << request << endl;
    }
}

void loginout(int clientfd, string command)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send loginout msg error:" << request << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

// 显示当前登录成功的用户信息
void showCurrentUserData()
{
    cout << "===================================login user=======================================" << endl;
    cout << "current login user=》id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------------------------friend list---------------------------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------------------group list----------------------------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (Groupuser &groupuser : group.getUsers())
            {
                cout << "    " << groupuser.getId() << " " << groupuser.getName() << " " << groupuser.getState() << " " << groupuser.getRole() << endl;
            }
        }
    }
    cout << "====================================================================================" << endl;
}

// 接收线程
void readTaskHandler(int clientfd)
{
    while (1)
    {
        char buffer[4096] = {0}; // 增加buffer大小
        int len = recv(clientfd, buffer, 4096, 0);
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }
        // 接受ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        if (ONE_CHAT_MSG == js["msgid"])
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>() << " said:" << js["msg"].get<string>() << endl;
            continue;
        }
        else if (GROUP_CHAT_MSG == js["msgid"])
        {
            cout << "群消息" << " [group " << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["userid"] << "]" << js["name"].get<string>() << " said:" << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

// 获取系统时间
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min);
    return string(date);
}