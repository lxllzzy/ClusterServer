#ifndef DB_H
#define DB_H
#include <mysql/mysql.h>
#include <string>
#include "muduo/base/Logging.h"
using namespace std;
using namespace muduo;

// 数据库配置信息
static string server = "127.0.0.1";
static string user = "root";
static string password = "123456";
static string dbname = "chat";
// 数据库操作类
class MySQL
{
public:
    MySQL();
    ~MySQL();
    
    bool connect();

    bool update(string sql);
    
    MYSQL_RES *query(string sql);

    MYSQL *getConnection();
private:
    MYSQL *_conn;
};

#endif