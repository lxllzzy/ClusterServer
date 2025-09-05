#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"
#include <string>
#include <iostream>
#include <functional>
using namespace std;
using namespace placeholders;
using json = nlohmann::json; 

ChatServer::ChatServer(EventLoop* loop, const InetAddress& listenAddr, const string &nameArg)
:_server(loop, listenAddr, nameArg)
,_loop(loop) {
    _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));
    _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));
    _server.setThreadNum(4);
}

void ChatServer::start() {
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn) {
    if (!conn->connected()) {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
    // if (conn->connected()) {
    //         cout << conn->peerAddress().toIpPort() << " -> " <<
    //             conn->localAddress().toIpPort() << " state:onlie" << endl;
    //     }
    //     else {
    //         cout << conn->peerAddress().toIpPort() << " -> " <<
    //             conn->localAddress().toIpPort() << " state:onlie" << endl; 
    //         conn->shutdown();
    //         // _loop->quit();
    //     }
}

void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
    string buffer = buf->retrieveAllAsString();
        // cout << "recv data :" << buffer << " time:" << time.toString() << endl;
        // conn->send(buffer);
    json js = json::parse(buffer);
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn, js, time);

}

