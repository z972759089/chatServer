#include "friendmodel.hpp"
#include "db.h"

// 添加用户的朋友信息
void friendModel::insert(int userid, int friendid)
{
     char sql[1024] = {0};
    sprintf(sql, "insert into friend values('%d','%d')", userid, friendid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }

}

// 返回用户好友列表
vector<User> friendModel::query(int userid)
{
     //  组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select u.id,u.name,u.state from user u inner join friend f on f.friendid = u.id  where f.userid = %d", userid);

    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有friend信息放入vec
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;

}