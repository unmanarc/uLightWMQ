#include "dbcollection.h"

#include "../globals.h"
#include <boost/algorithm/string/predicate.hpp>
#include <dirent.h>
#include <thread>

std::map<std::string, MessageDB *> DBCollection::rcpts;
std::mutex DBCollection::m;
using namespace Mantids::Application;

bool DBCollection::start()
{
    std::thread(messagesGC());

    std::string dirPath = Globals::getLC_Database_FilesPath();
    if (!access(dirPath.c_str(),R_OK))
    {
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir (dirPath.c_str())) != NULL)
        {
            std::set<std::string> files;
            while ((ent = readdir (dir)) != NULL)
            {
                if ((ent->d_type & DT_REG) != 0)
                    files.insert(ent->d_name);
            }
            closedir (dir);

            for (const std::string & file: files)
            {
                std::string queueDbName;
                if (boost::starts_with(file,"msgs_") && boost::ends_with(file,".db"))
                {
                    queueDbName = file.substr(5);
                    queueDbName = queueDbName.substr(0,queueDbName.size()-3);

                    Globals::getAppLog()->log0(__func__,Logs::LEVEL_INFO, "Loading Queue Database '%s'", queueDbName.c_str());
                    getOrCreateMessageDB(queueDbName);
                }
            }
        }
        else
            Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "Failed to list queues directory: %s, no queues loaded.", dirPath.c_str());
    }
    else
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_CRITICAL, "Missing queues directory: %s", dirPath.c_str());
        return false;
    }

    return true;

}

void DBCollection::messagesGC()
{
    for ( ;; )
    {
        {
            std::unique_lock<std::mutex> lock(m);
            for  ( const auto &rcpt : rcpts )
            {
                rcpt.second->cleanExpired();
            }
        }
        sleep( Config_LocalIni::getLC_Database_ExpiredGCCleanUpEvery() );
    }
}

MessageDB *DBCollection::getMessageDB(const std::string &rcpt)
{
    std::unique_lock<std::mutex> lock(m);
    return (rcpts.find(rcpt) != rcpts.end())? rcpts[rcpt] : nullptr;
}

MessageDB *DBCollection::getOrCreateMessageDB(const std::string &rcpt)
{
    std::unique_lock<std::mutex> lock(m);
    if (rcpts.find(rcpt) != rcpts.end())
        return rcpts[rcpt];
    else
    {
        auto msgDB = new MessageDB;
        if (msgDB->start( rcpt ))
        {
            rcpts[rcpt] = msgDB;
            return msgDB;
        }
        else
        {
            return nullptr;
        }

    }

}
