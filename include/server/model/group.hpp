#ifndef   GROUP_H
#define   GROUP_H
#include<string>
#include"groupuser.hpp"
#include<vector>
using namespace std;


//匹配group表的ORM类
class Group
{
public:
  
    Group(int id = -1, string name = "", string desc = "")
    {
        this->id = id;
        this->name = name;
        this->desc =desc;
    }

    int getId() { return this->id; };
    string getName() { return this->name; };
    string getDesc() { return this->desc; };
    vector<Groupuser> &getUsers(){ return this->users;};
  

    void setId(int id){ this->id = id; };
    void setName(string name){ this->name = name; };
    void setDesc(string desc){ this->desc = desc; };
    


private:
    int id;
    string name;
    string desc;
    vector<Groupuser> users; 
  
};

#endif