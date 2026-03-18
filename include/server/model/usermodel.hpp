#ifndef   USERMODEL_HPP
#define   USERMODEL_HPP
#include  "user.hpp"

//User表的数据操作类
class UserModel
{
private:
    /* data */
public:
    //user表的插入方法
    bool insert(User &user);   
    //根据用户id查询用户信息
    User query(int id);
    //user表更新状态
    bool updateState(User &user);
    //重置用户状态信息
    void resetState();


};


#endif