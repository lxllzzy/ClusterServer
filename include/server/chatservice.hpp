#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "usermodel.hpp"
#include "offlinemsgmodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
#include "json.hpp"
using json = nlohmann::json; 

using MesHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>;

class ChatService {
public:
    static ChatService* instance();
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    MesHandler getHandler(int msgid); 
    void clientCloseException(const TcpConnectionPtr &conn);
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void handlerRedisSubscribeMessage(int, string);
    
    void reset();


private:
    ChatService();
    unordered_map<int, MesHandler> _msgHandlerMap;
    UserModel _usermodel;
    OfflineMsgModel _offlineMsgModel;
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    mutex _connMutex;
    FriendModel _friendmodel;
    GroupModel _groupmodel;
    Redis _redis;
};

#endif