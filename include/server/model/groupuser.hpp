#ifndef   GROUPUSER_H
#define   GROUPUSER_H
#include"user.hpp"


//匹配groupuser表的ORM类
class Groupuser:public User
{
public:
    void setRole(string role){this->role = role;}
    string getRole(){ return this->role;}


private:
    string role;

  
};

#endif