#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
using namespace muduo;

ChatService* ChatService::instance() {
    static ChatService service;
    return &service;
}

ChatService::ChatService() {
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this,_1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this,_1, _2, _3)});

    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this,_1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this,_1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this,_1, _2, _3)});

    if (_redis.connect()) {
        _redis.init_notify_handler(std::bind(&ChatService::handlerRedisSubscribeMessage, this,_1, _2));
    }
}

MesHandler ChatService::getHandler(int msgid) {
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time) {
            LOG_ERROR << "msgid: " << msgid << " can not find handler!" ;
        };
    }
    else {
        return _msgHandlerMap[msgid];
    }
}

void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time) {
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end()) {
            _userConnMap.erase(it);
        }
    }
    _redis.unsubscribe(userid);

    User user(userid, "", "", "offline");
    _usermodel.updateState(user);
}


void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time) {
    // LOG_INFO << "do login service";
    int id = js["id"].get<int>();
    string password = js["password"];
    User user = _usermodel.query(id);
    if (user.getId() == id && user.getPassword() == password) {
        if (user.getState() == "online") {
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else {
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            
            _redis.subscribe(id);

            user.setState("online");
            _usermodel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty()) {
                response["offlinemsg"] = vec;
                _offlineMsgModel.remove(id);
            }
            vector<User> userVec = _friendmodel.query(id);
            if (!userVec.empty()) {
                vector<string> vec2;
                for (User &user : userVec) {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }                
                response["friends"] = vec2;
            }

            vector<Group> groupuserVec = _groupmodel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }
            conn->send(response.dump()); 
        }
        
    }
    else {
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        conn->send(response.dump());
    }
}

void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time){
    // LOG_INFO << "do reg service";
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    bool state = _usermodel.insert(user);
    if (state) {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

void ChatService::clientCloseException(const TcpConnectionPtr &conn) {
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it) {
            if (it->second == conn) {
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());

     if (user.getId() != -1) {
         user.setState("offline");
         _usermodel.updateState(user);
     }
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time) {
    int toid = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end()) {
            it->second->send(js.dump());
            return;
        }
    }

    User user = _usermodel.query(toid);
    if (user.getState() == "online") {
        _redis.publish(toid, js.dump());
        return;
    }

    _offlineMsgModel.insert(toid, js.dump());
}

void ChatService::reset() {
    _usermodel.resetState();
}

void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time) {
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendmodel.insert(userid, friendid);
}

void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time) {
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    Group group(-1, name, desc);
    if (_groupmodel.createGroup(group)) {
        _groupmodel.addGroup(userid, group.getId(), "creator");
    }
}

void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time) {
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupmodel.addGroup(userid, groupid, "normal");
}

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time) {
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupmodel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec) {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end()) {
            it->second->send(js.dump());
        }
        else {
            User user = _usermodel.query(id);
            if (user.getState() == "online") {
                _redis.publish(id, js.dump());
                return;
            }
            else {
                _offlineMsgModel.insert(id, js.dump());     
            }
        }
    }
}

void ChatService::handlerRedisSubscribeMessage(int userid,string msg) {
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    _offlineMsgModel.insert(userid, msg);
}

