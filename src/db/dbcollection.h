#ifndef DBCOLLECTION_H
#define DBCOLLECTION_H

#include <mutex>
#include "messagedb.h"

class DBCollection
{
public:
    static bool start();
    static void messagesGC();

    static MessageDB * getMessageDB(const std::string & rcpt);
    static MessageDB * getOrCreateMessageDB(const std::string & rcpt);

private:
    static std::map<std::string, MessageDB *> rcpts;
    static std::mutex m;
};

#endif // DBCOLLECTION_H
