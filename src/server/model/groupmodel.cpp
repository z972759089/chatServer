#include "groupmodel.hpp"
#include "db.h"

// 创建群组
bool GroupModel::creatGroup(Group &group)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname,groupdesc) values('%s','%s')", group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 加入群组
void GroupModel::addGroup(int userid, int groupid, string role)
{
    // 1. 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into groupUser values(%d,%d,'%s')", groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    //  组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from allgroup a inner join groupUser b on a.id = b.groupid where b.userid = %d", userid);

    vector<Group> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid所在群信息放入vec
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                vec.push_back(group);
            }
            mysql_free_result(res);
            
            // 现在查询每个群组的用户信息
            for (Group &group : vec)
            {
                char userSql[1024] = {0};
                sprintf(userSql, "select b.userid, b.grouprole, u.name, u.state from groupUser b inner join user u on b.userid = u.id where b.groupid = %d", group.getId());
                MYSQL_RES *userRes = mysql.query(userSql);
                if (userRes != nullptr)
                {
                    MYSQL_ROW userRow;
                    while ((userRow = mysql_fetch_row(userRes)) != nullptr)
                    {
                        Groupuser groupuser;
                        groupuser.setId(atoi(userRow[0]));
                        groupuser.setRole(userRow[1]);
                        groupuser.setName(userRow[2]);
                        groupuser.setState(userRow[3]);
                        group.getUsers().push_back(groupuser);
                    }
                    mysql_free_result(userRes);
                }
            }
            return vec;
        }
    }
    return vec;
}

// 根据指定的groupid查询群组用户id列表，除了userid自己，主要用户群聊业务给群组其他成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    //  组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupUser where groupid = %d and userid<>%d", groupid, userid);

    vector<int> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把useid所在群信息放入vec
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {

                vec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        
        }
    }
    return vec;
}